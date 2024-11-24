#include<stdio.h>
#include<sys/socket.h>
#include<sys/stat.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<pthread.h>
#include<semaphore.h>
#include <fcntl.h>
pthread_mutex_t l1;
sem_t sem;

struct cache{
    char* request;
    char* data;
    int len;
    char* response;
    time_t currtime;
    struct cache *next;
};


/*
struct cache * getData(char * buf){
    return NULL;
}
*/

int main(){

    sem_init(&sem,0,10);
    pthread_t l1;
    pthread_t att[100];
    struct sockaddr_in sockin,incoming;
    int sockfd = socket(AF_INET,SOCK_STREAM,0),option=1,curr=0;
    
    if(sockfd == -1){
        perror("Error while creation of socket!");
        exit(1);
    }

    char buffer[1024]={0},*ip="10.129.148.248";
    int connSock[50];
    sockin.sin_family = AF_INET;
    sockin.sin_port = htons(7001);
    sockin.sin_addr.s_addr = INADDR_ANY;

    setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&option,sizeof(option));

    bind(sockfd,(struct sockaddr *) &sockin,sizeof(sockin));
    
    printf("Server started...");

    listen(sockfd,100); 

    socklen_t len = sizeof(incoming);
    int newfd=0;

    while(1){
        
        newfd= accept(sockfd,(struct sockaddr *)&incoming,&len);
        
        // Handling the user requests:
        memset(buffer, 0, sizeof(buffer));

        int fd = open("sample", O_WRONLY | O_APPEND | O_TRUNC, 0777);
        int byteread=0;

        while((byteread=recv(newfd,buffer,1024,0))> 0){
            write(fd,buffer,byteread);
        }
            
        ssize_t bytes_received = recv(newfd, buffer, sizeof(buffer), 0);

        printf("Total bytes read: %ld\n",bytes_received);
        // hadling_request(newfd,buffer);
        printf("Size of received msg: %ld\n",bytes_received);

        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            printf("Msg received: %s\n", buffer);
        }
    }
    
    close(sockfd);


    return 0;
}
