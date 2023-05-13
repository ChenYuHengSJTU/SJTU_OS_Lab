#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<stdint.h>
#include<stdbool.h>
#include<assert.h>
#include<string.h>
#include<time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define FILENAMESIZE 128
#define TRACKSIZE 256
#define DISKSIZE cylinder * sector_per_cylinder * TRACKSIZE
#define MIN(x, y) (x) < (y) ? (x) : (y)

static int cylinder = 0, track_delay = 0, sector_per_cylinder = 0;
char disk_storage_filename[FILENAMESIZE];

char* disk = NULL;
char RWData[TRACKSIZE + 1];

int socket_fd, client_fd;
struct sockaddr_in server_addr, client_addr;
socklen_t client_addr_len;

struct parameter{
    int cylinder;
    int sector;
    // char* Data;
    char Data[TRACKSIZE];
} Parameter;

uint64_t P[3];
FILE* fp = NULL, *disk_log = NULL;

void parseParameter(char* cmd_line){
    // printf("%s\n", cmd_line);
    char *token;

    token = strtok(cmd_line, " ");
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
    token[strlen(token) - 1] = '\0';
    strncpy(Parameter.Data, token, TRACKSIZE);
    // Parameter.Data = token;
    // printf("%s", Parameter.Data);
    return 0;
}

void ReadDisk(int _cylinder, int _sector, char* Data, size_t offset, size_t n){
    assert(_cylinder >= 0 && _sector >=0 && _cylinder < cylinder && _sector < sector_per_cylinder);
    int _offset = _cylinder * sector_per_cylinder * TRACKSIZE + _sector * TRACKSIZE + offset;
    // int offset = cylinder * sector_per_cylinder * TRACKSIZE + sector * TRACKSIZE + (cylinder - 1) * (sector_per_cylinder - 1) + sector - 1; 
    // snprintf(Data, TRACKSIZE, "%s", disk + offset);
    // printf("read offset: %d\n", _offset);
    fseek(fp, _offset, SEEK_SET);
    assert(ferror(fp) == 0);
    // char data[32];
    int nn = fread(Data, 1, TRACKSIZE, fp);
    // int n = fread(Data, 1, TRACKSIZE, fp);
    assert(feof(fp) == 0);
    assert(ferror(fp) == 0);
    // assert(feof(fp) == 0 && ferror(fp) == 0);
    // printf("sizeof(Data): %ld read ret: %d\n", strlen(Data), nn);
    // printf("Data: %s\n", Data);
}

void WriteDisk(int _cylinder, int _sector, char* Data, size_t offset, size_t n){
    assert(_cylinder >= 0 && _sector >=0 && _cylinder < cylinder && _sector < sector_per_cylinder);
    int _offset = _cylinder * sector_per_cylinder * TRACKSIZE + _sector* TRACKSIZE + offset;
    assert(_offset < DISKSIZE);
    // snprintf(disk + offset, TRACKSIZE, "%s", Data);
    // printf("write offset: %d\n", _offset);
    fseek(fp, _offset, SEEK_SET);
    int nn = fwrite(Data, 1, TRACKSIZE, fp);
    assert(ferror(fp) == 0);
    if(nn < strlen(Data)){
        printf("Write error: %d bytes to write but only write %d bytes\n", strlen(Data), nn);
        // exit(0);
    }
    // printf("sizeof(Data): %ld write ret: %d\n", strlen(Data), nn);
    // printf("Data: %s\n", Data);
}

void Check(){
    fseek(fp, 0, SEEK_SET);
    assert(ferror(fp) == 0);
    printf("Check:\n");
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

void ParseArgs(char* args, uint64_t* block_num, char* data, size_t* n, size_t* offset, int* flag){
    // if(strlen(args) == 0)
        // return;
    char* token;
    token = strtok(args, " ");
    assert(token != NULL);
    *flag = atoi(token);
    // printf("flag: %d\n", *flag);
    token = strtok(NULL, " ");
    assert(token != NULL);    
    *block_num = atoi(token);
    // printf("block_num: %ld\n", *block_num);
    // token = strtok(NULL, " ");
    // assert(token != NULL);
    // printf("token: %s\n", token);
    // if(*flag){
    //     // printf("token: %s\n", token);
    //     token = strtok(NULL, " ");
    //     assert(token != NULL);
    //     memcpy(data, token, TRACKSIZE);
    //     printf("data: %s\n", data);
    // }
    // printf("token: %s\n", token);
    token = strtok(NULL, " ");
    assert(token != NULL);
    *n = atoi(token);
    // printf("n: %ld\n", *n);
    token = strtok(NULL, " ");
    assert(token != NULL);
    *offset = atoi(token);
    // printf("offset: %ld\n", *offset);
    // token = strtok(NULL, " ");
    // if(!token)
    //     return;
    // assert(token != NULL);
    // memcpy(data, token, TRACKSIZE);
    memcpy(data, args + TRACKSIZE, TRACKSIZE);
}

void AccessDiskAux(uint64_t block_num, char* data, size_t n, size_t offset, int flag){
    int _cylinder = block_num / sector_per_cylinder, _sector = block_num % sector_per_cylinder;
    // printf("%d %s %d %d %d\n", block_num, data, n, offset, flag);
    printf("cylinder: %d sector: %d\n", _cylinder, _sector);
    if(flag){   
        WriteDisk(_cylinder, _sector, data, offset, n);
    }
    else{
        ReadDisk(_cylinder, _sector, data, offset, n);
        send(client_fd, data, TRACKSIZE + 1, 0);
    }
}

void Disk_Working(){
    // printf("Disk is working\n");
    char cmd_line[DISKSIZE];    
    uint64_t block_num;
    size_t offset, n;
    int flag;
    while(1){
        client_fd = accept(socket_fd, (struct sockaddr*)&client_addr, &client_addr_len);
        assert(client_fd != -1);
        printf("client_fd: %d\n", client_fd);

        if(fork() == 0){
        while(1){
        
        memset(cmd_line, 0 , sizeof(cmd_line));
        // fgets(cmd_line, DISKSIZE, stdin);
        // send(client_fd, ">> ", 3, 0);
        // char str[32] = "";
        // read(client_fd, cmd_line, TRACKSIZE * 2 + 1);
        recv(client_fd, cmd_line, TRACKSIZE * 2 + 1, 0);
        // recv(client_fd, str, 32, 0);
        // printf("Readin:%s\n", str);
        printf("Readin:%s\n", cmd_line);
        // printf("begin test:");
        // for(int i = 0;i < TRACKSIZE * 2;++i){
            // printf("%c", cmd_line[i]);
        // }
        // printf("end test\n");

        // if(cmd_line[0] == 'E'){
        //     // use fgets something to pay attention to
        //     close(client_fd);
        //     assert(strcmp(cmd_line, "E\n") == 0);
        //     exit(0);
        // }
        // else if(cmd_line[0] == 'I'){
        //     printf("Cylinder: %d\tSector: %d\n", cylinder, sector_per_cylinder);
        //     // fflush(stdout);
        // }
        // else if(cmd_line[0] == 'C'){
        //     Check();
        // }
        // else {
            memset(RWData, 0, sizeof(RWData));
            ParseArgs(cmd_line, &block_num, RWData, &n, &offset, &flag);
            // memset(RWData, 0, sizeof(RWData));
            // printf("p time: %s\n", RWData + 120);
            AccessDiskAux(block_num, RWData, n, offset, flag);
        //     if(cmd_line[1] != ' '){
        //         printf("Error: wrong command\n");
        //         continue;
        //     }
        //     parseParameter(&cmd_line[2]);
        //     switch(cmd_line[0]){
        //         case 'R':
        //             // printf("R\n");
        //             printf("Read disk c %d s %d\n", Parameter.cylinder, Parameter.sector);
        //             ReadDisk(Parameter.cylinder, Parameter.sector, Parameter.Data, 0, TRACKSIZE);
        //             printf("%s\n", Parameter.Data);
        //             memset(Parameter.Data, 0, TRACKSIZE);
        //             break;
        //         case 'W':
        //             printf("Write to disk c %d s %d\n", Parameter.cylinder, Parameter.sector);
        //             WriteDisk(Parameter.cylinder, Parameter.sector, Parameter.Data, 0, TRACKSIZE);
        //             memset(Parameter.Data, 0, TRACKSIZE);
        //             break;
        //         default:
        //             printf("Error: wrong command\n");
        //             break;
        //     }
        // }
        }
        }
        else{
            close(client_fd);
        }
    }
}


int main(int argc, char* argv[]) {
    if(argc != 6){
        printf("Usage: ./disk <disk file> <block size> <number of blocks> <access pattern>\n");
        exit(1);
    }

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_fd < 0){
        printf("Error: cannot open socket\n");
        exit(1);
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[5]));

    if(bind(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        printf("Error: cannot bind socket\n");
        exit(1);
    }

    if(listen(socket_fd, 5) < 0){
        printf("Error: cannot listen socket\n");
        exit(1);
    }

    client_addr_len = sizeof(client_addr);

    setbuf(stdout, NULL);
    cylinder = atoi(argv[1]);
    sector_per_cylinder = atoi(argv[2]);
    track_delay = atoi(argv[3]);
    // printf("%s\n", argv[4]);
    strcpy(disk_storage_filename, argv[4]);

    fp = fopen(disk_storage_filename, "w+");
    disk_log = fopen("disk.log", "w+");
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
    memset(disk, '\0', DISKSIZE);
    // memset(RWData, 'x', TRACKSIZE);
    // fwrite(disk, 1 , DISKSIZE, fp);
    // printf("%s\n", disk);
    
    // for(int i = 0; i < cylinder; i++){
    //     int offset = i * sector_per_cylinder * TRACKSIZE;
    //     for(int j = 0;j < sector_per_cylinder; j++){
    //         // fwrite(disk + i * sector_per_cylinder * TRACKSIZE + j * TRACKSIZE, 1, TRACKSIZE, fp);
    //         // fprintf(fp, " ");
    //         // printf("%.*s", TRACKSIZE, disk + i * sector_per_cylinder * TRACKSIZE + j * TRACKSIZE);
    //         // printf(" ");
    //         // snprintf(disk + offset, TRACKSIZE, "%s", RWData);
    //         // offset += TRACKSIZE;
    //         // if(j == sector_per_cylinder - 1)
    //         //     sprintf(disk + offset + 1,"\n");
    //         // else{
    //         //     sprintf(disk + offset + 1," ");
    //         //     offset += 1;
    //         // }
    //     }
    //     // fprintf(fp, "\n");
    //     // printf("\n");
    //     // sprintf(disk);
    // }
    fseek(fp, 0, SEEK_SET);
    printf("write %d bytes initially\n", fwrite(disk, 1 , DISKSIZE, fp));
    // fwrite(disk, 1 , DISKSIZE, fp);

    // printf("%s\n", disk);
    Disk_Working();

    printf("Disk is created\n");
    fclose(fp);
    return 0;
}