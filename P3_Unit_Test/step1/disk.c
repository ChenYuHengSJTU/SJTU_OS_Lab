#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<stdint.h>
#include<stdbool.h>
#include<assert.h>
#include<string.h>
#include<time.h>


#define FILENAMESIZE 128
#define TRACKSIZE 256
#define DISKSIZE cylinder * sector_per_cylinder * TRACKSIZE

static int cylinder = 0, track_delay = 0, sector_per_cylinder = 0;
char disk_storage_filename[FILENAMESIZE];

char* disk = NULL;
char RWData[TRACKSIZE];

struct timespec to_wait, remaining;

struct parameter{
    int cylinder;
    int sector;
    // char* Data;
    char Data[TRACKSIZE];
} Parameter;

uint64_t P[3];
FILE* fp = NULL, *disk_log = NULL;

void TrackDelay(){
    assert(nanosleep(&to_wait, &remaining) == 0);
}

int parseParameter(char* cmd_line){
    // printf("%s\n", cmd_line);
    char *token;
    int cnt = 0;
    token = strtok(cmd_line, " ");
    // assert(token != NULL);
    if(!token)
        return cnt;
    cnt++;
    Parameter.cylinder = atoi(token);
    // printf("%s\n", token);
    // printf("1");
    token = strtok(NULL, " ");
    // assert(token != NULL);
    if(!token)
        return cnt;
    cnt++;
    Parameter.sector = atoi(token);
    // printf("token: %s\n", token);
    // printf("2");
    token = strtok(NULL, "\n");
    // assert(token != NULL);
    if(!token)
        return cnt;
    cnt++;
    // if(token == NULL)
        // return;
    // printf("token: %s\n", token);
    // token[strlen(token) - 1] = '\0';
    strncpy(Parameter.Data, token, TRACKSIZE);
    // Parameter.Data = token;
    // printf("%s", Parameter.Data);
    return cnt;
}

void ReadDisk(int _cylinder, int _sector, char* Data){
    // assert(_cylinder >= 0 && _sector >=0 && _cylinder < cylinder && _sector < sector_per_cylinder);
    if(!(_cylinder >= 0 && _sector >=0 && _cylinder < cylinder && _sector < sector_per_cylinder)){
        printf("\033[31mNO\033[0m\n");
        // fprintf(disk_log, "NO\n");
        return;
    }
    printf("\033[32mYES\033[0m\n");
    
    int offset = _cylinder * sector_per_cylinder * TRACKSIZE + _sector * TRACKSIZE; 
    // int offset = cylinder * sector_per_cylinder * TRACKSIZE + sector * TRACKSIZE + (cylinder - 1) * (sector_per_cylinder - 1) + sector - 1; 
    // snprintf(Data, TRACKSIZE, "%s", disk + offset);
    // printf("read offset: %d\n", offset);
    fprintf(disk_log ,"read offset: %d\n", offset);
    fseek(fp, offset, SEEK_SET);

    TrackDelay();

    assert(ferror(fp) == 0);
    // char data[32];
    int n = fread(Data, 1, TRACKSIZE, fp);
    // int n = fread(Data, 1, TRACKSIZE, fp);
    assert(feof(fp) == 0);
    assert(ferror(fp) == 0);
    // assert(feof(fp) == 0 && ferror(fp) == 0);
    // printf("sizeof(Data): %ld read ret: %d\n", strlen(Data), n);
}

void WriteDisk(int _cylinder, int _sector, char* Data){
    // assert(_cylinder >= 0 && _sector >=0 && _cylinder < cylinder && _sector < sector_per_cylinder);
    if(!(_cylinder >= 0 && _sector >=0 && _cylinder < cylinder && _sector < sector_per_cylinder)){
        printf("\033[31mNO\033[0m\n");
        // fprintf(disk_log, "NO\n");
        return;
    }
    printf("\033[32mYES\033[0m\n");

    int offset = _cylinder * sector_per_cylinder * TRACKSIZE + _sector* TRACKSIZE; 
    assert(offset < DISKSIZE);
    // snprintf(disk + offset, TRACKSIZE, "%s", Data);
    fprintf(disk_log ,"write offset: %d\n", offset);
    // printf("write offset: %d\n", offset);
    fseek(fp, offset, SEEK_SET);

    TrackDelay();

    int n = fwrite(Data, 1, strlen(Data), fp);
    assert(ferror(fp) == 0);
    if(n < strlen(Data)){
        printf("Write error: %d bytes to write but only write %d bytes\n", strlen(Data), n);
        exit(0);
    }
}

void Check(){
    fseek(fp, 0, SEEK_SET);
    assert(ferror(fp) == 0);
    // printf("Check:\n");
    for(int i = 0;i < cylinder;++i){
        printf("Cylinder %d:\n", i);
        for(int j = 0;j < sector_per_cylinder;++j){
            printf("Sector %d:\n", j);
            int rest = TRACKSIZE;
            while(rest >= 0){
                memset(RWData, 0, sizeof(RWData));
                int n = fread(RWData, 1, 32, fp);
                if(n == 0)
                    break;
                if(n!=32){
                    printf("Error: read %d bytes\n", n);
                    exit(0);
                }
                printf("%s\n", RWData);
                rest -= 32;
            }
        }
        printf("\n");
    }
}

void Disk_Working(){
    // printf("Disk is working\n");
    char cmd_line[DISKSIZE];    
    int argc = 0;
    while(1){
        memset(cmd_line, 0 , sizeof(cmd_line));
        printf(">> ");
        fgets(cmd_line, DISKSIZE, stdin);
        // fprintf(disk_log, "\033[31m%s:\033[0m\n", cmd_line);
        // \033[31mHello World\033[0m\n
        // printf("Readin:%s", cmd_line);
        if(cmd_line[0] == 'E'){
            // use fgets something to pay attention to
            fprintf(disk_log, "\033[31m%s\033[0m", cmd_line);
            fprintf(disk_log, "Goodbye\n");
            printf("Goodbye\n");
            assert(strcmp(cmd_line, "E\n") == 0);
            exit(0);
        }
        else if(cmd_line[0] == 'I'){
            fprintf(disk_log, "\033[35m%s\033[0m", cmd_line);
            printf("\tCylinder: %d\tSector: %d\n", cylinder, sector_per_cylinder);
            fprintf(disk_log ,"\tCylinder: %d\tSector: %d\n", cylinder, sector_per_cylinder);
            // fflush(stdout);
        }
        else if(cmd_line[0] == 'C'){
            fprintf(disk_log, "\033[33m%s\033[0m", cmd_line);
            Check();
        }
        else {
            if(cmd_line[1] != ' '){
                printf("Error: wrong command\n");
                continue;
            }
            switch(cmd_line[0]){
                case 'R':
                    // printf("R\n");
                    fprintf(disk_log, "\033[32m%s\033[0m\t", cmd_line);
                    argc = parseParameter(&cmd_line[2]);
                    if(argc != 2){
                        printf("Error: wrong R command used...\nR c s\n");
                        fprintf(disk_log ,"Error: wrong R command used...\n\tR c s\n");
                        continue;
                    }
                    fprintf(disk_log ,"Read from disk c %d s %d ", Parameter.cylinder, Parameter.sector);
                    // printf("Read disk c %d s %d\n", Parameter.cylinder, Parameter.sector);
                    ReadDisk(Parameter.cylinder, Parameter.sector, Parameter.Data);
                    printf("%s\n", Parameter.Data);
                    memset(Parameter.Data, 0, TRACKSIZE);
                    break;
                case 'W':
                    fprintf(disk_log, "\033[34m%s\033[0m\t", cmd_line);
                    argc = parseParameter(&cmd_line[2]);
                    fprintf(disk_log ,"Write to disk c %d s %d ", Parameter.cylinder, Parameter.sector);
                    // printf("Write to disk c %d s %d\n", Parameter.cylinder, Parameter.sector);
                    if(argc != 3){
                        fprintf(disk_log ,"Error: wrong W command used...\nW c s data\n");
                        printf("Error: wrong W command used...\n\tW c s data\n");
                        continue;
                    }
                    WriteDisk(Parameter.cylinder, Parameter.sector, Parameter.Data);
                    memset(Parameter.Data, 0, TRACKSIZE);
                    break;
                default:
                    printf("\033[34mError: wrong command: %s\033[0m", cmd_line);
                    fprintf(disk_log ,"\033[34mError: wrong command: %s\033[0m", cmd_line);
                    break;
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if(argc != 5){
        printf("Usage: ./disk <track to track delay> <number of cylinders> <number of sectors per cylinder> <disk file>\n");
        exit(1);
    }

    setbuf(stdout, NULL);
    cylinder = atoi(argv[1]);
    sector_per_cylinder = atoi(argv[2]);
    track_delay = atoi(argv[3]);
    // printf("%s\n", argv[4]);
    strcpy(disk_storage_filename, argv[4]);
    to_wait.tv_nsec = track_delay * 1000000;
    to_wait.tv_sec = 0;
    remaining.tv_nsec = 0;
    remaining.tv_sec = 0;

    fp = fopen(disk_storage_filename, "w+");
    disk_log = fopen("disk.log", "w+");
    setbuf(disk_log, NULL);
    setbuf(fp, NULL);
    if(fp == NULL){
        printf("Error: cannot open file %s\n", disk_storage_filename);
        exit(1);
    }
    printf("The disk file %s is created\n", disk_storage_filename);

    setbuf(fp, NULL);

    if(disk_log == NULL){
        printf("Error: cannot open file disk.log\n");
        // exit(1);
    }
    printf("The working log will be put into disk.log\n");

    fseek(fp, 0, SEEK_SET);

    disk = (char*)malloc(DISKSIZE);
    if(disk == NULL){
        printf("Error: cannot allocate memory for disk\n");
        exit(1);
    }

    printf("Disk size: %d\n", DISKSIZE);
    memset(disk, '\a', DISKSIZE);

    fseek(fp, 0, SEEK_SET);
    printf("initialize %d bytes successfully\n", fwrite(disk, 1 , DISKSIZE, fp));
    
    Disk_Working();

    // printf("Disk is created\n");
    fclose(fp);
    return 0;
}