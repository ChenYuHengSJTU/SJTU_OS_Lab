#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<stdint.h>
#include<stdbool.h>
#include<assert.h>
#include<string.h>

#define FILENAMESIZE 128
#define TRACKSIZE 256
#define DISKSIZE cylinder * sector_per_cylinder * TRACKSIZE

static int cylinder = 0, track_delay = 0, sector_per_cylinder = 0;
char disk_storage_filename[FILENAMESIZE];

char* disk = NULL;
char RWData[TRACKSIZE];

struct parameter{
    int cylinder;
    int sector;
    char* Data;
    // char writeData[TRACKSIZE];
} Parameter;

uint64_t P[3];
FILE* fp = NULL, *log = NULL;

void parseParameter(char* cmd_line){
    // printf("%s\n", cmd_line);
    char *token;
    // token = strtok(cmd_line, " "); // split the string using space, comma, dot, and exclamation mark as delimiters
    // int i = 0;
    // while (token != NULL) {
    //     // printf("%s\n", token);
    //     assert(token != NULL);
    //     assert(strcmp(token, " ") != 0 && strcmp(token, "\n") != 0 && strcmp(token, "\0") != 0);
    //     token = strtok(NULL, " .");
    //     P[i++] = atoi(token);
    // }
    // // printf("%d\n", i);
    // printf("%d %d %d\n", P[0], P[1], P[2]);
    // Parameter.cylinder = P[0];
    // Parameter.sector = P[1];
    // // Parameter.Data = (char*)P[2];
    // printf("parse command line end\n");
    // return 0;

    token = strtok(cmd_line, " "); // split the string using space, comma, dot, and exclamation mark as delimiters
    assert(token != NULL);
    Parameter.cylinder = atoi(token);
    // printf("%s\n", token);
    // printf("1");
    token = strtok(NULL, " ");
    assert(token != NULL);
    Parameter.sector = atoi(token);
    // printf("2");
    token = strtok(NULL, " ");
    // assert(token != NULL);
    if(token == NULL)
        return;
    Parameter.Data = token;
    printf("%s", Parameter.Data);
    // printf("3");
    // assert(token != NULL);
    // printf("4");
    // Parameter.Data = strtok(NULL, " ");
    return 0;
}

void WriteDisk(int cylinder, int sector, char* Data){
    int offset = cylinder * sector_per_cylinder * TRACKSIZE + sector * TRACKSIZE + (cylinder - 1) * (sector_per_cylinder - 1) + sector - 1; 
    // snprintf(disk + offset, TRACKSIZE, "%s", Data);
    fseek(fp, offset, SEEK_SET);
    fwrite(Data, 1, TRACKSIZE, fp);
}

void Disk_Working(){
    // printf("Disk is working\n");
    char cmd_line[DISKSIZE];    
    while(1){
        memset(cmd_line, 0 , sizeof(cmd_line));
        fgets(cmd_line, DISKSIZE, stdin);
        // printf("Readin:%s", cmd_line);
        if(cmd_line[0] == 'E'){
            // use fgets something to pay attention to
            assert(strcmp(cmd_line, "E\n") == 0);
            exit(0);
        }
        else if(cmd_line[0] == 'I'){
            printf("Cylinder: %d\tSector: %d\n", cylinder, sector_per_cylinder);
            // fflush(stdout);
        }
        else {
            parseParameter(&cmd_line[2]);
            switch(cmd_line[0]){
                case 'R':
                    // printf("R\n");
                    printf("Read disk c %d s %d\n", Parameter.cylinder, Parameter.sector);
                    break;
                case 'W':
                    printf("Write to disk c %d s %d\n", Parameter.cylinder, Parameter.sector);
                    WriteDisk(Parameter.cylinder, Parameter.sector, Parameter.Data);
                    break;
                default:
                    printf("Error: wrong command\n");
                    break;
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if(argc != 5){
        printf("Usage: ./disk <disk file> <block size> <number of blocks> <access pattern>\n");
        exit(1);
    }

    setbuf(stdout, NULL);

    cylinder = atoi(argv[1]);
    sector_per_cylinder = atoi(argv[2]);
    track_delay = atoi(argv[3]);
    // printf("%s\n", argv[4]);
    strcpy(disk_storage_filename, argv[4]);

    fp = fopen(disk_storage_filename, "w");
    log = fopen("disk.log", "w");
    if(fp == NULL){
        printf("Error: cannot open file %s\n", disk_storage_filename);
        exit(1);
    }
    printf("The disk file %s is created\n", disk_storage_filename);

    if(log == NULL){
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

    memset(disk, 0, DISKSIZE);
    memset(RWData, 'x', TRACKSIZE);
    // fwrite(disk, 1 , DISKSIZE, fp);
    // printf("%s\n", disk);
    
    for(int i = 0; i < cylinder; i++){
        int offset = i * sector_per_cylinder * TRACKSIZE;
        for(int j = 0;j < sector_per_cylinder; j++){
            fwrite(disk + i * sector_per_cylinder * TRACKSIZE + j * TRACKSIZE, 1, TRACKSIZE, fp);
            fprintf(fp, " ");
            // printf("%.*s", TRACKSIZE, disk + i * sector_per_cylinder * TRACKSIZE + j * TRACKSIZE);
            // printf(" ");
            // snprintf(disk + offset, TRACKSIZE, "%s", RWData);
            // offset += TRACKSIZE;
            // if(j == sector_per_cylinder - 1)
            //     sprintf(disk + offset + 1,"\n");
            // else{
            //     sprintf(disk + offset + 1," ");
            //     offset += 1;
            // }
        }
        fprintf(fp, "\n");
        // printf("\n");
        // sprintf(disk);
    }
    // printf("%s\n", disk);
    Disk_Working();

    printf("Disk is created\n");
    fclose(fp);
    return 0;
}