#include <unistd.h>
#include <stdio.h>

int main(int argc, char** argv){
    char buf[1024] = "";
    if(chdir("P1") == -1){
        perror("chdir error");
    }

    getcwd(buf, 1024);

    printf("pwd:%s\n", buf);


}