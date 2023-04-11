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

int main(int argc, char* argv[]){
    // sz = (int*)malloc(sizeof(int));
    
    if(argc == 4) bufsize = atoi(argv[3]);
    else{
        if(argc < 4){
            fprintf(stderr, "Too few arguments...\nUsage:./Copy <InputFile> <OutputFile> <BufferSize>\n");
            exit(1);
        }
        else{
            fprintf(stderr, "Too many arguments...\nUsage:./Copy <InputFile> <OutputFile> <BufferSize>\n");
            exit(1);
        }
    }

    input = open(argv[1], O_RDONLY);
    // remember to set the mode , otherwise the filesystem will fail to open the .out
    output = open(argv[2], O_WRONLY | O_CREAT ,S_IRUSR | S_IWUSR);


    assert(input != -1);
    assert(output != -1);

    struct timeval tv1, tv2;
    struct timezone tz1, tz2;

    gettimeofday(&tv1, &tz1);

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
        }

        close(fd[1]);
        close(input);
        close(output);
        exit(0);
    }else{
        // wait(NULL);
    }

    if(fork() == 0){
        // child process reads file and writes to the pipe
        close(fd[1]);
        int len;

        while((len = read(fd[0], buf, bufsize)) != 0){
            write(output, buf, len);
        }
        write(output, "\n", 1);
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

    printf("Time consumed: %ld\n",1000000 * (tv2.tv_sec - tv1.tv_sec) + tv2.tv_usec - tv1.tv_usec);

    close(input);
    close(output);
    // free(buf);

    long time_consumed = 1000000 * (tv2.tv_sec - tv1.tv_sec) + tv2.tv_usec - tv1.tv_usec;
    FILE* result = fopen("./copy_time.txt", "a");
    if(result == NULL){
        perror("open file");
        // exit(0);
    }
    else{
        fprintf(result, "%lu\n", time_consumed);
        fclose(result);
    }

    return 0;
}