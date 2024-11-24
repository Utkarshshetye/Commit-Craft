#ifndef CONFIG_H
#define CONFIG_H

#include <netinet/in.h>
#define PATH_MAX 256
#define INP_COMMAND_LEN 1024
#define SERVER_PORT 7001
// #define SERVER_ADDR "127.0.0.1"
#define SERVER_ADDR "10.129.148.248"
#define REDIS_ADDR "127.0.0.1"
#define REDIS_PORT 6379
#define PROXY_PORT 7449
#define MAX_Clients 10

typedef struct sockaddr_in clientsockaddr;


typedef struct {
    int sockfd;               
    struct sockaddr_in csaddr;
} cpara;

#endif