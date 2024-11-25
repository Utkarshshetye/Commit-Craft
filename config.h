#ifndef CONFIG_H
#define CONFIG_H

#include <netinet/in.h>
#define PATH_MAX 256
#define INP_COMMAND_LEN 1024
#define SERVER_PORT 7777
// #define SERVER_ADDR "127.0.0.1"
#define SERVER_ADDR "10.129.148.248"
// #define SERVER_ADDR "172.19.136.254"
#define REDIS_ADDR "127.0.0.1"
#define REDIS_PORT 6379
#define PROXY_PORT 7449
#define MAX_Clients 10
#define MAX_FNM_LEN 128
#define MAX_FILETYPE_LEN 8
#define BUFFER_SIZE 8192
#define HASH_SIZE 40
 
typedef struct sockaddr_in clientsockaddr;

typedef struct {
    int sockfd;               
    struct sockaddr_in csaddr;
} cpara;

typedef struct{
    char * filename;
    char * filetype;
} filebuf;

char command[BUFFER_SIZE], branch[BUFFER_SIZE], file_path[BUFFER_SIZE];

#endif