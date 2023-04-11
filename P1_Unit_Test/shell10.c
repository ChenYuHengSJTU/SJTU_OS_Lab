// #define _GNU_SOURCE
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
#include <errno.h> 
// #include <sstream>

// shell need to realize cd and perhaps some other special commands
// realize the net 
// xv6 sh for reference
// maybe replace all the '\n' with ' ' will be of some help

// maybe realize the > <

// typedef enum __bool { false = 0, true = 1, } bool;

#define MAX_SIZE 32767
#define MAX_ARG 10

char dst[MAX_SIZE] = {'\0'};
char pipe_dst[MAX_SIZE] = {'\0'};
char file_path[MAX_SIZE] = "/bin/";
// char** argv_put;
char* loc = NULL;
char* save_ptr = dst;

bool cmd_before_pipe = false;
bool cmd_after_pipe = false;
bool pipe_exist = false;
bool cmd_line_end = false;
bool received = false;

int fd[MAX_ARG][2];

int pipe_len;

char* Argv[MAX_ARG] = {NULL};
int Argc;

char* _Argv[MAX_ARG] = {NULL};

int cmd_bg[MAX_ARG] = {0}, cmd_end[MAX_ARG] = {0};
int cnt=0;

int sockfd;
bool server = false;
int host_num;

struct sockaddr_in serv_addr;
struct hostent *host;

int parseLine(char* dst, char *argv[], const char* delim) {
    char *p;
    // char *save_ptr = NULL;
    int count=0;
    // char * tmp = dst;
    p = strtok_r(dst, delim, &save_ptr);
    while (p) {
        // int len = strlen(p);
        // if(p[len] == '\n') 
        //     p[len] = '\0';
        argv[count] = p;
        count++;
        p = strtok_r(NULL, delim, &save_ptr);
    }

    return count;
}

void debug(int i){
    fprintf(stderr, "debug cmd[%s]: ", Argv[cmd_bg[i]]);
    for(int i = cmd_bg[i];i<=cmd_end[i];++i){
        fprintf(stderr, "%s ",Argv[i]);
    }
    fprintf(stderr,"\n");
}

bool Process_Input(){
    int sz = strlen(dst);
    dst[sz - 1] = '\0';
    // for(int i = 0;i < sz;++i){
    //     if(dst[i] == '\n') dst[i] = ' ';
    // }

    cnt = 0;
    Argc = parseLine(dst, Argv, " ");
    bool flag = true;
    int i = 0, j = 0;

    for(int i = 0; i < Argc; ++i){
        if(Argv[i] && strcmp(Argv[i], "|") == 0){
            cmd_bg[cnt] = j;
            cmd_end[cnt] = i - 1;
            j = i + 1;
            cnt++;
            if(i <= 0){
                flag = false;
                fprintf(stderr, "error: no command after pipe\n");
            }

            if(i == Argc - 1){
                flag = false;
                fprintf(stderr, "error: no command before pipe\n");
            }
        }

        if(i == Argc - 1){
            cmd_bg[cnt] = j;
            cmd_end[cnt] = i;
            j = i;
            cnt++;
        }
    }

    // fprintf(stderr, "cnt: %d\tArgc: %d\n", cnt, Argc);

    // for(int i = 0;i<cnt;++i){
    //     fprintf(stderr, "%d:begin at %d end at %d\n",i, cmd_bg[i],cmd_end[i]);
    // }

    return flag;
}

void Close_pipe(int k){
    for(int i = 0;i < cnt;++i){
        if(i == k || i == (k - 1))
            continue;
        close(fd[i][0]);
        close(fd[i][1]);
    }
}

int main(int argc, char** argv){
// if port use the argc and aggravate

    int len = 0;


    int    socket_fd, connect_fd;  
    struct sockaddr_in     servaddr;  
    char    buff[4096];  
    int     n;  
    //初始化Socket  
    if( (socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1 ){  
        printf("create socket error: %s(errno: %d)\n",strerror(errno),errno);  
        exit(0);  
    }  
    //初始化  
    memset(&servaddr, 0, sizeof(servaddr));  
    servaddr.sin_family = AF_INET;  
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);//IP地址设置成INADDR_ANY,让系统自动获取本机的IP地址。  
    servaddr.sin_port = htons(atoi(argv[1]));//设置的端口为DEFAULT_PORT  
  
    //将本地地址绑定到所创建的套接字上  
    if( bind(socket_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1){  
    printf("bind socket error: %s(errno: %d)\n",strerror(errno),errno);  
    exit(0);  
    }  
    //开始监听是否有客户端连接  
    if( listen(socket_fd, 10) == -1){  
    printf("listen socket error: %s(errno: %d)\n",strerror(errno),errno);  
    exit(0);  
    }

    while(1){
        //cannot be outputted immediately 
        // printf("$");
        // fprintf(stderr, "$ ");
        memset(dst, 0, MAX_SIZE);
        // len = read(0, dst, MAX_SIZE);

        fprintf(stderr, "listen from client\n");

        if( (connect_fd = accept(socket_fd, (struct sockaddr*)NULL, NULL)) == -1){  
            printf("accept socket error: %s(errno: %d)",strerror(errno),errno);  
            continue;  
        }  


        while(1){


send(connect_fd, "$ ", 2, 0);

        len = recv(connect_fd, dst, MAX_SIZE, 0); 
        // deal with special unseen character 
        dst[len - 1] = '\0';
        fprintf(stderr, "reveive:%s\tlen:%d\n", dst, len);
        
        if(len == 0){
            exit(0);
        }
        
        if(!Process_Input())
            continue;

        if(strcmp(Argv[0], "exit") == 0)
            break;

        for(int i = 0; i < cnt; ++i){
            if(pipe(fd[i]) < 0)
                fprintf(stderr, "create pipe failed\n");
        }

        // do{
        for(int i = 0;i < cnt; ++i){
                if(fork() == 0){
                    // debug(i);
                    // fprintf(stderr, "\n");
                    Close_pipe(i);

                    // if(i < cnt - 1){
                        close(fd[i][0]);
                        close(1);
                        assert(dup2(fd[i][1], STDOUT_FILENO) == 1);
                        close(fd[i][1]);
                    // }
                    // else{
                    //     close(fd[i][0]);
                    //     close(fd[i][1]);
                    // }

                    if(i > 0){
                        close(fd[i - 1][1]);
                        close(0);
                        assert(dup2(fd[i - 1][0], STDIN_FILENO) == 0);
                        close(fd[i - 1][0]);
                    }
                    // else{
                    //     close(fd[0][0]);
                    //     close(fd[0][1]);
                    // }

                    char* tmp[MAX_ARG];

                    for(int k = cmd_bg[i];k <= cmd_end[i]; ++k){
                        tmp[k - cmd_bg[i]] = Argv[k]; 
                    }

                    tmp[cmd_end[i] - cmd_bg[i] + 1] = NULL;

                    // if(strcmp(tmp[0], "exit") == 0 || strcmp(tmp[0], "q") == 0){
                    //     exit(0);
                    // }

                    execvp(tmp[0], tmp);

                    fprintf(stderr, "exec %s failed\n", tmp[0]);
                    exit(1);
                }
                else{
                    // it is important to close the parent pipe
                    if(i > 0){
                        close(fd[i-1][0]);
                        close(fd[i-1][1]);
                    }
                    wait(NULL);
                    // close(-1);
                }
            }

            if(fork() == 0){
                assert(close(fd[cnt - 1][1]) == 0);
                close(0);
                assert(dup2(fd[cnt - 1][0], STDIN_FILENO) == 0);
                assert(close(fd[cnt - 1][0]) == 0);

                char buf[4096]="";

                int sz = read(0, buf, 4096);
                buf[sz] = '\0';

                fprintf(stderr, "%s", buf);

                if(send(connect_fd, buf, sz, 0) == -1)
                    perror("send error");  
                close(connect_fd);  
                exit(0);  

                // exit(0);
            }
            else{
                if(!fd[cnt - 1][0] || !fd[cnt - 1][1]){
                    fprintf(stderr, "close error\n");
                    exit(0);
                }
                assert(close(fd[cnt - 1][0]) == 0);
                assert(close(fd[cnt - 1][1]) == 0);
                // close(connect_fd);  
                wait(NULL);
                fprintf(stderr, "loop end\n");
            }


        // }while(pipe_exist);
            // close(fd[cnt - 1][0]);
            // close(fd[cnt - 1][1]);

        }
        
    }

    close(connect_fd);  
    // close(client_sockfd);
    close(socket_fd); 
    return 0;
}