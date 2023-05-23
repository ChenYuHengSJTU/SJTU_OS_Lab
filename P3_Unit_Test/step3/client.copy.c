//#include <cerrno>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>

int main(int argc, char* argv[]){
  setbuf(stdout, NULL);
  int sock = socket(AF_INET, SOCK_STREAM, 0), client_fd;
  struct sockaddr_in sock_addr, client_addr;
  socklen_t client_len = sizeof(client_addr);
  sock_addr.sin_port = htons(atoi(argv[1]));
  // sock_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 设置服务器 IP 地址
  sock_addr.sin_family = AF_INET;
  if(bind(sock, (const struct sockaddr_in*)&sock_addr, sizeof(sock_addr)) != 0){
    perror("bind error:");
  }
  listen(sock, 10);
  while(1){
    size_t len = sizeof(sock_addr);
    client_fd = accept(sock, &client_addr, &client_len);
    if(fork() == 0){
      char str[32];
      if(client_fd == -1){
        perror("accept error");
        exit(-1);
      }
      // recv(client_fd, str, sizeof(str), 0);
      read(client_fd, str, 32);
      printf("%s\n", str);
      while(1);
      exit(0);
    }
    else{
      printf("another cliet\n");
    }
    printf("one connected\n");
  }
  close(sock);
}