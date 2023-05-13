//#include <stdlib.h>
//#include <unistd.h>
//#include <sys/types.h>
//#include <sys/socket.h>
//
//
//
//int main(int argc, char* argv[]){
//      
//}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <assert.h>

#define BUF_SIZE 1024

int main(int argc, char *argv[]) {
  setbuf(stdout, NULL);
  // setbuf(stdin, NULL);
  assert(argc == 2 && "too few or too much args, need 3\n");
  
  int sock;
  char message[BUF_SIZE];
  int str_len;
  struct sockaddr_in serv_addr;

//  if (argc != 3) { // 命令行参数错误
//        printf("Usage: %s <IP> <port>\n", argv[0]);
//    exit(1);
//    }

        sock = socket(AF_INET, SOCK_STREAM, 0); // 创建一个 TCP 套接字
    if (sock == -1)
    perror("socket() error");

      memset(&serv_addr, 0, sizeof(serv_addr)); // 初始化服务器地址为零
    serv_addr.sin_family = AF_INET; // 使用 IPV4 地址族
    // serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 设置服务器 IP 地址
    // in_addr_t
    serv_addr.sin_port = htons(atoi(argv[1])); // 设置服务器端口号

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) // 连接到服务器
        perror("connect() error");

    char str[32];
    // memset(str, 0, sizeof(str));
    scanf("%s", str);
    printf("%s\n", str);
    write(sock, (void*)str, 32);
    // send(sock, "file", 4, 0);
//    str_len = read(sock, message, BUF_SIZE - 1); // 读取服务器发送的消息
//    if (str_len == -1)
//      perror("read() error");

//    printf("Message from server: %s\n", message);

    close(sock); // 关闭套接字

    return 0;
}
