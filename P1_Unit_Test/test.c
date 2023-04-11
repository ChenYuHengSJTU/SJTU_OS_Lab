#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stddef.h>
#include <stdbool.h>

int main(){
    // char* argv[] = {"cd", "..", NULL};
    // execvp("cd", argv);
    // char* argv2[] = {"pwd", NULL};
    // execvp("pwd", argv2);


    char* argv[] = {"echo", "shell.c", ">", "./test.txt"};
    execvp("echo", argv);

    return 0;
}