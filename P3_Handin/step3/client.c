#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<stdint.h>
#include<stdbool.h>
#include<assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>

#define MAXLINE 256
#define MAX_FILE_SIZE 512
// #define MAX_FILE_SIZE (16384 - (16384 >> 2)) * (256)


int server_fd, client_fd;
struct sockaddr_in server_addr;
socklen_t server_len = sizeof(server_addr);
char cmd[MAXLINE];
char buf[MAX_FILE_SIZE];

int main(int argc, char* argv[]) {
    setbuf(stdout, NULL);
    if(argc != 2) {
        printf("usage: ./client <port>\n");
        exit(1);
    }

    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(client_fd == -1) {
        perror("socket error");
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[1]));

    if(connect(client_fd, (struct sockaddr*)&server_addr, server_len) == -1) {
        perror("connect error");
        exit(1);
    }

    while(1){
        // int n = recv(client_fd, buf, MAX_FILE_SIZE, 0);
        // assert(n == 256);
        int n = read(client_fd, buf, MAX_FILE_SIZE);
        if(n == 0)
            break;
        printf("%s", buf);
        // scanf("%s", cmd);
        fgets(cmd, MAXLINE, stdin);
        // printf("%s", cmd);
        send(client_fd, cmd, sizeof(cmd), 0);
        // write(client_fd, cmd, strlen(cmd));
        memset(buf, 0, sizeof(buf));
        memset(cmd, 0, sizeof(cmd));
        read(client_fd, buf, MAX_FILE_SIZE);
        printf("%s", buf);
        if(strcmp(buf, "Goodbye\n") == 0) {
            break;
        }
        memset(buf, 0, sizeof(buf));
    }

    return 0;
}