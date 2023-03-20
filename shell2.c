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

int fd[2];

int pipe_len;

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
    // char *save_ptr = NULL;
    int count=0;
    char * tmp = dst;
    p = strtok_r(tmp, delim, &save_ptr);
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
            return parseLine(dst, argv, delim);
        }
        else {
            pipe_exist = true;
        }
    }

    // if(!p) cmd_line_end = true;

    return count;
}

int _parseLine(char* dst, char *argv[], const char* delim) {
    char *p;
    char *save_ptr = NULL;
    int count=0;
    p = strtok_r(dst, delim, &save_ptr);
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
            return parseLine(dst, argv, delim);
        }
        else {
            pipe_exist = true;
        }
    }

    // if(!p) cmd_line_end = true;

    return count;
}

void replace_n(char* dst){
    int sz = strlen(dst);
    for(int i = 0;i < sz;++i){
        if(dst[i] == '\n') dst[i] = ' ';
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

void debug(char** argvs, int Argc){
    printf("argc: %d\n", Argc);
    assert(argvs[0] != NULL);
    printf("%s\n", argvs[0]);
    for(int i = 0; argvs[i] != NULL; i++){
        printf("%s\n", argvs[i]);
    }
}


int main(int argc, char* argv){
    // if port use the argc and aggravate

    char** Argv = (char**)(malloc(sizeof(char*) * MAX_ARG));
    char** Argv_pipe = (char**)(malloc(sizeof(char*) * MAX_ARG));
    // char** argv_put = (char**)(malloc(sizeof(char*) * MAX_ARG));

    // int fd[2];
    int Argc;
    int Argc_pipe;
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

        if(len && dst[len - 1] == '\n')
            dst[len - 1] = '\0';

        save_ptr = dst;
        // 如果是多个空格呢？
        // char* tmp;
        // tmp = strtok_s(dst, " ");
        while(1){
            // printf("old line\n");
			Argc = parseLine(save_ptr, Argv, " ");
            // fprintf(stderr, "save_ptr:%s\n", save_ptr);
	        // fprintf(stderr, "argc: %d\n", argc);

            // reach the end of the cmd line
            if(Argc == 0){
                break;
            }
	
	        // delete the last '\n'
            // int _len = strlen(Argv[Argc - 1]);
            // if(argv1[Argc - 1][_len - 1] == '\n')
	        //     argv1[Argc - 1][_len - 1] = '\0';
	
	        assert(Argc < MAX_ARG && Argc > 0);
	        
	        if(strcmp(Argv[0], "exit") == 0){
	            exit(0);
	        }
	
            if(received){
                // for(int i = 0; i < Argc_pipe ; ++i){
                //     Argv[Argc + i] = Argv_pipe[i];
                // }
                // Argc += Argc_pipe;

                

            }

            Argv[Argc] = NULL;
            assert(Argv[0] != NULL);

            // debug(Argv, Argc);

            if(pipe(fd) == -1){
                fprintf(stderr, "pipe error\n");
                exit(1);
            }                

            if(fork() == 0){
                close(fd[0]);
                close(1);
                assert(dup2(fd[1], STDOUT_FILENO) == 1);

                // fprintf(stderr, "cmd: %s\n", Argv[0]);
                execvp(Argv[0], Argv);
                
                fprintf(stderr, "exec %s failed\n", Argv[0]);
            }
            else{
                wait(NULL);
                close(fd[1]);

                if(received){
                    received = false;
                    memset(pipe_dst, 0, MAX_SIZE);
                }

                // pipe_len = read(fd[0], pipe_dst, MAX_SIZE);

                // if(!pipe_exist){
                //     fprintf(stderr, "%s", pipe_dst);
                //     memset(pipe_dst, 0, MAX_SIZE);
                //     Argc_pipe = 0;
                // }
                
                // // fprintf(stderr, "%s", pipe_dst);

                // else{
                //     // fprintf(stderr, "Yes\n");
                //     // replace_n(pipe_dst);
                //     // Argc_pipe = _parseLine(pipe_dst, Argv_pipe, " ");
                //     // fprintf(stderr, "from pipe [%s] get %d args\n", pipe_dst, Argc_pipe);
                //     pipe_exist = false;
                //     // memset(pipe_dst, 0, MAX_SIZE);
                //     received = true;
                // }
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

    free(Argv);
    free(Argv_pipe);
    return 0;
}