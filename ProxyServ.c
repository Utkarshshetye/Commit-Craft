#include "config.h"
#include "proxy_api.h"
#include <stdio.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/sendfile.h>
#include <git2.h>
#include <stdbool.h>
#include <hiredis/hiredis.h>
#include <semaphore.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <openssl/sha.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <assert.h>

enum commands {ADD,INIT};
sem_t sem;
char cwdbuffer[PATH_MAX];

redisContext *rc;
git_signature sign;
cpara para;

#define BRANCH_FILE ".git/refs/heads"
#define LAST_COMMIT_FILE_TEMPLATE ".git/refs/heads/%s"

double GetTime() {
    struct timeval t;
    int rc = gettimeofday(&t, NULL);
    assert(rc == 0);
    return (double) t.tv_sec + (double) t.tv_usec/1e6;
}

void set_values(redisContext * context){
    redisCommand(context, "HSET commands %s %d", "GIT_INIT", 0);
    redisCommand(context, "HSET commands %s %d", "GIT_ADD", 1);
    redisCommand(context, "HSET commands %s %d", "GIT_COMMIT", 2);
    redisCommand(context, "HSET commands %s %d", "GIT_PUSH", 3);
    redisCommand(context, "HSET commands %s %d", "GIT_PULL", 4);

    redisCommand(context, "HSET commands %s %d", "HISTORY", 5);
}

char* get_path(){
  
    getcwd(cwdbuffer,sizeof(cwdbuffer));

    return cwdbuffer;
}

int get_index(redisContext* context, char* inp) {

    redisReply* reply = redisCommand(context,"HGET commands %s",inp);

    if (reply == NULL) {
        fprintf(stderr, "Error executing command: %s\n", context->errstr);
        return -1;
    }

    if(reply == NULL || reply->type == REDIS_REPLY_NIL){
        return -1;
    }

    int value = atoi(reply->str);

    freeReplyObject(reply);

    return value;
}

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

void trim_whitespace(char *str) {
    char *end;

    // Trim leading whitespace
    while (isspace((unsigned char)*str)) str++;

    // If all spaces or empty string
    if (*str == 0) {
        *str = '\0';
        return;
    }

    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    *(end + 1) = '\0';
}

void signature_define(){

    // redisReply * reply1 = redisCommand(rc,"HGET sign name");
    // redisReply * reply2 = redisCommand(rc,"HGET sign email");

    // reply reply;
    /*
    if(reply1->str && reply2->str){
        // Email and Name is already provided
        reply.name = reply1->str;
        reply.email = reply2->str;
        return reply;
    }

    */
    
    printf("We are here");
    char email[64];
    char name[64];

    printf("git want to know more? Name: ");
    scanf("%s",name);

    int len = strlen(name);
    char * nm = malloc(len+1);
    memcpy(nm,name,strlen(name));

    printf("\n");
    printf("Email: ");
    scanf("%s",email);

    int len1 = strlen(email);
    char * nm1 = malloc(len1+1);
    memcpy(nm1,email,strlen(email));
    
    sign.email = nm1;
    sign.name = nm;

    // reply.email = email;
    // reply.name = name;
    
    // redisCommand(rc,"HSET sign name %s",name);

    // redisCommand(rc,"HSET sign email %s",email);

    return;
}

void git_add_at_client(){
    char *path = get_path();
    git_repository *repo = NULL;
    git_index *idx;

    int status = git_repository_open(&repo,path);

    if(status!=0){
        printf("This is not a git repository!!\n");
        exit(-1);
    }

    int idxstatus = git_repository_index(&idx,repo);

    if(idxstatus!=0){
        printf("Index Not Found!!\n");
        exit(-1);
    }

    git_index_add_all(idx,NULL,GIT_INDEX_ADD_CHECK_PATHSPEC,NULL,NULL);

    // git_index_write(idx);

    status = git_index_write(idx);
    if (status != 0) {
        printf("Failed to write index: %s\n", git_error_last()->message);
    }

    git_index_free(idx);
    git_repository_free(repo);
}

void git_commit_at_client(){

    char *path = get_path();
    char msg[100];
    size_t parent_count = 0;
    git_reference *ref = NULL;
    git_repository *repo = NULL;
    git_object *root = NULL;
    git_tree *tree = NULL;
    git_oid id;
    git_oid cmoid;
    git_index *idx = NULL;
    git_commit *parentcommit =  NULL;

    printf("Please enter the commit message for your changes: ");
    scanf("%s",msg);

    int status = git_repository_open(&repo,path);

    if(status!=0){
        printf("This is not a git repository!!\n");
        exit(-1);
    }

    int idxstatus = git_repository_index(&idx,repo);

    if(idxstatus!=0){
        printf("No index present!");
        exit(-1);
    }

    git_index_write_tree(&id,idx);

    git_tree_lookup(&tree,repo,&id);

    signature_define();
    
    free(sign.email);
    free(sign.name);
    
    if(tree){
        parent_count = 1;
    }

    git_commit_create_v(&cmoid,repo,"HEAD",&sign,&sign,NULL,msg,tree,parent_count);

    git_index_free(idx);
    git_object_free(root);
    git_reference_free(ref);
}


void git_init_at_client(){

    int count=0;
    char *path;
    struct stat *s;
    stat(".git",s);

    git_repository *repo = NULL;

    path = get_path();

    // printf("%s\n",path);

    git_repository_init(&repo,".",false);

    printf("Initialized existing Git repository in %s/.git\n",path);
}

int sockfd;
struct sockaddr_in server_addr;
// clientsockaddr sockaddr;

/*
void setup(){

    sockfd = socket(AF_INET,SOCK_STREAM,0);

    // sockaddr.sin_family = AF_INET;
    // sockaddr.sin_port = htons(SERVER_PORT);
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    inet_pton(AF_INET,SERVER_ADDR,&server_addr.sin_addr);

    // int status = connect(sockfd,(struct sockaddr *)&sockaddr,sizeof(sockaddr));

    // Setup-Code

    // printf("Status: %d\n",status);
    // scanf("%s",buffer);
    
    // if(status < 0){
    //     printf("Connection Failed!!\n");
    // }


    // printf("Message sent successfully!!!\n");

    // close(para.sockfd);
}
*/

int reconnect_to_server(int *client_fd, struct sockaddr_in *server_addr) {
    close(*client_fd);

    printf("Attempting to reconnect to server...\n");

    if ((*client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed during reconnect");
        return -1;
    }
    
    // printf("%s",inet_ntop(AF_INET,server_addr->sin_addr))
    // printf("Client fd: %d",*fd);

    // struct sockaddr_in servaddr;

    // servaddr.sin_family = AF_INET;
    // servaddr.sin_port = htons(SERVER_PORT);

    if (connect(*client_fd,(struct sockaddr *)server_addr, sizeof(*server_addr)) < 0) {
        perror("Reconnection failed");
        // printf("Get lost!!\n");
        fflush(stdout);
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
        printf("FD is: %d\n",*client_fd);
        return -1;
    }
    return 0;
}

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

void git_push_s(char *branch, char *filepath){
    // &client_fd, &server_addr, branch, file_path
    // setup();
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

    printf("sockfd: %d\n",sockfd);
    // printf("SADDR: %s\n", server_addr.sin_addr);
    printf("COMMAND: %s\n", command);

    if(send_command(&sockfd,&server_addr.sin_addr, command, strlen(command)) < 0) {
        return;
    }

    printf("Sending file data for branch: %s\n", branch);
    if (send_full_file_data(sockfd, file_path) < 0) {
        return;
    }
    
    // Receive response to check for merge conflict
    char response[BUFFER_SIZE];
    ssize_t received = recv(sockfd, response, sizeof(response) - 1, 0);
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
void handle_push(int *client_fd, struct sockaddr_in *server_addr, const char *branch, const char *file_path) {
    double t1 = GetTime(),t2;
    char local_head[HASH_SIZE];
	bzero(local_head,HASH_SIZE);
    
	if (read_branch_head(branch, local_head) < 0) {
        return;
    }
	
	trim_whitespace(local_head);

    struct stat st;
    if (stat(file_path, &st) != 0) {
        perror("Error getting file size");
        return;
    }
    long file_size = st.st_size;
	printf("File Size:%ld",file_size);
	
	//if(strlen(local_head) == 0)
	//{
	   // Compute SHA1 hash for the received file
		compute_sha1(file_path, file_size, local_head);
	//}

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

       t2 = GetTime();
    } else {
        perror("Error receiving response for PUSH");
    }

    printf("Service time: %f\n",t2-t1);
}
void git_push_server(){
//     printf("Enter branch: ");
//     fgets(branch, sizeof(branch), stdin);
//     branch[strcspn(branch, "\n")] = '\0';

//     printf("Enter file path to push: ");
//     fgets(file_path, sizeof(file_path), stdin);
//     file_path[strcspn(file_path, "\n")] = '\0';

//     git_push_s(branch,file_path);

    printf("Enter branch: ");
            fgets(branch, sizeof(branch), stdin);
            branch[strcspn(branch, "\n")] = '\0';

    printf("Enter file path to push: ");
            fgets(file_path, sizeof(file_path), stdin);
            file_path[strcspn(file_path, "\n")] = '\0';

    handle_push(&sockfd, &server_addr, branch, file_path);
     
}

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

void handle_pull(int *client_fd, struct sockaddr_in *server_addr, const char *branch, const char *file_path) {
    double t1 = GetTime(),t2;
  
    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "PULL %s", branch);

    if (send_command(client_fd, server_addr, command, strlen(command)) < 0) {
        return;
    }

    printf("Receiving file data for branch: %s\n", branch);
    if (receive_full_file_data(client_fd, file_path) == 0) {
        printf("File successfully received for branch: %s\n", branch);
    }

    t2= GetTime();

    printf("Service time: %f\n",t2-t1);
}

void git_pull_at_client(){
    printf("Enter branch: ");
    fgets(branch, sizeof(branch), stdin);
    branch[strcspn(branch, "\n")] = '\0';

    printf("Enter file path to save the pull data: ");
    fgets(file_path, sizeof(file_path), stdin);
    file_path[strcspn(file_path, "\n")] = '\0';

    handle_pull(&sockfd, &server_addr, branch, file_path);
}

void git_history(){
    printf("Enter branch: ");
    fgets(branch, sizeof(branch), stdin);
    branch[strcspn(branch, "\n")] = '\0';
    handle_history(&sockfd, &server_addr, branch);
}

// Use the Remote Repository to fulfil the request
void hadling_remote_request(char * cmd){
    if(strcmp(cmd,"GIT_PUSH")==0){
        git_push_server();
    }
    else if(strcmp(cmd,"GIT_PULL")==0){
        git_pull_at_client();

    }else if(strcmp(cmd,"HISTORY")==0){
        git_history();
    }
}

void handling_local_request(char * cmd){
    // printf("We are here:\n");
    if(strcmp(cmd,"GIT_INIT")==0){
        git_init_at_client();

    }else if(strcmp(cmd,"GIT_ADD")==0){
        git_add_at_client();

    }else if(strcmp(cmd,"GIT_COMMIT")==0){
        git_commit_at_client();
    }
}

void handle_request(char * inpcmd){
    // printf("%s",inpcmd);
   
    char *cmd = strtok(inpcmd, "\n");
    int op = get_index(rc, cmd);

    switch (op){
        case 0: 
        case 1:
        case 2:
            handling_local_request(cmd);
            break;
        case 3:
            hadling_remote_request(cmd);
            break;
        case 4:
            hadling_remote_request(cmd);
            break;
        case 5:
            hadling_remote_request(cmd);
            break;
        default:
            hadling_remote_request(cmd);
            break;
    }
}

void get_values(redisContext *context){
    redisReply *reply = (redisReply *)redisCommand(context, "HGETALL commands");

    if(reply==NULL){
        printf("No values are present in the Redis");
    }

    for(int i=0;i<reply->elements;i+=2){
        printf("Command: %s, value: %s\n",reply->element[i]->str, reply->element[i+1]->str);
    }

    return;
}

int main(){

    // setup();
    git_libgit2_init();

    // int client_fd;
    // struct sockaddr_in server_addr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    // struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, "10.129.148.248", &server_addr.sin_addr) <= 0) {
        perror("Invalid server address");
        return EXIT_FAILURE;
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        return EXIT_FAILURE;
    }

    rc = redisConnect(REDIS_ADDR,REDIS_PORT);

    if(rc->err || !rc){
        printf("Redis Connection Failure!!\n");
    }  

    set_values(rc);
    get_values(rc);

    char inpcmd[200];
    int msgid;
    key_t key;

    // ftok to generate unique key
    key = ftok("client", 0);
    // msgget creates a message queue
    // and returns identifier
    msgid = msgget(key, 0666 | IPC_CREAT);

    while(1){
        msgrcv(msgid, inpcmd, sizeof(inpcmd), 0, 0);
        handle_request(inpcmd);
    }




    /*
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, "10.129.148.248", &server_addr.sin_addr) <= 0) {
        perror("Invalid server address");
        return EXIT_FAILURE;
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        return EXIT_FAILURE;
    }

    while (1) {
        char command[BUFFER_SIZE], branch[BUFFER_SIZE], file_path[BUFFER_SIZE];

        printf("\nAvailable commands: PULL, PUSH, HISTORY, EXIT\n");
        printf("Enter command: ");
        fgets(command, sizeof(command), stdin);
        command[strcspn(command, "\n")] = '\0';

        if (strcmp(command, "PUSH") == 0) {
            printf("Enter branch: ");
            fgets(branch, sizeof(branch), stdin);
            branch[strcspn(branch, "\n")] = '\0';

            printf("Enter file path to push: ");
            fgets(file_path, sizeof(file_path), stdin);
            file_path[strcspn(file_path, "\n")] = '\0';

            handle_push(&sockfd, &server_addr, branch, file_path);
        }

        */



    // }


    // rc = redisConnect(REDIS_ADDR,REDIS_PORT);


    // if(rc->err || !rc){
    //     printf("Redis Connection Failure!!\n");
    // }  

    // set_values(rc);
    // // get_values(rc);

    // char inpcmd[200];
    // int msgid;
    // key_t key;

    // // ftok to generate unique key
    // key = ftok("client", 0);
    // // msgget creates a message queue
    // // and returns identifier
    // msgid = msgget(key, 0666 | IPC_CREAT);

    // // sem_init(&sem, 0, MAX_Clients);

    // while(1){
    //     msgrcv(msgid, inpcmd, sizeof(inpcmd), 0, 0);
    //     handle_request(inpcmd);
    // }

    return 0;
}