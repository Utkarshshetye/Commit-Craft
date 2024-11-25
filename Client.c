#include "config.h"
#include<stdio.h>
#include<string.h>
#include<sys/socket.h>
#include<sys/stat.h>
#include<arpa/inet.h>
#include<git2.h>
#include<stdbool.h>
#include<limits.h>
#include<unistd.h>
#include<hiredis/hiredis.h>
#include<sys/sendfile.h>
#include<sys/msg.h>
#include<sys/ipc.h>
#include<sys/types.h>

// globals: 
// redisContext *rc;
// git_signature sign;

// void set_values(redisContext * context){
//     redisCommand(context, "HSET commands %s %d", "GIT_INIT", 0);
//     redisCommand(context, "HSET commands %s %d", "GIT_ADD", 1);
//     redisCommand(context, "HSET commands %s %d", "GIT_COMMIT", 2);
//     redisCommand(context, "HSET commands %s %d", "GIT_PUSH", 3);
//     redisCommand(context, "HSET commands %s %d", "GIT_PULL", 4);
//     redisCommand(context, "HSET commands %s %d", "GIT_CHECKOUT", 5);
// }

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

// int get_index(redisContext* context, char* inp) {

//     redisReply* reply = redisCommand(context,"HGET commands %s",inp);

//     if (reply == NULL) {
//         fprintf(stderr, "Error executing command: %s\n", context->errstr);
//         return -1;
//     }

//     if(reply == NULL || reply->type == REDIS_REPLY_NIL){
//         return -1;
//     }

//     int value = atoi(reply->str);

//     freeReplyObject(reply);

//     return value;
// }

// char cwdbuffer[PATH_MAX];

// char* get_path(){
  
//     getcwd(cwdbuffer,sizeof(cwdbuffer));

//     return cwdbuffer;
// }

// void signature_define(){
    
//     // redisReply * reply1 = redisCommand(rc,"HGET sign name");
//     // redisReply * reply2 = redisCommand(rc,"HGET sign email");

//     // reply reply;
//     /*
//     if(reply1->str && reply2->str){
//         // Email and Name is already provided
//         reply.name = reply1->str;
//         reply.email = reply2->str;
//         return reply;
//     }

//     */

//     char email[64];
//     char name[64];

//     printf("git want to know more? Name: ");
//     scanf("%s",name);

//     printf("\n");
//     printf("Email: ");
//     scanf("%s",email);

//     sign.email = email;
//     sign.name = name;

//     // reply.email = email;
//     // reply.name = name;
    
//     // redisCommand(rc,"HSET sign name %s",name);

//     // redisCommand(rc,"HSET sign email %s",email);

//     return;
// }


// void setup(cpara para){

//     int status = connect(para.sockfd,(struct sockaddr *)&para.csaddr,sizeof(para.csaddr));

//     // Setup-Code

//     // char * msg = "hello";
//     // printf("Status: %d\n",status);
//     // scanf("%s",buffer);

//     // send(para.sockfd,buffer,strlen(msg),0);

// }

// // Core functionalities:
// void git_init_at_client(){

//     int count=0;
//     char *path;
//     struct stat *s;

//     git_repository *repo = NULL;

//     stat(".git",s);

//     path = get_path();

//     if(S_ISDIR(s->st_mode)){
//         // .git is already present
//         git_repository_init(&repo,".",false);
//         printf("Reinitialized existing Git repository in %s/.git\n",path);
//     }else{
//         git_repository_init(&repo,".",false);
//         printf("Initialized existing Git repository in %s/.git\n",path);
//     }
// }

// void git_add_at_client(){
//     char *path = get_path();
//     git_repository *repo = NULL;
//     git_index *idx;

//     int status = git_repository_open(&repo,path);

//     if(status!=0){
//         printf("This is not a git repository!!\n");
//         exit(-1);
//     }

//     int idxstatus = git_repository_index(&idx,repo);

//     if(idxstatus!=0){
//         printf("Index Not Found!!\n");
//         exit(-1);
//     }

//     git_index_add_all(idx,NULL,GIT_INDEX_ADD_CHECK_PATHSPEC,NULL,NULL);

//     git_index_write(idx);
// }

// void git_commit_at_client(){

//     char *path = get_path();
//     char msg[50];
//     size_t parent_count = 0;
//     git_reference *ref = NULL;
//     git_repository *repo = NULL;
//     git_object *root = NULL;
//     git_tree *tree = NULL;
//     git_oid id;
//     git_oid cmoid;
//     git_index *idx = NULL;

//     printf("Please enter the commit message for your changes: ");
//     scanf("%s",msg);

//     int status = git_repository_open(&repo,path);

//     if(status!=0){
//         printf("This is not a git repository!!\n");
//         exit(-1);
//     }

//     int idxstatus = git_repository_index(&idx,repo);

//     if(idxstatus!=0){
//         printf("No index present!");
//         exit(-1);
//     }

//     // We need to update the head
//     // need to call: 
//     int revstatus = git_revparse_ext(&root,&ref,repo,"HEAD");

//     if(revstatus==0){
//         // printf("Successful!!\n");
//     }
    
//     if(revstatus==GIT_ENOTFOUND){
//        perror("GIT_ENOTFOUND!!\n");
//     }

//     if(revstatus==GIT_EAMBIGUOUS){
//         printf("GIT_EAMBIGUOUS occured!");
//         exit(-1);
//     }

//     if(revstatus==GIT_EINVALIDSPEC){
//         printf("GIT_EINVALIDSPEC occured!");
//         exit(-1);
//     }
    
//     git_index_write_tree(&id,idx);

//     git_index_write(idx);

//     git_tree_lookup(&tree,repo,&id);

//     signature_define();
    
//     // sign.email = reply.email;
//     // sign.name = reply.name;

//     // Assuming, author and commiter same, so &sign
//     // is coming 2 times here

//     if(tree){
//         parent_count = 1;
//     }

//     git_commit_create_v(&cmoid,repo,"HEAD",&sign,&sign,NULL,msg,tree,parent_count);

//     // git_index_free(idx);
//     // git_signature_free(&sign);
//     // // git_object_free(root);
//     // git_reference_free(ref);
//     // git_tree_free(tree);
// }

// void git_push_at_server(cpara para){
//     setup(para);
    
//     // sendfile()

// }

// void connection(){
//     struct sockaddr_in sockin,incoming;
//     int sockfd = socket(AF_INET,SOCK_STREAM,0),option=1;

//     sem_init(&sem, 0, MAX_Clients);
    
//     if(sockfd == -1){
//         perror("Error while creation of socket!");
//         exit(1);
//     }

//     char buffer[1024]={0},*ip="127.0.0.1";

//     sockin.sin_family = AF_INET;
//     sockin.sin_port = htons(7001);
//     sockin.sin_addr.s_addr = INADDR_ANY;

//     setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&option,sizeof(option));

//     inet_pton(AF_INET,ip,&sockin.sin_addr);

//     bind(sockfd,(struct sockaddr *) &sockin,sizeof(sockin));
    
//     printf("Server started...");

//     listen(sockfd,100); 
//     socklen_t len = sizeof(incoming);

//     while(1){
    
//         int newfd = accept(sockfd,(struct sockaddr *)&incoming,&len);

//         // Handling the user requests:
//         memset(buffer, 0, sizeof(buffer));

//         ssize_t bytes_received = recv(newfd, buffer, sizeof(buffer), 0);
        
//         hadling_request(newfd,buffer);
//         printf("Size of received msg: %ld\n",bytes_received);

//         if (bytes_received > 0) {
//             buffer[bytes_received] = '\0';
//             printf("Msg received: %s\n", buffer);
//         }

//         close(newfd);
//     }
    
//     close(sockfd);

// }

int main(){

    // Message queue for command communication
    int key = ftok("client", 0),msgid;
    char inpcmd[100];
    // scanf("%s",inpcmd);
    
    while(1) {
        // fgets(">>",2,stdout);
        
        puts(">");
        fgets(inpcmd,sizeof(inpcmd),stdin);

        if((msgid = msgget(key,IPC_CREAT | 0666)) < 0){
            printf("Unable to create a connection!");
            exit(1);
        }

        msgsnd(msgid,inpcmd,strlen(inpcmd)+1,0);
    }
    

    return 0;
}


/*
int main(){
    
    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    char* reply;
    char inp[INP_COMMAND_LEN];
    
    clientsockaddr csaddr;

    csaddr.sin_family = AF_INET;
    csaddr.sin_port = htons(SERVER_PORT);

    inet_pton(AF_INET,SERVER_ADDR,&csaddr.sin_addr);

    rc = redisConnect(SERVER_ADDR,REDIS_PORT);

    if(rc->err || !rc){
        printf("Redis Connection Failure!!\n");
    }  

    git_libgit2_init();

    set_values(rc);

    get_values(rc);

    scanf("%s", inp);

    int op = get_index(rc, inp);

    if(op==-1){
        printf("git: '%s' is not a git command.\n",inp);
        exit(-1);
    }

    switch (op){

        case 0:
            git_init_at_client();
            break;

        case 1:
            git_add_at_client();
            break;

        case 2:
            git_commit_at_client();
            break;

        case 3:

            // Setting up the parameters:
            cpara parameters;
            parameters.sockfd = sockfd;
            parameters.csaddr = csaddr;
           
            git_push_at_server(parameters);

            break;

        default:
            break;
        }

        memset(inp,0,sizeof(inp));



    return 0;
}
*/