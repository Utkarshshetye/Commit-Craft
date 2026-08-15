#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <pthread.h>
#include <openssl/sha.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <dirent.h>

#define PORT 7777
#define BUFFER_SIZE 1048576
#define READ_BUF_SIZE 65536
#define HASH_SIZE 40
#define BRANCH_FILE ".git/refs/branches"
#define LAST_COMMIT_FILE_TEMPLATE ".git/refs/branches/%s"
#ifndef MAX_THREADS
#define MAX_THREADS 8
#endif
#define MAX_CLIENTS 128

typedef struct ClientNode {
    int client_fd;
    struct ClientNode *next;
} ClientNode;

ClientNode *queue_front = NULL, *queue_rear = NULL;
pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

void enqueue_client(int client_fd) {
    ClientNode *new_node = (ClientNode *)malloc(sizeof(ClientNode));
    if (!new_node) {
        perror("Memory allocation failed");
        close(client_fd);
        return;
    }
    new_node->client_fd = client_fd;
    new_node->next = NULL;

    pthread_mutex_lock(&queue_lock);
    if (queue_rear == NULL) {
        queue_front = new_node;
        queue_rear = new_node;
    } else {
        queue_rear->next = new_node;
        queue_rear = new_node;
    }
    pthread_cond_signal(&queue_cond);
    pthread_mutex_unlock(&queue_lock);
}

int dequeue_client() {
    pthread_mutex_lock(&queue_lock);
    while (queue_front == NULL) {
        pthread_cond_wait(&queue_cond, &queue_lock);
    }
    ClientNode *temp = queue_front;
    int client_fd = temp->client_fd;
    queue_front = queue_front->next;
    if (queue_front == NULL) {
        queue_rear = NULL;
    }
    free(temp);
    pthread_mutex_unlock(&queue_lock);
    return client_fd;
}

void compute_sha1(const char *data, size_t size, char *hash_out) {
    FILE *file = fopen(data, "rb");
    if (!file) {
        perror("Error opening file for SHA1 computation");
        return;
    }

    SHA_CTX sha_ctx;
    SHA1_Init(&sha_ctx);

    char *buffer = malloc(READ_BUF_SIZE);
    if (!buffer) {
        perror("Failed to allocate SHA1 buffer");
        fclose(file);
        return;
    }
    size_t bytes_read;
    size_t total_hashed = 0;
    
    while ((bytes_read = fread(buffer, 1, READ_BUF_SIZE, file)) > 0) {
        SHA1_Update(&sha_ctx, buffer, bytes_read);
        total_hashed += bytes_read;
        if (total_hashed % (100 * 1024 * 1024) < READ_BUF_SIZE) {
            printf("Main_Server: Hashed %zu MB...\n", total_hashed / (1024 * 1024));
            fflush(stdout);
        }
    }
    fclose(file);
    free(buffer);

    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1_Final(hash, &sha_ctx);

    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        sprintf(hash_out + (i * 2), "%02x", hash[i]);
    }
    hash_out[HASH_SIZE] = '\0';
    printf("Main_Server: SHA1 complete: %s\n", hash_out);
    fflush(stdout);
}

void write_object(const char *hash, const char *content, size_t size) {
    char path[256];
    snprintf(path, sizeof(path), ".git/objects/%.2s", hash);
    mkdir(path, 0755);

    snprintf(path, sizeof(path), ".git/objects/%.2s/%.38s", hash, hash + 2);
    FILE *file = fopen(path, "wb");
    if (!file) {
        perror("Error writing object");
        exit(EXIT_FAILURE);
    }
    fwrite(content, 1, size, file);
    fclose(file);
}

void write_branch_head(const char *branch, const char *hash) {
    char path[256];
    snprintf(path, sizeof(path), LAST_COMMIT_FILE_TEMPLATE, branch);
    FILE *file = fopen(path, "w");
    if (!file) {
        perror("Error writing branch head");
        exit(EXIT_FAILURE);
    }
    fprintf(file, "%s", hash);
    fclose(file);
}

void read_branch_head(const char *branch, char *hash) {
    char path[256];
    snprintf(path, sizeof(path), LAST_COMMIT_FILE_TEMPLATE, branch);
    FILE *file = fopen(path, "r");
    if (file) {
        fgets(hash, HASH_SIZE + 1, file);
        fclose(file);
    } else {
        memset(hash, 0, HASH_SIZE + 1);
    }
}

void handle_pull(int client_fd, const char *branch) {
    char last_commit[HASH_SIZE + 1];
    read_branch_head(branch, last_commit);

    if (strlen(last_commit) == 0) {
        send(client_fd, "ERROR: No commits available on this branch\n", 44, 0);
        return;
    }

    char path[256];
    snprintf(path, sizeof(path), ".git/objects/%.2s/%.38s", last_commit, last_commit + 2);
	printf("%s\n",path);
    FILE *file = fopen(path, "rb");
    if (!file) {
        send(client_fd, "ERROR: Commit object missing\n", 29, 0);
        return;
    }

    char *content = malloc(READ_BUF_SIZE);
    if (!content) { fclose(file); return; }
    size_t bytes_read;
    while ((bytes_read = fread(content, 1, READ_BUF_SIZE, file)) > 0) {
        ssize_t bytes_sent = send(client_fd, content, bytes_read, 0);
        if (bytes_sent < 0) {
            perror("Error sending data to client");
            fclose(file);
            free(content);
            return;
        }
    }

    fclose(file);
    free(content);
}

void handle_push(int client_fd, const char *branch, const char *file_name, size_t file_size, char *initial_data, size_t initial_len) {
    char hash[HASH_SIZE + 1];
    char *buffer = malloc(BUFFER_SIZE);
    if (!buffer) {
        perror("Failed to allocate buffer in handle_push");
        return;
    }
	size_t total_received = 0;
    
    // Create a temporary file to store the received content
    char temp_path[] = ".git/temp_object_XXXXXX";
    int temp_fd = mkstemp(temp_path);

    if (temp_fd == -1) {
        perror("Error creating temporary file");
        send(client_fd, "ERROR: Could not create temporary file\n", 39, 0);
        free(buffer);
        return;
    }

    printf("Main_Server: Expecting %lu bytes for file %s. Initial data: %lu bytes\n", file_size, file_name, initial_len);
    fflush(stdout);
    if (initial_len > 0) {
        if (write(temp_fd, initial_data, initial_len) != (ssize_t)initial_len) {
            perror("Error writing initial data to temporary file");
            close(temp_fd);
            unlink(temp_path);
            send(client_fd, "ERROR: Could not save file\n", 27, 0);
            free(buffer);
            return;
        }
        total_received += initial_len;
    }

    size_t last_print = 0;
    while (total_received < file_size) {
        ssize_t bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            perror("Error receiving file data");
            printf("Main_Server: Connection lost after %lu / %lu bytes\n", total_received, file_size);
            fflush(stdout);
            close(temp_fd);
            unlink(temp_path);
            free(buffer);
            return;
        }

        if (write(temp_fd, buffer, bytes_received) != bytes_received) {
            perror("Error writing to temporary file");
            close(temp_fd);
            unlink(temp_path);
            send(client_fd, "ERROR: Could not save file\n", 27, 0);
            free(buffer);
            return;
        }
        total_received += bytes_received;
        if (total_received - last_print > 100000000) {
            printf("Main_Server: Received %lu / %lu bytes (%.0f%%)\n", total_received, file_size, (double)total_received / file_size * 100);
            fflush(stdout);
            last_print = total_received;
        }
    }

    // Close the temporary file
    close(temp_fd);
    printf("Main_Server: Finished receiving %lu bytes. Starting SHA-1 hash...\n", total_received);
    fflush(stdout);

    // Compute SHA1 hash for the received file
    compute_sha1(temp_path, file_size, hash);

    // Create object path using the hash
    char object_dir[256];
    snprintf(object_dir, sizeof(object_dir), ".git/objects/%.2s", hash);
    mkdir(object_dir, 0755);

    char object_path[256];
    snprintf(object_path, sizeof(object_path), ".git/objects/%.2s/%.38s", hash, hash + 2);

    // Move the temporary file to the object path
    if (rename(temp_path, object_path) != 0) {
        perror("Error moving temporary file to object directory");
        unlink(temp_path); // Remove temporary file
        send(client_fd, "ERROR: Could not save object file\n", 34, 0);
        free(buffer);
        return;
    }

    // Update the branch head with the new commit hash
    write_branch_head(branch, hash);

    // Send success response
    char response[128];
    snprintf(response, sizeof(response), "SUCCESS: File saved as %s, commit hash: %s\n", file_name, hash);
    send(client_fd, response, strlen(response), 0);
    free(buffer);
}


void handle_history(int client_fd, const char *branch) {
    char hash[HASH_SIZE + 1];
    read_branch_head(branch, hash);

    if (strlen(hash) == 0) {
        send(client_fd, "ERROR: No commits available on this branch\n", 44, 0);
        return;
    }

    char *response = malloc(8192);
    if (!response) return;
    strcpy(response, "Commit history:\n");
    while (strlen(hash) > 0) {
        strcat(response, hash);
        strcat(response, "\n");

        char path[256];
        snprintf(path, sizeof(path), ".git/objects/%.2s/%.38s", hash, hash + 2);

        FILE *file = fopen(path, "rb");
        if (!file) {
            break;
        }

        fread(hash, 1, HASH_SIZE, file);
        hash[HASH_SIZE] = '\0';
        fclose(file);
    }

    send(client_fd, response, strlen(response), 0);
    free(response);
}

void handle_client(int client_fd) {
    char *buffer = malloc(BUFFER_SIZE);
    if (!buffer) {
        perror("Failed to allocate buffer");
        close(client_fd);
        return;
    }
    ssize_t received = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
    if (received <= 0) {
        perror("Error receiving data");
        free(buffer);
        close(client_fd);
        return;
    }

    buffer[received] = '\0';

	char command[16], branch[32], file_name[128];
    size_t file_size = 0;

    // Parse command, branch, file name, and file size
    sscanf(buffer, "%s %s %lu %s", command, branch, &file_size, file_name);
	
	if (strcmp(command,"PUSH") == 0) {
        char *header_end = strchr(buffer, '\n');
        size_t header_len = 0;
        char *initial_data = NULL;
        size_t initial_len = 0;
        
        if (header_end) {
            header_len = (header_end - buffer) + 1;
            initial_data = header_end + 1;
            initial_len = received - header_len;
        }

        handle_push(client_fd, branch, file_name, file_size, initial_data, initial_len);
	}
    else if (strcmp(command, "PULL") == 0) {
        handle_pull(client_fd, branch);
    } else if (strcmp(command, "HISTORY") == 0) {
        handle_history(client_fd, branch);
    } else {
        send(client_fd, "ERROR: Invalid command\n", 23, 0);
    }

    free(buffer);
    close(client_fd);
}

void *worker_thread(void *arg) {
    while (1) {
        int client_fd = dequeue_client();
        handle_client(client_fd);
    }
    return NULL;
}

int create_directory_if_not_exists(const char *dir) {
    struct stat st = {0};

    if (stat(dir, &st) == -1) {  // Check if the directory exists
        if (mkdir(dir, 0755) == 0) {
            printf("Directory created: %s\n", dir);
            return 1;  // Successfully created
        } else {
            perror("mkdir failed");
            return -1;  // Error occurred
        }
    } else {
        printf("Directory already exists: %s\n", dir);
        return 0;  // Directory already exists
    }
}



int main() {
    int server_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
	
	
	if (create_directory_if_not_exists(".git") == 1) {
        create_directory_if_not_exists(".git/objects");
		create_directory_if_not_exists(".git/refs");
		create_directory_if_not_exists(BRANCH_FILE);
    }

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) == -1) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    pthread_t threads[MAX_THREADS];
    for (int i = 0; i < MAX_THREADS; i++) {
        pthread_create(&threads[i], NULL, worker_thread, NULL);
    }

    printf("Server started...\n");

    while (1) {
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd == -1) {
            perror("Client accept failed");
            continue;
        }
        enqueue_client(client_fd);
    }

    close(server_fd);
    return 0;
}
