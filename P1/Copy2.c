#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/time.h>


#define FileNameSize 256

char inputfile[FileNameSize];
char outputfile[FileNameSize];
int bufsize;
// char* buf;
int input, output;
int fd[2];
// int* sz;

// void Copy(){
//     if(pipe(fd)){
//         printf("Some error occur whiling trying to create a pipe\n");
//         exit(1);
//     }
//     char buf[bufsize];
//     memset(buf, 0, bufsize);

//     if(fork() == 0){
//         // child process reads file and writes to the pipe
//         close(fd[0]);
//         int len;

//         if((len = read(input, buf, bufsize)) == 0){
//             close(fd[1]);
//             exit(0);
//         }
//         // buf[len] = -1;
//         // buf[len + 1] = '\n';
//         // buf[len - 1] = '\0';
//         // *sz = len;
//         write(fd[1], buf, len);
//         close(fd[1]);
//         exit(0);
//     }
//     else{
//         close(fd[1]);
//         wait((int*)0);

//         int len;

//         // printf("sz: %d\n", *sz);

//         if((len = read(fd[0], buf, bufsize)) == 0){
//             close(fd[0]);
//             exit(0);
//         }
//         write(output, buf, bufsize);
//         close(fd[0]);
//         Copy();
//     }
// }


int main(int argc, char* argv[]){
    // sz = (int*)malloc(sizeof(int));
    
    if(argc <= 1){
        printf("Too few arguments\nTry: ./Copy <InputFile> <OutputFile> <BufferSize>\n");
        exit(1);
    }

    if(strlen(argv[1]) >= FileNameSize){
        printf("Inputfile name too long\n");
        exit(1);
    }

    if(argc > 2 && strlen(argv[2]) >= FileNameSize){
        printf("Outputfile name too long\n");
        exit(1);
    }

    if(argc > 3) bufsize = atoi(argv[3]);

    input = open(argv[1], O_RDONLY);
    // remember to set the mode , otherwise the filesystem will fail to open the .out
    output = open(argv[2], O_WRONLY | O_CREAT ,S_IRUSR | S_IWUSR);


    assert(input != -1);
    assert(output != -1);

    // buf = malloc(bufsize);
    // assert(buf != NULL);

    // 
    struct timeval tv1, tv2;
    struct timezone tz1, tz2;

    gettimeofday(&tv1, &tz1);

    // if(fork() == 0){
    //     Copy();
    //     exit(0);
    // }
    // else{
    //     wait(NULL);
    //     close(fd[0]);
    //     close(fd[1]);
    // }
    char buf[bufsize];
    memset(buf, 0, bufsize);

    if(pipe(fd) == -1){
        perror("pipe error");
    }

    if(fork() == 0){
        // child process reads file and writes to the pipe
        close(fd[0]);
        int len;

        while((len = read(input, buf, bufsize)) != 0){
            write(fd[1], buf, len);
            // close(fd[1]);
            // exit(0);
        }
        // buf[len] = -1;
        // buf[len + 1] = '\n';
        // buf[len - 1] = '\0';
        // *sz = len;
        // write(fd[1], buf, len);
        close(fd[1]);
        close(input);
        close(output);
        exit(0);
    }else{
        wait(NULL);
    }

    if(fork() == 0){
        // child process reads file and writes to the pipe
        close(fd[1]);
        int len;

        while((len = read(fd[0], buf, bufsize)) != 0){
            write(output, buf, len);
            // close(fd[1]);
            // exit(0);
        }
        // buf[len] = -1;
        // buf[len + 1] = '\n';
        // buf[len - 1] = '\0';
        // *sz = len;
        // write(fd[1], buf, len);
        close(fd[0]);
        close(input);
        close(output);
        exit(0);
    }else{
        close(fd[1]);
        close(fd[0]);
        wait(NULL);
    }

    gettimeofday(&tv2, &tz2);

    printf("Time consumed: %lu\n",tv2.tv_usec - tv1.tv_usec);

    close(input);
    close(output);
    // free(buf);

    return 0;
}