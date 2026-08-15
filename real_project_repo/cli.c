#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <openssl/sha.h>
#include <sys/stat.h>
#include <ctype.h>

#define PORT 7777
#define BUFFER_SIZE 8192
#define HASH_SIZE 40
#define BRANCH_FILE ".git/refs/heads"
#define LAST_COMMIT_FILE_TEMPLATE ".git/refs/heads/%s"

void compute_sha1(const char *data, size_t len, char *hash_out) {
       FILE *file = fopen(data, "rb");
    if (!file) {
        perror("Error opening file for SHA1 computation");
        exit(EXIT_FAILURE);
    }

    SHA_CTX sha_ctx;
    SHA1_Init(&sha_ctx);

    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        SHA1_Update(&sha_ctx, buffer, bytes_read);
    }
    fclose(file);

    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1_Final(hash, &sha_ctx);

    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        sprintf(hash_out + (i * 2), "%02x", hash[i]);
    }
    hash_out[HASH_SIZE] = '\0';
}

// Function to read local branch head
int read_branch_head(const char *branch, char *head_hash) {
    char path[BUFFER_SIZE];
    snprintf(path, sizeof(path), LAST_COMMIT_FILE_TEMPLATE, branch);

    FILE *file = fopen(path, "r");
    if (!file) {
        // No branch head exists (e.g., first commit)
        head_hash[0] = '\0';
        return 0;
    }

    if (fgets(head_hash, HASH_SIZE + 1, file) == NULL) {
        perror("Error reading branch head");
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}

// Function to write branch head
int write_branch_head(const char *branch, const char *head_hash) {
    char path[BUFFER_SIZE];
    snprintf(path, sizeof(path), LAST_COMMIT_FILE_TEMPLATE, branch);

    FILE *file = fopen(path, "w");
    if (!file) {
        perror("Error writing branch head");
        return -1;
    }

    fprintf(file, "%s\n", head_hash);
    fclose(file);
    return 0;
}

int reconnect_to_server(int *client_fd, struct sockaddr_in *server_addr) {
    close(*client_fd);

    printf("Attempting to reconnect to server...\n");
    if ((*client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed during reconnect");
        return -1;
    }

    if (connect(*client_fd, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0) {
        perror("Reconnection failed");
        return -1;
    }

    printf("Reconnected to server\n");
    return 0;
}

int send_command(int *client_fd, struct sockaddr_in *server_addr, const char *command, size_t command_len) {
    if (reconnect_to_server(client_fd, server_addr) == 0) {
        // Retry sending command after reconnection
        if (send(*client_fd, command, command_len, 0) < 0) {
            perror("Error sending command after reconnect");
            return -1;
        }
    } else {
        return -1; // Reconnection failed
    }
    return 0;
}

// Function to send full file data for PUSH
int send_full_file_data(int *client_fd, const char *file_path) {
    FILE *file = fopen(file_path, "rb");
    if (!file) {
        perror("Error opening file");
        return -1;
    }

    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (send(*client_fd, buffer, bytes_read, 0) < 0) {
            perror("Error sending file data");
            fclose(file);
            return -1;
        }
    }

    fclose(file);
    return 0;
}

// Function to receive full file data for PULL
int receive_full_file_data(int *client_fd, const char *file_path) {
    FILE *file = fopen(file_path, "wb");
    if (!file) {
        perror("Error opening file to write");
        return -1;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    while ((bytes_received = recv(*client_fd, buffer, sizeof(buffer), 0)) > 0) {
        fwrite(buffer, 1, bytes_received, file);
    }

    if (bytes_received < 0) {
        perror("Error receiving file data");
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}

void handle_pull(int *client_fd, struct sockaddr_in *server_addr, const char *branch, const char *file_path) {
    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "PULL %s", branch);

    if (send_command(client_fd, server_addr, command, strlen(command)) < 0) {
        return;
    }

    // Receive the file data
    printf("Receiving file data for branch: %s\n", branch);
    if (receive_full_file_data(client_fd, file_path) == 0) {
        printf("File successfully received for branch: %s\n", branch);
    }
}

void trim_whitespace(char *str) {
    char *end;

    // Trim leading whitespace
    while (isspace((unsigned char)*str)) str++;

    // If all spaces or empty string
    if (*str == 0) {
        *str = '\0';
        return;
    }

    // Trim trailing whitespace
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    // Write new null terminator
    *(end + 1) = '\0';
}

void handle_push(int *client_fd, struct sockaddr_in *server_addr, const char *branch, const char *file_path) {
    char local_head[HASH_SIZE];
	bzero(local_head,HASH_SIZE);
    
	if (read_branch_head(branch, local_head) < 0) {
        return;
    }
	
	trim_whitespace(local_head);
	
    // Get the file size
    struct stat st;
    if (stat(file_path, &st) != 0) {
        perror("Error getting file size");
        return;
    }
    long file_size = st.st_size;
	printf("File Size:%ld",file_size);
	
	if(strlen(local_head) == 0)
	{
	   // Compute SHA1 hash for the received file
		compute_sha1(file_path, file_size, local_head);
	}

    // Send branch head and file size along with the PUSH command
    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "PUSH %s %ld %s", branch, file_size, local_head);
	printf("%s\n",command);

    if (send_command(client_fd, server_addr, command, strlen(command)) < 0) {
        return;
    }

    // Send the full file data
    printf("Sending file data for branch: %s\n", branch);
    if (send_full_file_data(client_fd, file_path) < 0) {
        return;
    }

    // Receive response to check for merge conflict
    char response[BUFFER_SIZE];
    ssize_t received = recv(*client_fd, response, sizeof(response) - 1, 0);
    if (received > 0) {
        response[received] = '\0';
        printf("PUSH Response:\n%s\n", response);

        // If merge conflict exists, notify user and suggest pull
        if (strstr(response, "MERGE CONFLICT") != NULL) {
            printf("Merge conflict detected. Please pull the latest changes first.\n");
        } else {
            // Successfully pushed, update the branch head
            write_branch_head(branch, local_head);
        }
    } else {
        perror("Error receiving response for PUSH");
    }
}

void handle_history(int *client_fd, struct sockaddr_in *server_addr, const char *branch) {
    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "HISTORY %s", branch);

    if (send_command(client_fd, server_addr, command, strlen(command)) < 0) {
        return;
    }

    char response[BUFFER_SIZE];
    ssize_t received = recv(*client_fd, response, sizeof(response) - 1, 0);
    if (received > 0) {
        response[received] = '\0';
        printf("HISTORY Response:\n%s\n", response);
    } else {
        perror("Error receiving response for HISTORY");
    }
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
    int client_fd;
    struct sockaddr_in server_addr;
    
	
	// if (create_directory_if_not_exists(".git") == 1) {
    //     create_directory_if_not_exists(".git/branches");
    //     create_directory_if_not_exists(".git/ref/branches");
        
    // }

    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "10.129.148.248", &server_addr.sin_addr) <= 0) {
        perror("Invalid server address");
        return EXIT_FAILURE;
    }

    if (connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        return EXIT_FAILURE;
    }
    
    printf("Connected to server\n");

    while (1) {
        char command[BUFFER_SIZE], branch[BUFFER_SIZE], file_path[BUFFER_SIZE];

        printf("\nAvailable commands: PULL, PUSH, HISTORY, EXIT\n");
        printf("Enter command: ");
        fgets(command, sizeof(command), stdin);
        command[strcspn(command, "\n")] = '\0'; // Remove trailing newline

        if (strcmp(command, "EXIT") == 0) {
            printf("Exiting...\n");
            break;
        }

        if (strcmp(command, "PULL") == 0) {
            printf("Enter branch: ");
            fgets(branch, sizeof(branch), stdin);
            branch[strcspn(branch, "\n")] = '\0';

            printf("Enter file path to save the pull data: ");
            fgets(file_path, sizeof(file_path), stdin);
            file_path[strcspn(file_path, "\n")] = '\0';

            handle_pull(&client_fd, &server_addr, branch, file_path);
        } else if (strcmp(command, "PUSH") == 0) {
            printf("Enter branch: ");
            fgets(branch, sizeof(branch), stdin);
            branch[strcspn(branch, "\n")] = '\0';

            printf("Enter file path to push: ");
            fgets(file_path, sizeof(file_path), stdin);
            file_path[strcspn(file_path, "\n")] = '\0';

            handle_push(&client_fd, &server_addr, branch, file_path);
        } else if (strcmp(command, "HISTORY") == 0) {
            printf("Enter branch: ");
            fgets(branch, sizeof(branch), stdin);
            branch[strcspn(branch, "\n")] = '\0';
            handle_history(&client_fd, &server_addr, branch);
        } else {
            printf("Invalid command. Try again.\n");
        }
    }
    close(client_fd);

    return 0;
}