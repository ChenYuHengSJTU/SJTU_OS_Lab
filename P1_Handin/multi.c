/*
This file is the source code of the excutable nulti.
It will call nulti_static or multi_random depending on the input.
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]){
    if(argc == 1){
        if(fork() == 0){
            execvp("./multi_static", argv);
            perror("exec error");
        }
        else{
            wait(NULL);
            exit(0);
        }
    }
    else if(argc == 2){
        if(fork() == 0){
            execvp("./multi_random", argv);
            perror("exec error");
        }
        else{
            wait(NULL);
            exit(0);
        }
    }
    else{
        printf("Command line error[1]\nUsage: ./multi\n       ./multi <size>\n");
        exit(1);
    }
    return 0;
}