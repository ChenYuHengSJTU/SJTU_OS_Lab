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

int fd[2];

void redirect(int fd[2]);

int getargs(char** argvs){
    int j = 0;
    int n = 0;
    for(int i = 0; dst[i]; i++){
        if(dst[i] == ' '){
            argvs[n++] = &dst[j];
            dst[i] = '\0';
            i++;
            while(dst[i] && dst[i] == ' '){
                i++;
                dst[i] = '\0';
            }
            j = i;
            i--;
            // printf("i: %d\tj: %d\n", i, j);
        }
    }
    argvs[n++] = &dst[j];

    argvs[n] = NULL;
    return n;
}

int parseLine(char* dst, char *argv[], const char* delim) {
    char *p;
    int count=0;
    p = strtok_r(save_ptr, delim, &save_ptr);
    while (p && strcmp(p,"|")!=0) {
        int len = strlen(p);
        if(p[len] == '\n') 
            p[len] = '\0';
        argv[count] = p;
        count++;
        p = strtok_r(NULL, delim, &save_ptr);
    }

    if(pipe_exist && count) 
        cmd_after_pipe = true;

    if(count){
        cmd_before_pipe = true;
    }

    if(p && strcmp(p, "|") == 0){
        if(!cmd_before_pipe){
            fprintf(stderr, "error: no command before pipe\n");
        }
        else {
            // fprintf(stderr, "find a pipe\n");
            pipe_exist = true;
            // redirect(fd);
        }
    }

    if(!p) cmd_line_end = true;

    return count;
}

void replace_n(char* dst){
    int sz = strlen(dst);
    for(int i = 0;i < sz;++i){
        if(dst[i] == '\n') dst[i] = '\0';
    }
}

void remove_n(char** argv, int sz){
    for(int i = 0;i < sz; ++i){
        int len = strlen(argv[i]);
        if(argv[i][len - 1] == '\n') argv[i][len - 1] = '\0';
    }
}

// seem useless
void clear(char** argvs){
    free(argvs);
}

void debug(char** argvs, int _argc){
    printf("argc: %d\n", _argc);
    assert(argvs[0] != NULL);
    printf("%s\n", argvs[0]);
    for(int i = 0; argvs[i] != NULL; i++){
        printf("%s\n", argvs[i]);
    }
}

void redirect(int fd[2]){
    close(0); // close stdin
    dup(fd[0]); // copy fd[0] to stdin
    close(fd[0]);
    close(1);
    dup(fd[1]);
    close(fd[1]);
}

int main(int argc, char* argv){
    // if port use the argc and aggravate

    char** argv1 = (char**)(malloc(sizeof(char*) * MAX_ARG));
    char** argv2 = (char**)(malloc(sizeof(char*) * MAX_ARG));
    // char** argv_put = (char**)(malloc(sizeof(char*) * MAX_ARG));

    // int fd[2];

    while(1){
        //cannot be outputted immediately 
        // printf("$");

        memset(dst, '\0', MAX_SIZE);
        // write(2, "$ ", 2);
        fprintf(stderr, "$ ");
        int len = read(0, dst, MAX_SIZE);
        // printf("readin: %d\ndst: %s\n", len, dst);
        assert(len > 0 && len < MAX_SIZE);
        if(len == 0) continue;
        if(len == 1 && dst[0] == '\n') continue;
        dst[len] = '\0';

        save_ptr = dst;
        // 如果是多个空格呢？
        // char* tmp;
        // tmp = strtok_s(dst, " ");
        while(1){
            // printf("old line\n");
			int _argc = parseLine(save_ptr, argv1, " ");

	        // fprintf(stderr, "argc: %d\n", argc);

            // reach the end of the cmd line
            if(_argc == 0)
                break;
	
	        // delete the last '\n'
            int _len = strlen(argv1[_argc - 1]);
            if(argv1[_argc - 1][_len - 1] == '\n')
	            argv1[_argc - 1][_len - 1] = '\0';
    
	        argv1[_argc] = NULL;
            assert(argv1[0] != NULL);
	
	        assert(_argc < MAX_ARG && _argc > 0);
	        
	        if(strcmp(argv1[0], "exit") == 0){
	            exit(0);
	        }
	
            if(!pipe_exist){
                if(fork() == 0){
	                if(execvp(argv1[0], argv1) == -1){
	                    fprintf(stderr, "exec error\n");
	                    exit(1);
	                }
	                exit(0);
	            }
	            else{
	                wait(NULL);
	            }
            }
            else{
                // fprintf(stderr, "pipe in\n");

                if(pipe(fd) == -1){
                    fprintf(stderr, "pipe error\n");
                    exit(1);
                }

                int pid[2] = {-1, -1};
                for(int i = 0;i < 2;++i){
                    pid[i] = fork();
                    if(pid[i] == 0){
                        if(i == 0){
                            close(fd[0]);
                            close(1);
                            assert(dup2(fd[1], STDOUT_FILENO) == 1);
                            close(fd[1]);

                            if(execvp(argv1[0], argv1) == -1){
	                            fprintf(stderr, "cmd: %s\nexec fork1 error\n", argv1[0]);
	                            exit(1);
	                        }

                            exit(0);
                        }
                        else if(i == 1){
                            close(fd[1]);
                            close(0);
                            assert(dup2(fd[0], STDIN_FILENO) == 0);
                            close(fd[0]);

                            // _argc = parseLine(save_ptr, argv2, " ");

                            // // delete the last '\n'
                            // int _len = strlen(argv2[_argc - 1]);
                            // if(argv2[_argc - 1][_len - 1] == '\n')
            	            //     argv2[_argc - 1][_len - 1] = '\0';

                            // // fprintf(stderr, "argv2[0]: %s\n", argv2[0]);

                            // if(_argc == 0){
                            //     fprintf(stderr, "error: no command after pipe\n");
                            //     break;
                            // }

                            // char dst_pipe[MAX_SIZE] = "";
                            // len = read(0, dst_pipe, MAX_SIZE);
                            // dst_pipe[len] = '\0';
                            // assert(len > 0 && len < MAX_SIZE);

                            // // replace_n(dst_pipe);
                            // // fprintf(stderr, "read: %s\n", dst);

                            // // fprintf(stderr, "save_ptr  before: %s\n", save_ptr);
                            // save_ptr = dst_pipe;
                            // // fprintf(stderr, "save_ptr  after: %s\n", save_ptr);
                            // _argc += parseLine(save_ptr, argv2 + _argc, " ");

                            

                            // if(strcmp(argv2[0], "wc\0") == 0 || strcmp(argv2[0], "cat") == 0 || strcmp(argv2[0], "find") == 0)
                            //     remove_n(argv2, _argc);

                            // argv2[_argc] = NULL;
                            // fprintf(stderr, "_argc all: %d\n", _argc);

                            if(execvp(argv2[0], argv2) == -1){
	                            fprintf(stderr, "cmd: %s\nexec fork2 error\n", argv2[0]);
	                            exit(1);
	                        }

                            exit(0);
                        }   
                    }
                    else{
                        wait(NULL);
                    }
                }


                close(fd[0]);
                close(fd[1]);
                pipe_exist = false;
            }

            if(cmd_line_end){
                cmd_line_end = false;
                break;
            }
        }

        if(pipe_exist){
            fprintf(stderr, "error: no command after pipe\n");
        }

        // close(fd[0]);
        // close(fd[1]);
        pipe_exist = false;
        cmd_before_pipe = false;
        cmd_after_pipe = false;
    }

    free(argv1);
    free(argv2);
    return 0;
}