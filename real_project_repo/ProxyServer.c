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

enum commands {ADD,INIT};
sem_t sem;
char cwdbuffer[PATH_MAX];

redisContext *rc;
git_signature sign;
cpara para;

void set_values(redisContext * context){
    redisCommand(context, "HSET commands %s %d", "GIT_INIT", 0);
    redisCommand(context, "HSET commands %s %d", "GIT_ADD", 1);
    redisCommand(context, "HSET commands %s %d", "GIT_COMMIT", 2);
    redisCommand(context, "HSET commands %s %d", "GIT_PUSH", 3);
    redisCommand(context, "HSET commands %s %d", "GIT_PULL", 4);
    redisCommand(context, "HSET commands %s %d", "GIT_CHECKOUT", 5);
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
    
    // printf("Using hardcoded signature for automated testing\n");
    char * nm = strdup("Test User");
    char * nm1 = strdup("test@example.com");
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

    printf("Using hardcoded commit message for automated testing\n");
    strcpy(msg, "Automated E2E Test Commit");

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


    // git_index_add_all(idx,NULL,GIT_INDEX_ADD_CHECK_PATHSPEC,NULL,NULL);







    // We need to update the head
    // need to call: 
    /*
    int revstatus = git_revparse_ext(&root,&ref,repo,"HEAD");

    if(revstatus==0){
        // printf("Successful!!\n");
    }
    
    if(revstatus==GIT_ENOTFOUND){
       perror("GIT_ENOTFOUND!!\n");
    }

    if(revstatus==GIT_EAMBIGUOUS){
        printf("GIT_EAMBIGUOUS occured!");
        exit(-1);
    }

    if(revstatus==GIT_EINVALIDSPEC){
        printf("GIT_EINVALIDSPEC occured!");
        exit(-1);
    }
    
    */


    git_index_write_tree(&id,idx);

    // git_index_write(idx);

    git_tree_lookup(&tree,repo,&id);

    

    ///
    // git_oid headoid;

    // int ref_status = git_reference_name_to_id(&headoid, repo, "HEAD");

    // if (ref_status != 0) {
    //     fprintf(stderr, "Failed to resolve HEAD: %s\n", git_error_last()->message);
    //     git_repository_free(repo);
    //     return;
    // }
    
    // int commit_status = git_commit_lookup(&parentcommit, repo,&headoid);
   

    signature_define();
    
    // sign.email = reply.email;
    // sign.name = reply.name;
    free(sign.email);
    free(sign.name);
    
    // sign.email = NULL;
    // sign.name = NULL;

    // Assuming, author and commiter same, so &sign
    // is coming 2 times here

    if(tree){
        parent_count = 1;
    }

    git_commit_create_v(&cmoid,repo,"HEAD",&sign,&sign,NULL,msg,tree,parent_count);

    git_index_free(idx);
    // git_signature_free(&sign);
    git_object_free(root);
    git_reference_free(ref);
    // git_tree_free(tree);
    // git_repository_free(repo);
}


void git_init_at_client(){

    int count=0;
    char *path;
    struct stat *s;

    git_repository *repo = NULL;

    stat(".git",s);

    path = get_path();

    // printf("%s\n",path);

    git_repository_init(&repo,".",false);

    printf("Initialized existing Git repository in %s/.git\n",path);
}

int sockfd;

void setup(){

    sockfd = socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in sockin,incoming;
    clientsockaddr sockaddr;

    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(SERVER_PORT);
    
    inet_pton(AF_INET,SERVER_ADDR,&sockaddr.sin_addr);

    int status = connect(sockfd,(struct sockaddr *)&sockaddr,sizeof(sockaddr));

    // Setup-Code

    // printf("Status: %d\n",status);
    // scanf("%s",buffer);
    
    if(status < 0){
        printf("Connection Failed!!\n");
    }


    // printf("Message sent successfully!!!\n");

    // close(para.sockfd);
}

void git_push_server() {
    char msg[50] = "hello";
    git_repository *repo;
    git_oid head_oid;
    git_commit *commit = NULL;
    git_commit *parentcommit = NULL;
    git_tree *current_tree = NULL;
    git_tree *parent_tree = NULL;
    git_diff *diff = NULL;
    git_status_list *status_list = NULL;

    char *path = get_path();
    git_index *idx;

    // git_repository_init(&repo,".",false);

    int repo_status = git_repository_open(&repo, path);
    if (repo_status != 0) {
        fprintf(stderr, "Failed to open repository: %s\n", git_error_last()->message);
        return;
    }

    int ref_status = git_reference_name_to_id(&head_oid, repo, "HEAD");

    if (ref_status == 0) {
        int commit_status = git_commit_lookup(&commit, repo, &head_oid);
        if (commit_status != 0) {
            fprintf(stderr, "Warning: Failed to lookup commit: %s\n", git_error_last()->message);
            fflush(stderr);
        }
    } else {
        printf("No HEAD found; this is the first push.\n");
        fflush(stdout);
    }

    if (commit) {
        if (git_commit_parentcount(commit) > 0) {
            const git_oid *parent_oid = git_commit_parent_id(commit, 0);
            if (parent_oid != NULL) {
                int parent_status = git_commit_lookup(&parentcommit, repo, parent_oid);
                if (parent_status != 0) {
                    fprintf(stderr, "Warning: Failed to lookup parent commit: %s\n", git_error_last()->message);
                }
            }
        }

        if (git_commit_tree(&current_tree, commit) != 0) {
            fprintf(stderr, "Warning: Failed to get tree for current commit: %s\n", git_error_last()->message);
        }
    }

    if (parentcommit) {
        if (git_commit_tree(&parent_tree, parentcommit) != 0) {
            fprintf(stderr, "Warning: Failed to get tree for parent commit: %s\n", git_error_last()->message);
        }
    }

    if (parent_tree && current_tree) {
        int diff_status = git_diff_tree_to_tree(&diff, repo, parent_tree, current_tree, NULL);

        if (diff_status != 0) {
            fprintf(stderr, "Warning: Failed to generate diff: %s\n", git_error_last()->message);
        }

    } else {
        printf("No parent tree available; assuming initial commit.\n");
        fflush(stdout);
    }


    int error = git_status_list_new(&status_list,repo,NULL);

    int count = git_status_list_entrycount(status_list);
    printf("Files changed: %d\n", count);
    fflush(stdout);

    if(count==0){
        printf("Working directory clean, no files to push!\n");
    } else {
        for(int i=0; i<count; i++){
            const git_status_entry * entry = git_status_byindex(status_list, i);
            
            // Get path whether it's in index or working directory
            const char * path = NULL;
            if (entry->index_to_workdir) {
                path = entry->index_to_workdir->new_file.path;
            } else if (entry->head_to_index) {
                path = entry->head_to_index->new_file.path;
            }

            if (path != NULL && entry->status != GIT_STATUS_CURRENT && entry->status != GIT_STATUS_IGNORED) {
                printf("Processing file: %s (status: %u)\n", path, entry->status);
                fflush(stdout);
                int fd = open(path, O_RDONLY);
                if (fd < 0) {
                    perror("Failed to open file");
                    continue;
                }
                
                struct stat statbuf;
                fstat(fd, &statbuf);
                off_t off = statbuf.st_size;
                off_t start = 0;

                // Open a new connection for each file since Main_Server closes the socket after one command
                setup();

                // Send the required network protocol header to Main_Server
                char header[256];
                snprintf(header, sizeof(header), "PUSH main %ld %s\n", (long)off, path);
                send(sockfd, header, strlen(header), 0);

                // Use zero-copy to transfer the file payload robustly in chunks if needed
                printf("Starting zero-copy sendfile for %ld bytes...\n", (long)off);
                fflush(stdout);
                off_t bytes_to_send = off;
                while (bytes_to_send > 0) {
                    ssize_t sent = sendfile(sockfd, fd, &start, bytes_to_send);
                    if (sent <= 0) {
                        perror("sendfile failed");
                        break;
                    }
                    bytes_to_send -= sent;
                    printf("Sent chunk: %ld bytes (remaining: %ld)\n", (long)sent, (long)bytes_to_send);
                }
                printf("...Sent %s (%ld bytes) successfully. Waiting for server confirmation...\n", path, (long)off);
                char srv_resp[256] = {0};
                recv(sockfd, srv_resp, sizeof(srv_resp)-1, 0);
                printf("Server replied: %s\n", srv_resp);
                fflush(stdout);
                close(fd);
                close(sockfd);
            }
        }    
    }

    // send(sockfd, msg, strlen(msg), 0);
   
    if(diff) git_diff_free(diff);
    if (parent_tree) git_tree_free(parent_tree);
    if (current_tree) git_tree_free(current_tree);
    if (parentcommit) git_commit_free(parentcommit);

    git_status_list_free(status_list);
    git_commit_free(commit);
    git_repository_free(repo);
}

/*
void git_push_server(){
    setup();

    char msg[50] = "hello";
    git_repository *repo;
    git_oid out;
    git_commit *commit = NULL;
    git_commit *parentcommit = NULL;
    git_tree *ct;
    git_tree *pt;
    git_diff *df;

    char *path = get_path();
    // git_repository *repo = NULL;
    git_index *idx;


    // int status = git_repository_open(&repo, ".");
    git_repository_open(&repo,path);

    // Latest changes made
    git_reference_name_to_id(&out,repo,"HEAD");

    // get total commits done
    git_commit_lookup(&commit,repo,&out);

    if(git_commit_parentcount(commit) > 0){
        // Parent exist
        const git_oid * oid = git_commit_parent_id(commit,0);       
        int status = git_commit_lookup(&parentcommit,repo,oid);

        if(status!=0){
            fprintf(stderr, "Failed to lookup parent commit: %s\n", git_error_last()->message);
            fflush(stdout);
        }
    }

    // commit tree and parent tree
    
    // git_commit_tree(&ct,commit);
    if (git_commit_tree(&ct, commit) != 0) {
        fprintf(stderr, "Could not get tree for the current commit\n");
        // return -1;
        fflush(stdout);
    }

    if(parentcommit){
        if (git_commit_tree(&pt, parentcommit) != 0) {
            fprintf(stderr, "Could not get tree for the parent commit\n");
            // git_tree_free(parentcommit); // Free the commit tree if it was allocated
            fflush(stdout);
        }
        git_diff_tree_to_tree(&df,repo,pt,ct,NULL);
    }

    // git_commit_tree(&pt,parentcommit);

    send(sockfd,msg,strlen(msg),0);

    // if(git_diff_tree_to_tree(&df,repo,pt,ct,NULL)<0){
    //     printf("Unable to compare the changes\n");
    //     exit(1);
    // }

    // int dfrecords = git_diff_num_deltas(df);
    
    // char message[1024] = {0};
    // snprintf(message, sizeof(message), "Commit ID: %s\nMessage: %s\nChanged Files:\n", 
    //          git_oid_tostr_s(&out), git_commit_message(commit));

    // printf("Buffer content: %s",message);
    // send(sockfd, message, strlen(message), 0);

    // Let's say, we sent the data somehow
}

*/

// Use the Remote Repository to fulfil the request
void hadling_remote_request(char * cmd){
    if(strcmp(cmd,"GIT_PUSH")==0){
        git_push_server();
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
        default:
            // printf("PUSH: %d\n",op);
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

    git_libgit2_init();

    rc = redisConnect(REDIS_ADDR,REDIS_PORT);

    if(rc->err || !rc){
        printf("Redis Connection Failure!!\n");
    }  
    
    set_values(rc);
    // get_values(rc);

    char inpcmd[200];
    int msgid;
    key_t key;

    // ftok to generate unique key
    key = ftok("client", 0);
    // msgget creates a message queue
    // and returns identifier
    msgid = msgget(key, 0666 | IPC_CREAT);

    sem_init(&sem, 0, MAX_Clients);

    while(1){
        memset(inpcmd, 0, sizeof(inpcmd));
        if (msgrcv(msgid, inpcmd, sizeof(inpcmd), 0, 0) < 0) {
            perror("msgrcv failed");
            sleep(1);
            continue;
        }
        if (strlen(inpcmd) > 0) {
            handle_request(inpcmd);
        }
    }
    
    return 0;
}