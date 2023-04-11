#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stddef.h>
#include <stdbool.h>

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
    for(int i = 0;i<Argc;++i){
        if(i == k || i == (k - 1))
            continue;
        close(fd[i][0]);
        close(fd[i][1]);
    }
}

int fd1[2];

int main(int argc, char* argv){
// if port use the argc and aggravate

    int len = 0;

    while(1){
        //cannot be outputted immediately 
        // printf("$");
        fprintf(stderr, "$ ");
        memset(dst, 0, MAX_SIZE);
        len = read(0, dst, MAX_SIZE);
        if(len == 0) 
            continue;
        
        if(!Process_Input())
            continue;

        // for(int i = 0; i < cnt; ++i){
        //     if(pipe(fd[i]) < 0)
        //         fprintf(stderr, "create pipe failed\n");
        // }
        assert(pipe(fd1) >= 0);

        // do{
            for(int i = 0;i < 2; ++i){
                if(fork() == 0){
                    // debug(i);
                    // fprintf(stderr, "\n");
                    // Close_pipe(i);
                    

                    if(i == 0){
                        close(fd1[0]);
                        close(1);
                        assert(dup2(fd1[1], STDOUT_FILENO) == 1);
                        close(fd1[1]);
                    }
                        
                   
                    if(i == 1){
                        close(fd1[1]);
                        close(0);
                        assert(dup2(fd1[0], STDIN_FILENO) == 0);
                        close(fd1[0]);
                    }
                   
                        
                        


                    char* tmp[MAX_ARG];

                    for(int k = cmd_bg[i];k <= cmd_end[i]; ++k){
                        tmp[k - cmd_bg[i]] = Argv[k]; 
                    }

                    tmp[cmd_end[i] - cmd_bg[i] + 1] = NULL;

                    execvp(tmp[0], tmp);

                    fprintf(stderr, "exec %s failed\n", tmp[0]);

                }
                else{
                    if(i == 1){
                        close(fd1[1]);
                        close(fd1[0]);
                    }
                    wait(NULL);
                    // close(-1);
                }
            }
            close(fd1[0]);
            close(fd1[1]);
        // }while(pipe_exist);

    }

    return 0;
}