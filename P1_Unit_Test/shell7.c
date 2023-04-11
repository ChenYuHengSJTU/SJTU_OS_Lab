#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stddef.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

int main(int argc,char** argv){
    int sockfd;
sockfd = socket(AF_INET, SOCK_STREAM, 0);
// error check
struct sockaddr_in serv_addr;
struct hostent *host;
serv_addr.sin_family = AF_INET;
host = gethostbyname(argv[1]);
// error check
memcpy(&serv_addr.sin_addr.s_addr, host->h_addr, 
host->h_length);
serv_addr.sin_port = htons(atoi(argv[1]));
connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));

    

}
