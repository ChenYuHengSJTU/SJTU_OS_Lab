// #define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>
// #include <sstream>

// shell need to realize cd and perhaps some other special commands
// realize the net 
// xv6 sh for reference
// maybe replace all the '\n' with ' ' will be of some help

// maybe realize the > <

// maybe vim and less and some other cmd that will make use of the screen will
// not work as shell, but it will works and we may not see the output

// define the max size of command line input and max nums of args
#define MAX_SIZE 32767
#define MAX_ARG 100

// the array which carries the input
char dst[MAX_SIZE] = {'\0'};
int len = 0;
// to be used in strtok_r
char* save_ptr = dst;

// some bool var to judge whether the command line is valid
bool cmd_before_pipe = false;
bool cmd_after_pipe = false;
bool pipe_exist = false;
bool received = false;

// pipe
int fd[MAX_ARG][2];

// argv
char* Argv[MAX_ARG] = {NULL};
int Argc;

// mark the start and end of commands divided by |
int cmd_bg[MAX_ARG] = {0}, cmd_end[MAX_ARG] = {0};
int cnt=0;

// for connection between terminal
int socket_fd, connect_fd;  
struct sockaddr_in servaddr;  

char work_dir[MAX_SIZE];
int dir_len = 0;

// get args
int parseLine(char* dst, char *argv[], const char* delim) {
    char *p;
    int count=0;
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

// for debug
void debug(int i){
    fprintf(stderr, "debug cmd[%s]: ", Argv[cmd_bg[i]]);
    for(int i = cmd_bg[i];i<=cmd_end[i];++i){
        fprintf(stderr, "%s ",Argv[i]);
    }
    fprintf(stderr,"\n");
}

// to process the args
bool Process_Input(){
    int sz = strlen(dst);
    dst[sz - 1] = '\0';

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

    return flag;
}

// close unused pipes
void Close_pipe(int k){
    for(int i = 0;i < cnt;++i){
        if(i == k || i == (k - 1))
            continue;
        close(fd[i][0]);
        close(fd[i][1]);
    }
}

void Cmd_cd(const char* dir){
    DIR* dp = NULL;
    struct dirent* entry;
    struct stat statbuf;

    dp = opendir(dir);
    
    if(dp == NULL){
        fprintf(stderr, "cannot open folder %s\n", dir);
    }

    if(chdir(dir) == -1){
        perror("chdir error");
    }

    fprintf(stderr, "chdir to %s\n", dir);

    closedir(dp);
}

static inline void __attribute__((destructor))
Clear(){
    fflush(stdout);
    close(socket_fd);
    close(connect_fd);
    Close_pipe(-1);
    // fprintf(stderr, "end\n");
}

int main(int argc, char** argv){
    if(argc <= 1){
        fprintf(stderr, "Too few arguments\nUsage:./shell <Port>\n");
        exit(1);
    }

    if(argc >= 3){
        fprintf(stderr, "Too many arguments\nUsage:./shell <Port>\n");
        exit(1);
    }

    if( (socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1 ){  
        printf("create socket error: %s(errno: %d)\n",strerror(errno),errno);  
        exit(0);  
    }  

    memset(&servaddr, 0, sizeof(servaddr));  
    servaddr.sin_family = AF_INET;  
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(atoi(argv[1]));
    
    if( bind(socket_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1){  
        printf("bind socket error: %s(errno: %d)\n",strerror(errno),errno);  
        exit(0);  
    }  

    if( listen(socket_fd, 10) == -1){  
    printf("listen socket error: %s(errno: %d)\n",strerror(errno),errno);  
    exit(0);  
    }

    // signal(SIGCHLD, SIG_IGN);

    // the main loop of the server
    // to support more clients
    while(1){
        if( (connect_fd = accept(socket_fd, (struct sockaddr*)NULL, NULL)) == -1){  
            printf("accept socket error: %s(errno: %d)",strerror(errno),errno);  
            exit(1);
        }  
        
        fprintf(stderr, "listen from a new client\n");
        
        // fork() to create child process to support more than one client
        // each fork() corresponds with a client
        if(fork() == 0){

            // main loop in child process to wait for input
            while(1){
                memset(work_dir, 0, MAX_SIZE);
                dir_len = strlen(getcwd(work_dir, MAX_SIZE));
                send(connect_fd, "\033[0;34m", strlen("\033[0;34m"), 0);
                send(connect_fd, work_dir, dir_len, 0);
                send(connect_fd, "$ ", 2, 0);
                send(connect_fd, "\033[0m\n", strlen("\033[0m"), 0);
                memset(dst, 0, MAX_SIZE);
                len = recv(connect_fd, dst, MAX_SIZE, 0); 
                // deal with special unseen character 
                dst[len - 1] = '\0';
                // fprintf(stderr, "reveive:%s\tlen:%d\n", dst, len);
        
                // exit
                if(len == 0){
                    // exit(0);
                    // fprintf(stderr, "get nothing\n");
                    continue;
                }
        
                if(!Process_Input())
                    continue;

                // exit
                if(strcmp(Argv[0], "exit") == 0){
                    send(connect_fd ,"Byebye!\n", strlen("Byebye!\n"), 0);
                    close(connect_fd);
                    // fflush(stdout);
                    exit(0);
                }

                // create pipe
                for(int i = 0; i < cnt; ++i){
                    if(pipe(fd[i]) < 0)
                        fprintf(stderr, "create pipe failed\n");
                }

                // deal with cnt cmd divided by |
                // aside from the first command, each cmd will get standard input
                // from fd[i-1][0], and output to fd[i][1]
                for(int i = 0;i < cnt; ++i){
                    if(fork() == 0){
                        Close_pipe(i);
                    
                        close(fd[i][0]);
                        close(1);
                        assert(dup2(fd[i][1], STDOUT_FILENO) == 1);
                        close(fd[i][1]);

                        if(i > 0){
                            close(fd[i - 1][1]);
                            close(0);
                            assert(dup2(fd[i - 1][0], STDIN_FILENO) == 0);
                            close(fd[i - 1][0]);
                        }

                        char* tmp[MAX_ARG];

                        for(int k = cmd_bg[i];k <= cmd_end[i]; ++k){
                            tmp[k - cmd_bg[i]] = Argv[k]; 
                        }

                        tmp[cmd_end[i] - cmd_bg[i] + 1] = NULL;

                        fflush(stdout);

                        if(strcmp(tmp[0], "cd") == 0 || strcmp(tmp[0], "chdir") == 0){
                            // Cmd_cd(Argv[1]);
                            // continue;
                            // just exit
                            exit(0);
                        }
                        execvp(tmp[0], tmp);

                        // if exec successgully, will never get here
                        char sent[1024] = "";
                        sprintf(sent, "exec %s failed\n", tmp[0]);
                        send(connect_fd, sent, strlen(sent), 0);
                        exit(1);
                    }
                    else{
                        // it is important to close the parent pipe
                        if(i > 0){
                            close(fd[i-1][0]);
                            close(fd[i-1][1]);
                        }
                        wait(NULL);
                        if(strcmp(Argv[cmd_bg[i]], "cd") == 0 || strcmp(Argv[cmd_bg[i]], "chdir") == 0){
                            Cmd_cd(Argv[cmd_bg[i] + 1]);
                        }
                    }
                }

                // another child process to redirect the output to client
                if(fork() == 0){
                    assert(close(fd[cnt - 1][1]) == 0);
                    close(0);
                    assert(dup2(fd[cnt - 1][0], STDIN_FILENO) == 0);
                    assert(close(fd[cnt - 1][0]) == 0);

                    char buf[4096]="";

                    int sz = read(0, buf, 4096);
                    buf[sz] = '\0';

                    // fprintf(stderr, "%s", buf);

                    if(send(connect_fd, buf, sz, 0) == -1)
                        perror("send error");  

                    // if(write(connect_fd, buf, sz) == -1)
                    //     perror("write error");
                
                    // printf("send over\n");
                    fflush(stdout);
                    close(connect_fd);  
                    exit(0);  
                }
                else{
                    if(!fd[cnt - 1][0] || !fd[cnt - 1][1]){
                        fprintf(stderr, "close error\n");
                        exit(0);
                    }
            
                    assert(close(fd[cnt - 1][0]) == 0);
                    assert(close(fd[cnt - 1][1]) == 0);

                    wait(NULL);
                    // fprintf(stderr, "loop end\n");
                }     
        
            }
            fflush(stdout);
            close(connect_fd);
            exit(0);
        }
        else{
            close(connect_fd);
            fprintf(stderr, "preparing for another client\n");
        }
    }

    close(connect_fd);  
    close(socket_fd); 
    return 0;
}