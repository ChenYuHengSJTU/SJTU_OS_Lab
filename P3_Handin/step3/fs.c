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

#define FILENAMESIZE 100

// use ifdef to switch between different disk and heap
// To make my programs more elegant, I should implement functions using the same primitives with corresponding API
// maybe the duplicating work is that I should implement the free block list twice,maybe I should re-consider this 
//TODO all the disk operation must correspond to those realized in disk.c
// TODO all the fs operation , including finding block... everything excluding disk writing or reading is implemented in this file

// Now I want to treat the file and dir the same way 
// they both use inode
// one inode use one block 
// here is the inode structure
// the direct , second-indirect, third-indirect 8 + 3 + 1

#define DIRECT 8
#define SECOND_INDIRECT 4
#define FILENAME_SIZE 100
#define BLOCK_SIZE 256 
#define DIRECT_BLOCK 8
#define SECOND_INDIRECT_BLOCK 4 * 32
#define CMD_MAX_LEN 256
#define ENTRY_NUM 32
// this can be calculated from the max amount of the disk
#define MAX_DIR_DEPTH 128
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
// \033[34mThis is blue color text!\033[0m\n
// #define BLUE(text) "\033[34m # text # \033[0m\n"

const char* error_message[]={
    "Format command error...\nUsage: f\n",
    "Make File command error...\nUsage: mk f\n",
    "Make Dir command error...\nUsage: mkdir d\n",
    "Remove File command error...\nUsage: rm f\n",
    "Change Dir command error...\nUsage: cd path\n",
    "Remove Dir command error...\nUsage: rmdir d\n",
    "List Dir command error...\nUsage: ls\n",
    "Cat File command error...\nUsage: cat f\n",
    "Write File command error...\nUsage: w f l data\n",
    "Insert to File command error...\nUsage: i f pos l data\n",
    "Delete File command error...\nUsage: d f pos l\n",
    "Exit command error...\nUsage: e\n"
};

#define ERR_LEN sizeof(error_message) / sizeof(error_message[0])

// get the args 
// get the args 
int Get_Args(char* cmd, char* args[]){
    // printf("cmd=%s\n", cmd);
    int cnt = 0, tot = 3, flag = 0;
    char p[CMD_MAX_LEN] = "";
    strncpy(p, cmd, CMD_MAX_LEN);
    // char* p = cmd;
    char* tmp = strtok(p, " ");
    // char* tmp = strsep(&p, " ");
    if(tmp == NULL){
        // printf("tmp is NULL\n");
        return -1;
    }
    if(strcmp(tmp, "w") == 0){
        tot = 2;
        flag = 1;
    }
    else if(strcmp(tmp, "i") == 0){
        flag = 1;
    }
    args[cnt++] = tmp;
    // printf("tot=%d\n", tot);
    while((tmp = strtok(NULL, " ")) != NULL && tot > 0){
    // while((tmp = strsep(&p, " ")) != NULL && tot > 0){
        args[cnt++] = cmd + (size_t)(tmp - p);
        // tmp = strtok(NULL, " ");
        cmd[tmp - p + strlen(tmp)] = '\0';
        tot--;
        // printf("cnt=%d\n", cnt);
    }
    fflush(stdout);
    if(flag && tmp != NULL){
        // printf("meet i or w\n");
        // tmp = strtok(NULL, " ");
        args[cnt++] = cmd + (size_t)(tmp - p);
        // printf("args[cnt - 1]=%s\n", args[cnt - 1]);
    }
    return cnt;
}

struct Inode{
    char filename[FILENAME_SIZE];
    uint64_t file_size;
    // here are only two types of file: dir 1 and file 0
    // 32-bits size for expansion
    uint32_t file_type;
    // permission word , R or W or X 
    // need some more design 
    uint32_t permission;
    // reference for the file 
    // uint64_t link_count;
    // TODO get the time information
    // uint32_t time;
    char Time[32];
    // linking to the file (should be further considered)
    // the sum block number of the file or dir 
    // for dir it is the number of entries
    uint64_t block_num;
    uint64_t direct[8];
    uint64_t second_indirect[4];
    // uint64_t third_indirect;
};

// to store the free block list
struct Block{
    uint64_t block_num;
    struct Block* nxt;
};

struct Block* i_free_block_list = NULL, *i_free_block_tail = NULL, *d_free_block_list = NULL, *d_free_block_tail = NULL;

// use the ptr ?
// struct Inode* Current_Dir;
// TODO maybe the block number of the cur-dir is a better choice
uint64_t Current_Dir;
// useless?
// be used to store the current dir inode for prompt
struct Inode CurrentDir_Inode;
char pwd[FILENAME_SIZE * MAX_DIR_DEPTH];
// struct Inode dir;

// the disk ,we should divide it into blocks too 
// char* disk = NULL;
FILE* fs_log = NULL;
// to share the code 
#define BLOCK_NUM 16384
// #define CYLINDER 50
// #define SEC_PER_CYLINDER 200
// #define DISK_SIZE CYLINDER * SEC_PER_CYLINDER * BLOCK_SIZE
#define DISK_SIZE BLOCK_NUM * BLOCK_SIZE

// we may not need super block and log block here 
#define INODE_BLOCK 0
#define DATA_BLOCK (BLOCK_NUM >> 2)
#define MAX_FILE_SIZE (BLOCK_NUM - DATA_BLOCK) * (BLOCK_SIZE)

// socket 
int client_fd, socket_fd, fs_fd;
struct sockaddr_in server_addr, client_addr, disk_addr;
socklen_t client_len, disk_len;

// char output[MAX_FILE_SIZE];
char output[BLOCK_SIZE * 2];

// some block-related functions
// calculate the physical block num 
uint64_t* GetPhysicalBlockNum(struct Inode* inode, int block_num){
    // return inode->direct[block_num];
    if(block_num < DIRECT_BLOCK){
        return &(inode->direct[block_num]);
    }
    else if(block_num < DIRECT_BLOCK + SECOND_INDIRECT_BLOCK){
        // get the second indirect block num 
        int second_indirect_block_num = inode->second_indirect[(block_num - DIRECT_BLOCK) / ENTRY_NUM];
        // get the block num in the second indirect block 
        int block_num_in_second_indirect = (block_num - DIRECT_BLOCK) % ENTRY_NUM;
        // get the physical block num 
        uint64_t second_indirect[ENTRY_NUM];
        // uint64_t* second_indirect = (uint64_t*)(disk + (INODE_BLOCK + second_indirect_block_num) * BLOCK_SIZE);
        ReadBlock(second_indirect_block_num + INODE_BLOCK, (char*)second_indirect, BLOCK_SIZE, 0, 0);
        return &(second_indirect[block_num_in_second_indirect]);
    }
    else{
        // TODO
        assert(0);
    }
}


// TODO access the disk 
// flag 0 - read, 1 - write
int AccessDisk(int flag, uint64_t block_num, char* data, size_t n, size_t offset){
    // combine them to a char array
    // printf("AccessDisk: %ld %s %ld %ld\n", block_num, data, n, offset);
    // printf("a1 time: %s\n", data + 120);
    char _data[CMD_MAX_LEN + BLOCK_SIZE + 1];
    memset(_data, '\0', CMD_MAX_LEN + BLOCK_SIZE +1);
    if(flag == 0){
        sprintf(_data, "%d %ld %d %d", flag, block_num, n, offset);
    }
    else
        // sprintf(_data, "%d %ld %d %d %s", flag, block_num, n, offset, data);
    {
        sprintf(_data, "%d %ld %d %d", flag, block_num, n, offset);
        memcpy(_data + CMD_MAX_LEN, data, n);
    }
    if(flag){
        fprintf(fs_log ,"WriteDisk: ");
    }
    else{
        fprintf(fs_log, "ReadDisk: ");
    }
    fprintf(fs_log, "%s\n", _data);
    // printf("a2 time: %s\n", data + 120);
    // fflush(stdout);
    // sleep(1);
    // write(fs_fd, _data, BLOCK_SIZE * 2 + 1);
    send(fs_fd, _data, BLOCK_SIZE * 2 + 1, 0);
    if(flag == 0){
        memset(data, 0, BLOCK_SIZE + 1);
        recv(fs_fd, data, BLOCK_SIZE + 1, 0);
        // read(fs_fd, data, BLOCK_SIZE + 1);
    }
}


// write to the block (if can combined with read?)
// block num is the physical block num
// TODO consider the parameter, offset should be a must
// flag means the inode block or the data block
// 0 - inode block, 1 - data block
// the flag parameter is somewhat useless
// but remain it for further use
int WriteBlock(uint64_t block_num, char* data, size_t n, size_t offset, int flag){
    // get the physical block num 
    // int physical_block_num = GetPhysicalBlockNum(block_num);
    // write to the disk 
    // printf("w time: %s\n", data + 120);
    if(offset >= BLOCK_SIZE){
        sprintf(output+strlen(output), "FAIL: offset is too large\n");
        return -1;
    }
    // if(!flag)
        // memcpy(disk + (INODE_BLOCK + block_num) * BLOCK_SIZE + offset, data, n);
    // else 
        // memcpy(disk + (DATA_BLOCK + block_num) * BLOCK_SIZE + offset, data, n);
    AccessDisk(1, block_num, data, n, offset);
}

// read from the block
int ReadBlock(uint64_t block_num, char* data, size_t n, size_t offset, int flag){
    if(offset >= BLOCK_SIZE){
        sprintf(output+strlen(output), "FAIL: offset is too large\n");
        return -1;
    }
    // if(!flag)
        // memcpy(data, disk + (INODE_BLOCK + block_num) * BLOCK_SIZE + offset, n);
    // else 
        // memcpy(data, disk + (DATA_BLOCK + block_num) * BLOCK_SIZE + offset, n);
    char tmp[BLOCK_SIZE] = "";
    AccessDisk(0, block_num, tmp, n, offset);
    fprintf(fs_log, "ReadBlock: %s\n", tmp);
    memcpy(data, tmp, n);
}

// Initialize the Disk 
void Init_Block(){
    fprintf(fs_log, "InitBlock: Init the blocks\n");
    int block_sum = BLOCK_NUM;
    struct Block* new_block = (struct Block*)malloc(sizeof(struct Block));
    i_free_block_list = new_block;
    i_free_block_tail = new_block;
    new_block->block_num = 0;
    for(int i = 1;i < DATA_BLOCK;++i){
        new_block = (struct Block*)malloc(sizeof(struct Block));
        new_block->block_num = i;
        i_free_block_tail->nxt = new_block;
        i_free_block_tail = new_block;
    }

    new_block = (struct Block*)malloc(sizeof(struct Block));
    new_block->block_num = DATA_BLOCK;
    d_free_block_list = new_block;
    d_free_block_tail = new_block;
    for(int i = DATA_BLOCK + 1;i < block_sum;++i){
        new_block = (struct Block*)malloc(sizeof(struct Block));
        new_block->block_num = i;
        d_free_block_tail->nxt = new_block;
        d_free_block_tail = new_block;
    }
    fprintf(fs_log, "InitBlock: Init the blocks successfully\n");
}

void ReturnBlock(){
    fprintf(fs_log, "ReturnBlock: Return the blocks\n");
    struct Block* tmp = i_free_block_list;
    while(tmp != NULL){
        struct Block* nxt = tmp->nxt;
        free(tmp);
        tmp = nxt;
    }
    tmp = d_free_block_list;
    while(tmp != NULL){
        struct Block* nxt = tmp->nxt;
        free(tmp);
        tmp = nxt;
    }
    fprintf(fs_log, "ReturnBlock: Return the blocks successfully\n");
}

// also some functions to maintain the free block list 
// get the free block
uint64_t I_GetFreeBlock(){
    if(i_free_block_list == NULL){
        return -1;
    }
    struct Block* tmp = i_free_block_list;
    i_free_block_list = i_free_block_list->nxt;
    uint64_t ret = tmp->block_num;
    free(tmp);
    fprintf(fs_log, "allocate a inode block: %ld\n", ret);
    // sprintf(output+strlen(output), "allocate a inode block: %ld\n", ret);
    return ret;
}

void I_ReturnBlock(uint64_t block_num){
    struct Block* new_block = (struct Block*)malloc(sizeof(struct Block));
    new_block->block_num = block_num;
    new_block->nxt = NULL;
    i_free_block_tail->nxt = new_block;
    i_free_block_tail = new_block;
    if(i_free_block_list == NULL)
        i_free_block_list = new_block;
    fprintf(fs_log, "return a inode block: %ld\n", block_num);
    // sprintf(output+strlen(output), "return a inode block: %ld\n", block_num);
}

uint64_t D_GetFreeBlock(){
    if(d_free_block_list == NULL){
        return -1;
    }
    struct Block* tmp = d_free_block_list;
    d_free_block_list = d_free_block_list->nxt;
    uint64_t ret = tmp->block_num;
    // assert(tmp != NULL && ret >= DATA_BLOCK);
    free(tmp);
    fprintf(fs_log, "allocate a data block: %ld\n", ret);
    // sprintf(output+strlen(output), "allocate a data block: %ld\n", ret);
    return ret;
}

// return the block to the free block list
void D_ReturnBlock(uint64_t block_num){
    struct Block* new_block = (struct Block*)malloc(sizeof(struct Block));
    new_block->block_num = block_num;
    new_block->nxt = NULL;
    d_free_block_tail->nxt = new_block;
    d_free_block_tail = new_block;
    if(d_free_block_list == NULL)
        d_free_block_list = new_block;
    fprintf(fs_log, "return a data block: %ld\n", block_num);
    // sprintf(output+strlen(output), "return a data block: %ld\n", block_num);
}

void GetTime(char* Time){
    // time_t now;
    // struct tm *local_time;
    // now = time(NULL);
    // local_time = localtime(&now);
    memset(Time, 0, sizeof(uint32_t));
    time_t t = time(NULL);
    struct tm* lt = localtime(&t);
    sprintf(Time, "%d-%d-%d %d:%d:%d", lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday, lt->tm_hour, lt->tm_min, lt->tm_sec);
}

// TODO the duplicating create inode code should be integrated into one function
// to format the fs on the disk 
int Format(char **args){
    fprintf(fs_log, "Format: Start to format the disk\n");
    static bool initialized = false;
    if(initialized){
        sprintf(output+strlen(output), "Format: the disk has been initialized\nNothing to do...\n");
        fprintf(fs_log, "Format: Do nothing since the disk has been initialized\n");
        return -1;
    }
    // Here should create the root dir 
    // disk = (char*)malloc(DISK_SIZE);
    // memset(disk, 0, sizeof(disk));
    // init the free block list
    Init_Block();
    // create the root dir
    // mention that each dir newly created has . and ..
    // the first two inodes is . and ..
    struct Inode root;
    strcpy(root.filename, "/");
    root.file_size = 0;
    root.file_type = 1;
    root.permission = 0;
    // root.link_count = 0;
    root.block_num = 2;
    // memset(root.direct, 0, sizeof(root.direct));
    // memset(root.second_indirect, 0, sizeof(root.second_indirect));
    for(int i = 0;i < DIRECT;++i)
        root.direct[i] = -1;
    for(int i = 0;i < SECOND_INDIRECT;++i)
        root.second_indirect[i] = -1;
    // root.third_indirect = 0;
    // write the root dir to the disk
    // use the unified API 
    // memcpy(disk + INODE_BLOCK * BLOCK_SIZE, &root, sizeof(root));
    Current_Dir = I_GetFreeBlock();
    root.direct[0] = Current_Dir;
    // -1 means no file
    // TODO may 0 is a no-worse choice
    root.direct[1] = Current_Dir;
    GetTime(root.Time);
    // printf("root time: %s\n", root.Time);
    WriteBlock(Current_Dir + INODE_BLOCK, (char*)&root, sizeof(root), 0, 0);
    char block[BLOCK_SIZE + 1] = "";
    printf("root content :%s\n", (char*)&root);
    memcpy(block, &root, sizeof(root));
    printf("root content :%s len: %ld\n", block, strlen(block));
    printf("time: %s\n", block + (root.Time - (char*)&root));
    // sprintf(output+strlen(output), "end formatting\n");
    fprintf(fs_log, "Format: Finish formatting the disk\n");
    // CurrentDir_Inode = (struct Inode*)(disk +(INODE_BLOCK + Current_Dir) * BLOCK_SIZE);
    printf("CurrentDir_Inode: %s\n", CurrentDir_Inode.filename);
    ReadBlock(Current_Dir + INODE_BLOCK, (char*)&CurrentDir_Inode, sizeof(CurrentDir_Inode), 0, 0);
    printf("CurrentDir_Inode: %s\n", CurrentDir_Inode.filename);
}

// add an entry to the dir
void AddEntry(struct Inode* dir, uint64_t inode_num){
    // find the first empty entry
    // sprintf(output+strlen(output), "add entry to %s\n", dir->filename);
    for(int i = 0;i < DIRECT_BLOCK;++i){
        if(dir->direct[i] == -1){
            dir->direct[i] = inode_num;
            dir->block_num++;
            return;
        }
    }
    for(int j = 0;j < 4;++j){
        if(dir->second_indirect[j] == -1){
            dir->second_indirect[j] = D_GetFreeBlock();
            uint64_t indirect_block[ENTRY_NUM];
            // uint64_t* indirect_block = (uint64_t*)(disk + (INODE_BLOCK + dir->second_indirect[j]) * BLOCK_SIZE);
            // uint64_t* indirect_block = (uint64_t*)malloc(BLOCK_SIZE);
            ReadBlock(dir->second_indirect[j] + INODE_BLOCK, (char*)&indirect_block, sizeof(indirect_block), 0, 0);
            indirect_block[0] = inode_num;
            dir->block_num++;
            return;
        }
        // uint64_t* indirect_block = (uint64_t*)(disk + (INODE_BLOCK + dir->second_indirect[j]) * BLOCK_SIZE);
        uint64_t indirect_block[ENTRY_NUM];
        ReadBlock(dir->second_indirect[j] + INODE_BLOCK, (char*)&indirect_block, sizeof(indirect_block), 0, 0);
        for(int i = 0;i < ENTRY_NUM;++i){
            if(indirect_block[i] == -1){
                indirect_block[i] = inode_num;
                dir->block_num++;
                return;
            }
        }
    }
    sprintf(output+strlen(output), "Fail: Current Dir has no space for new entry\n");
    fprintf(fs_log, "Fail: Current Dir has no space for new entry\n");
    return;
}

uint64_t* FindEntry(uint64_t inode_num ,char* entry_name);

// mk f 
// TODO we need to find a inode entry for each file and dir too by traversing the dir-inode
int MakeFile(char **args){
    uint64_t* inode_num = FindEntry(Current_Dir ,args[1]);
    if(inode_num != NULL){
        sprintf(output+strlen(output), "FAIL: File %s already exists\n", args[1]);
        fprintf(fs_log, "FAIL: File %s already exists\n", args[1]);
        return -1;
    }
    struct Inode new_file;
    strcpy(new_file.filename, args[1]);
    new_file.file_size = 0;
    new_file.file_type = 0;
    new_file.permission = 0;
    // new_file.link_count = 0;
    new_file.block_num = 0;
    memset(new_file.direct, 0, sizeof(new_file.direct));
    memset(new_file.second_indirect, 0, sizeof(new_file.second_indirect));
    // new_file.third_indirect = 0;
    // write the new file to the disk
    // use the unified API
    int Inode_num = I_GetFreeBlock();
    if(Inode_num == -1){
        sprintf(output+strlen(output), "FAIL: No enough space to create file!\nDir and File number is up to the limit\n");
        fprintf(fs_log, "FAIL: No enough space to create file! Dir and File number is up to the limit\n");
        return -1;
    }
    GetTime(new_file.Time);
    // write the inode 
    WriteBlock(Inode_num, (char*)&new_file, sizeof(new_file), 0, 0);
    // add the new file to the current dir
    // find the last block
    // int last_block = Current_Dir.block_num;
    // TODO how to use unified API is a difficult here, add file to a dir is the same with add sub-dir to a dir, both them could be implemented by writing blocks 
    // int physical_block = GetPhysicalBlockNum(&Current_Dir, last_block);
    // AddEntry(&Current_Dir, Inode_num);
    struct Inode dir;
    ReadBlock(Current_Dir + INODE_BLOCK, (char*)&dir, sizeof(dir), 0, 0);
    // AddEntry((struct Inode*)(disk + (INODE_BLOCK + Current_Dir) * BLOCK_SIZE), Inode_num);256 
    AddEntry(&dir, Inode_num);
    WriteBlock(Current_Dir + INODE_BLOCK, (char*)&dir, sizeof(dir), 0, 0);
}

// mkdir d 
int MakeDir(char **args){
    uint64_t* inode_num = FindEntry(Current_Dir ,args[1]);
    if(inode_num != NULL){
        sprintf(output+strlen(output), "FAIL: Dir %s already exists\n", args[1]);
        fprintf(fs_log, "FAIL: Dir %s already exists\n", args[1]);
        return -1;
    }
    struct Inode new_dir;
    strcpy(new_dir.filename, args[1]);
    new_dir.file_size = 0;
    new_dir.file_type = 1;
    new_dir.permission = 0;
    // new_dir.link_count = 0;
    new_dir.block_num = 2;
    for(int i = 0;i < DIRECT;++i)
        new_dir.direct[i] = -1;
    for(int i = 0;i < SECOND_INDIRECT;++i)
        new_dir.second_indirect[i] = -1;
    // memset(new_dir.direct, 0, sizeof(new_dir.direct));
    // memset(new_dir.second_indirect, 0, sizeof(new_dir.second_indirect));
    // new_dir.third_indirect = 0;
    // write the new dir to the disk
    // use the unified API
    int Inode_num = I_GetFreeBlock();
    if(Inode_num == -1){
        sprintf(output+strlen(output), "FAIL: No enough space to create dir!\nDir and File number is up to the limit\n");
        fprintf(fs_log, "FAIL: No enough space to create dir! Dir and File number is up to the limit\n");
        return -1;
    }
    // add the new dir to the current dir
    // AddEntry((struct Inode*)(disk + (INODE_BLOCK + Current_Dir) * BLOCK_SIZE), Inode_num);
    struct Inode dir;
    ReadBlock(Current_Dir + INODE_BLOCK, (char*)&dir, sizeof(dir), 0, 0);
    AddEntry(&dir, Inode_num);
    // AddEntry(&new_dir, Inode_num);
    new_dir.direct[0] = Inode_num;
    new_dir.direct[1] = Current_Dir;

    GetTime(new_dir.Time);
    // write the inode
    WriteBlock(Inode_num, (char*)(&new_dir), sizeof(new_dir), 0, 0);
    WriteBlock(Current_Dir + INODE_BLOCK, (char*)&dir, sizeof(dir), 0, 0);
}

// maybe block num of Inode is more useful
uint64_t second_block[ENTRY_NUM];
struct Inode Dir, tmp;
uint64_t* FindEntry(uint64_t inode_num ,char* entry_name){
    printf("Find Entry\n");
    // struct Inode dir, tmp; 
    // struct Inode tmp;
    // dir = (struct Inode*)(disk + (INODE_BLOCK + inode_num) * BLOCK_SIZE);
    ReadBlock(inode_num + INODE_BLOCK, (char*)&Dir, sizeof(Dir), 0, 0);
    if(Dir.file_type == 0){
        sprintf(output+strlen(output), "FAIL: %s is not a dir\n", Dir.filename);
        fprintf(fs_log, "FAIL: %s is not a dir\n", Dir.filename);
        return NULL;
    }
    // memcpy(&dir, disk + (INODE_BLOCK + Current_Dir) * BLOCK_SIZE, sizeof(dir));
    for(int i = 2;i < DIRECT_BLOCK;++i){
        if(Dir.direct[i] == -1)
            continue;
        ReadBlock(Dir.direct[i] + INODE_BLOCK, (char*)&tmp, sizeof(tmp), 0, 0);
        // memcpy(&tmp, disk + (INODE_BLOCK + dir->direct[i]) * BLOCK_SIZE, sizeof(tmp));
        if(strcmp(tmp.filename, entry_name) == 0)
            return (&Dir.direct[i]);
        printf("tmp.filename: %s\n", tmp.filename);
    }
    for(int j = 0;j < SECOND_INDIRECT;++j){
        if(Dir.second_indirect[j] == -1)
            continue;
        // second_block = (uint64_t*)(disk + (INODE_BLOCK + dir.second_indirect[j]) * BLOCK_SIZE);
        ReadBlock(Dir.second_indirect[j] + INODE_BLOCK, (char*)&second_block, sizeof(second_block), 0, 0);
        for(int i = 0;i < ENTRY_NUM;++i){
            if(second_block[i] == -1)
                continue;
            ReadBlock(second_block[i] + INODE_BLOCK, (char*)&tmp, sizeof(tmp), 0, 0);
            // memcpy(&tmp, disk + (INODE_BLOCK + second_block[i]) * BLOCK_SIZE, sizeof(tmp));
            if(strcmp(tmp.filename, entry_name) == 0)
                return (&second_block[i]);
        }
    }
    return NULL;
}

// just return the data block
int FreeFile(struct Inode* file){
    int block_num = file->block_num;
    // sprintf(output+strlen(output), "block_num: %d\n", block_num);
    for(int i = 0;i < block_num;++i){
        D_ReturnBlock(*GetPhysicalBlockNum(file, i));
    }
    file->block_num = 0;
    file->file_size = 0;
}

// TODO should consider the relative path and the absolute path
//  be integrated into the find function
bool EntryExist(char* entry_name){
    return FindEntry(Current_Dir, entry_name) != NULL;
}
// rm f 
int RemoveFile(char **args){
    uint64_t* Inode_num = FindEntry(Current_Dir , args[1]);
    if(Inode_num == NULL){
        sprintf(output+strlen(output), "FAIL: No such file!\nDeletion error!\n");
        fprintf(fs_log, "FAIL: No such file! Deletion error!\n");
        return -1;
    }
    // delete the file from the current dir
    // TODO maybe it is not a good practice, but maybe faster
    // struct Inode* file = (struct Inode*)(disk + (INODE_BLOCK + *Inode_num) * BLOCK_SIZE);
    struct Inode file;
    ReadBlock(*Inode_num + INODE_BLOCK, (char*)&file, sizeof(file), 0, 0);
    if(file.file_type == 1){
        sprintf(output+strlen(output), "FAIL: %s is not a file\n", file.filename);
        fprintf(fs_log, "FAIL: %s is not a file\n", file.filename);
        return -1;
    }
    FreeFile(&file);
    I_ReturnBlock(*Inode_num);
    *Inode_num = -1;
    // WriteBlock(Current_Dir + INODE_BLOCK, (char*)&Current_Dir, sizeof(Current_Dir), 0, 0);
    WriteBlock(Current_Dir + INODE_BLOCK, (char*)&Dir, sizeof(Dir), 0, 0);
}

// TODO 
int FreeDir(struct Inode* dir){
    // int block_num = dir->block_num;
    assert(dir != NULL);
    assert(dir->file_type == 1);
    for(int i = 2;i < DIRECT_BLOCK;++i){
        if(dir->direct[i] == -1)
            continue;
        // struct Inode* tmp = (struct Inode*)(disk + (INODE_BLOCK + dir->direct[i]) * BLOCK_SIZE);
        struct Inode tmp;
        ReadBlock(dir->direct[i] + INODE_BLOCK, (char*)&tmp, sizeof(tmp), 0, 0);
        if(tmp.file_type == 1){
            FreeDir(&tmp);
        }
        else{
            FreeFile(&tmp);
        }
        I_ReturnBlock(dir->direct[i]);
        // I_ReturnBlock(*GetPhysicalBlockNum(dir, i));
    }
    for(int j = 0;j < 4;++j){
        if(dir->second_indirect[j] == -1)
            continue;
        // assert(0);
        // uint64_t* second_block = (uint64_t*)(disk + (INODE_BLOCK + dir->second_indirect[j]) * BLOCK_SIZE);
        uint64_t second_block[ENTRY_NUM];
        ReadBlock(dir->second_indirect[j] + INODE_BLOCK, (char*)second_block, sizeof(second_block), 0, 0);     
        for(int i = 0;i < ENTRY_NUM;++i){
            if(second_block[i] == -1)
                continue;
            // struct Inode* tmp = (struct Inode*)(disk + (INODE_BLOCK + second_block[i]) * BLOCK_SIZE);
            struct Inode tmp;
            ReadBlock(second_block[i] + INODE_BLOCK, (char*)&tmp, sizeof(tmp), 0, 0);
            if(tmp.file_type == 1){
                FreeDir(&tmp);
            }
            else{
                FreeFile(&tmp);
            }
            I_ReturnBlock(second_block[i]);
        }
        I_ReturnBlock(dir->second_indirect[j]);
    }
    dir->block_num = 0;
    dir->file_size = 0;
}

// Dir has to be recursively deleted
// rmdir d
int RemoveDir(char **args){
    if(strcmp(args[1], "/") == 0){
        sprintf(output+strlen(output), "FAIL: Cannot remove root dir!\n");
        fprintf(fs_log, "FAIL: Cannot remove root dir!\n");
        return -1;
    }
    if(strcmp(args[1], ".") == 0){
        sprintf(output+strlen(output), "FAIL: Cannot remove current dir!\n");
        fprintf(fs_log, "FAIL: Cannot remove current dir!\n");
        return -1;
    }
    if(strcmp(args[1], "..") == 0){
        sprintf(output+strlen(output), "FAIL: Cannot remove parent dir!\n");
        fprintf(fs_log, "FAIL: Cannot remove parent dir!\n");
        return -1;
    }
    uint64_t* Inode_num = FindEntry(Current_Dir, args[1]);
    if(Inode_num == NULL){
        sprintf(output+strlen(output), "FAIL: No such dir!\nDeletion error!\n");
        fprintf(fs_log, "FAIL: No such dir! Deletion error!\n");
        return -1;
    }
    // delete the dir from the current dir
    // TODO maybe it is not a good practice
    // struct Inode* dir = (struct Inode*)(disk + (INODE_BLOCK + *Inode_num) * BLOCK_SIZE);
    // declared before find entry function
    struct Inode dir;
    ReadBlock(*Inode_num + INODE_BLOCK, (char*)&dir, sizeof(dir), 0, 0);
    if(dir.file_type == 0){
        sprintf(output+strlen(output), "FAIL: %s is not a dir\n", dir.filename);
        fprintf(fs_log, "FAIL: %s is not a dir\n", dir.filename);
        return -1;
    }
    FreeDir(&dir);
    I_ReturnBlock(*Inode_num);
    *Inode_num = -1;
    // WriteBlock(Current_Dir + INODE_BLOCK, (char*)&Current_Dir, sizeof(Current_Dir), 0, 0);
    WriteBlock(Current_Dir + INODE_BLOCK, (char*)&Dir, sizeof(Dir), 0, 0);
}

// cd path 
// consider the relative and absolute path
void ParsePath(char* path, char* dirs[]){
    assert(path != NULL);
    int i = 0;
    if(path[0] == '/'){
        dirs[i++] = "/";
        path++;
    }
    char* token = strtok(path, "/");
    while(token != NULL){
        dirs[i++] = token;
        token = strtok(NULL, "/");
    }
    dirs[i] = NULL;
}

int ChangeDir(char **args){
    char* dirs[MAX_DIR_DEPTH];
    ParsePath(args[1], dirs);
    uint64_t tmp_cur_dir = Current_Dir;
    int cnt = 0;
    // struct Inode cur_dir = *(struct Inode*)(disk + (INODE_BLOCK + Current_Dir) * BLOCK_SIZE);
    struct Inode cur_dir;
    ReadBlock(Current_Dir + INODE_BLOCK, (char*)&cur_dir, sizeof(cur_dir), 0, 0);
    // sprintf(output+strlen(output), "Current_Dir: %ld, dir-direct[0]: %ld\n", Current_Dir, cur_dir.direct[0]);
    assert(Current_Dir == cur_dir.direct[0]);
    if(strcmp(dirs[0], "/") == 0){
        tmp_cur_dir = 0;
        cnt++;
    }
    else if(strcmp(dirs[0], "..") == 0){
        // sprintf(output+strlen(output), "back to the fa\n");
        tmp_cur_dir = cur_dir.direct[1];
        // sprintf(output+strlen(output), "tmp_cur_dir: %ld\n", tmp_cur_dir);
        cnt++;
    }
    else if(strcmp(dirs[0], ".") == 0){
        tmp_cur_dir = Current_Dir;
        cnt++;
    }
    else{
        // sprintf(output+strlen(output), "working in %ld\n", Current_Dir);
        tmp_cur_dir = Current_Dir;
    }
    // cur_dir = *(struct Inode*)(disk + (INODE_BLOCK + tmp_cur_dir) * BLOCK_SIZE);
    ReadBlock(tmp_cur_dir + INODE_BLOCK, (char*)&cur_dir, sizeof(cur_dir), 0, 0);
    while(dirs[cnt] != NULL){
        // sprintf(output+strlen(output), "dirs[%d]: %s\n", cnt, dirs[cnt]);
        uint64_t* Inode_num = FindEntry(tmp_cur_dir, dirs[cnt]);
        if(Inode_num == NULL){
            sprintf(output+strlen(output), "FAIL: No such dir %s!\n", dirs[cnt]);
            fprintf(fs_log, "FAIL: No such dir %s!\n", dirs[cnt]);
            return -1;
        }
        // struct Inode* inode = (struct Inode*)(disk + (INODE_BLOCK + *Inode_num) * BLOCK_SIZE);
        struct Inode inode;
        ReadBlock(*Inode_num + INODE_BLOCK, (char*)&inode, sizeof(inode), 0, 0);
        if(inode.file_type == 0){
            sprintf(output+strlen(output), "FAIL: %s is not a dir!\n", inode.filename);
            fprintf(fs_log, "FAIL: %s is not a dir!\n", inode.filename);
            return -1;
        }
        // sprintf(output+strlen(output), "inode . %ld .. %ld\n", inode->direct[0], inode->direct[1]);
        tmp_cur_dir = *Inode_num;
        // memcpy(&cur_dir, disk + (INODE_BLOCK + *Inode_num) * BLOCK_SIZE, sizeof(struct Inode));
        ReadBlock(*Inode_num + INODE_BLOCK, (char*)&cur_dir, sizeof(cur_dir), 0, 0);
        cnt++;
    }
    Current_Dir = tmp_cur_dir;
    // CurrentDir_Inode = (struct Inode*)(disk +(INODE_BLOCK + Current_Dir) * BLOCK_SIZE);
    ReadBlock(Current_Dir + INODE_BLOCK, (char*)&CurrentDir_Inode, sizeof(CurrentDir_Inode), 0, 0);
    memcpy(pwd, args[1], strlen(args[1]));
    // cur_dir = *(struct Inode*)(disk + (INODE_BLOCK + tmp_cur_dir) * BLOCK_SIZE);
    // ReadBlock(tmp_cur_dir + INODE_BLOCK, (char*)&cur_dir, sizeof(cur_dir), 0, 0);
    // sprintf(output+strlen(output), "after Current_Dir: %ld, dir-direct[0]: %ld\n", Current_Dir, cur_dir.direct[0]);
}

void PrintEntry(struct Inode* inode){
    assert(inode != NULL);
    // TODO we should consider the format using *. to implement it
    sprintf(output+strlen(output), "%s\t", inode->filename);
    sprintf(output+strlen(output), "%ld\t", inode->file_size);
    if(inode->file_type == 0)
        sprintf(output+strlen(output), "%s\t", "F");
    else 
        sprintf(output+strlen(output), "%s\t", "D");
    sprintf(output+strlen(output), "%s\t", inode->Time);
    sprintf(output+strlen(output), "\n");
}

int PrintCurDir(char **args){
    sprintf(output+strlen(output), "Current dir: %ld\n", Current_Dir);
    // struct Inode* inode = (struct Inode*)(disk + (INODE_BLOCK + Current_Dir) * BLOCK_SIZE);
    struct Inode inode;
    ReadBlock(Current_Dir + INODE_BLOCK, (char*)&inode, sizeof(inode), 0, 0);
    sprintf(output+strlen(output), "%s\n", inode.filename);
    // sprintf(output+strlen(output), "inode . %ld .. %ld\n", inode->direct[0], inode->direct[1]);
    PrintEntry(&inode);
}

// ls
// for dir inode , we cannot use the block_num, we must use -1 to manage the invalid inode entry
int List(char **args){
    // sprintf(output+strlen(output), "List called\n");
    struct Inode dir, tmp; 
    // memcpy(&dir, disk + (INODE_BLOCK + Current_Dir) * BLOCK_SIZE, sizeof(dir));
    ReadBlock(Current_Dir + INODE_BLOCK, (char*)&dir, sizeof(dir), 0, 0);
    if(dir.file_type == 0){
        sprintf(output+strlen(output), "FAIL: Not a dir!\n");
        sprintf(output+strlen(output), "FAIL: Not a dir!\n");
        // ERROR!!!
        assert(0);
        return -1;
    }
    // sprintf(output+strlen(output), ".:%ld\t..:%ld\n", dir.direct[0], dir.direct[1]);
    // sprintf(output+strlen(output), "%s\n", dir.filename);
    assert(dir.direct[0] != -1 && dir.direct[1] != -1);
    // memcpy(&tmp, disk + (INODE_BLOCK + dir.direct[0]) * BLOCK_SIZE, sizeof(tmp));
    ReadBlock(dir.direct[0] + INODE_BLOCK, (char*)&tmp, sizeof(tmp), 0, 0);
    sprintf(output+strlen(output), ".\t");
    sprintf(output+strlen(output), "%ld\t", tmp.file_size);
    sprintf(output+strlen(output), "%s\t", "D");
    sprintf(output+strlen(output), "%s\t", tmp.Time);
    sprintf(output+strlen(output), "\n");

    // memcpy(&tmp, disk + (INODE_BLOCK + dir.direct[1]) * BLOCK_SIZE, sizeof(tmp));
    ReadBlock(dir.direct[1] + INODE_BLOCK, (char*)&tmp, sizeof(tmp), 0, 0);
    sprintf(output+strlen(output), "..\t");
    sprintf(output+strlen(output), "%ld\t", tmp.file_size);
    sprintf(output+strlen(output), "%s\t", "D");
    sprintf(output+strlen(output), "%s\t", tmp.Time);
    sprintf(output+strlen(output), "\n");

    for(int i = 2;i < DIRECT;++i){
        if(dir.direct[i] == -1)
            continue;
        // sprintf(output+strlen(output), "%d\n%ld\t%ld\n", i, dir.direct[i], (INODE_BLOCK + dir.direct[i]) * BLOCK_SIZE);
        assert((INODE_BLOCK + dir.direct[i]) * BLOCK_SIZE <= DISK_SIZE);
        // memcpy(&tmp, disk + (INODE_BLOCK + dir.direct[i]) * BLOCK_SIZE, sizeof(tmp));
        ReadBlock(dir.direct[i] + INODE_BLOCK, (char*)&tmp, sizeof(tmp), 0, 0);
        PrintEntry(&tmp);
    }
    for(int i = 0;i < SECOND_INDIRECT_BLOCK;++i){
        if(dir.second_indirect[i / ENTRY_NUM] == -1)
            continue;
        // sprintf(output+strlen(output), "%d\n", i);
        assert((INODE_BLOCK + dir.second_indirect[i / ENTRY_NUM]) * BLOCK_SIZE < DISK_SIZE);
        // uint64_t* second_block = (uint64_t*)(disk + (INODE_BLOCK + dir.second_indirect[i / ENTRY_NUM] * BLOCK_SIZE));
        uint64_t second_block[ENTRY_NUM];
        ReadBlock(dir.second_indirect[i / ENTRY_NUM] + INODE_BLOCK, (char*)second_block, sizeof(second_block), 0, 0);
        if(second_block[i % ENTRY_NUM] == -1)
            continue;
        // memcpy(&tmp, disk + (second_block[i % ENTRY_NUM] + INODE_BLOCK) * BLOCK_SIZE, sizeof(tmp));
        ReadBlock(second_block[i % ENTRY_NUM] + INODE_BLOCK, (char*)&tmp, sizeof(tmp), 0, 0);
        PrintEntry(&tmp);
    }
    return 0;
}

// cat f 
int Cat(char **args){
    // sprintf(output+strlen(output), "Look file: %s\n", args[1]);
    uint64_t* Inode_num = FindEntry(Current_Dir, args[1]);
    // sprintf(output+strlen(output), "Find Over\n");
    if(Inode_num == NULL){
        sprintf(output+strlen(output), "FAIL: No such file! %s\nCat error\n", args[1]);
        fprintf(fs_log, "FAIL: No such file %s! Cat error\n", args[1]);
        return -1;
    }
    struct Inode file;
    // memcpy(&file, disk + (INODE_BLOCK + *Inode_num) * BLOCK_SIZE, sizeof(file));
    ReadBlock(*Inode_num + INODE_BLOCK, (char*)&file, sizeof(file), 0, 0);
    if(file.file_type == 1){
        sprintf(output+strlen(output), "FAIL: %s is a dir!\nCat error\n", args[1]);
        fprintf(fs_log, "FAIL: %s is a dir! Cat error\n", args[1]);
        return -1;
    }
    char* content = (char*)malloc(file.file_size + 1);
    memset(content, 0, file.file_size + 1);
    int sz = file.file_size, offset = 0;
    // sprintf(output+strlen(output), "block num: %ld\n", file.block_num);
    assert(file.file_size / BLOCK_SIZE + (file.file_size % BLOCK_SIZE != 0) == file.block_num);
    for(int i = 0;i < DIRECT_BLOCK && sz > 0;++i){
        int len = MIN(sz, BLOCK_SIZE);
        // memcpy(content + offset, disk + (file.direct[i]) * BLOCK_SIZE, len);
        ReadBlock(file.direct[i], content + offset, len, 0, 0);
        sz -= BLOCK_SIZE;
        offset += BLOCK_SIZE;
    }
    for(int i = 0;i < SECOND_INDIRECT_BLOCK && sz > 0;++i){
        // uint64_t* second_block = (uint64_t*)(disk + (INODE_BLOCK + file.second_indirect[i]) * BLOCK_SIZE);
        uint64_t second_block[ENTRY_NUM];
        ReadBlock(file.second_indirect[i] + INODE_BLOCK, (char*)second_block, sizeof(second_block), 0, 0);
        for(int j = 0;j < ENTRY_NUM;++j){
            int len = MIN(sz, BLOCK_SIZE);
            // memcpy(content + offset, disk + (second_block[j]) * BLOCK_SIZE, len);
            ReadBlock(second_block[j], content + offset, len, 0, 0);
            sz -= BLOCK_SIZE;
            offset += BLOCK_SIZE;
        }
    }
    sprintf(output+strlen(output), "%s\n", content);
    free(content);
}

int WriteAux(struct Inode* file, char* data, int len){
    // sprintf(output+strlen(output), "write aux: %s\n", data);
    int sz = len;
    if(len < 0){
        sprintf(output+strlen(output), "FAIL: Invalid length!\nWrite error!\n");
        fprintf(fs_log, "FAIL: Invalid length! Write error!\n");
        return -1;
    }
    int block_num = len / BLOCK_SIZE + (len % BLOCK_SIZE != 0);
    for(int i = 0;i < block_num && len > 0;++i){
        uint64_t* block = GetPhysicalBlockNum(file, i);
        *block = D_GetFreeBlock();
        // sprintf(output+strlen(output), "block: %ld\n", *block);
        if(*block == -1){
            sprintf(output+strlen(output), "FAIL: No enough space!\nWrite error!\n");
            fprintf(fs_log, "FAIL: No enough space! Write error!\n");
            return -1;
        }
        // file.direct[i] = block;
        // memcpy(disk + (*block) * BLOCK_SIZE, data + i * BLOCK_SIZE, MIN(BLOCK_SIZE, len));
        WriteBlock(*block, data + i * BLOCK_SIZE, MIN(BLOCK_SIZE, len), 0, 0);
        file->block_num++;
        len -= BLOCK_SIZE;
    }
    file->file_size = sz;
}

// w f l data  
int Write(char **args){
    // sprintf(output+strlen(output), "Write called\n");
    char* file_name = args[1];
    uint64_t* inode_block = FindEntry(Current_Dir, file_name);
    if(inode_block == NULL){
        sprintf(output+strlen(output), "FAIL: No such file!\nWrite error!\n");
        fprintf(fs_log, "FAIL: No such file! Write error!\n");
        return -1;
    }
    if(atoi(args[2]) < 0){
        sprintf(output+strlen(output), "FAIL: Invalid length!\nWrite error!\n");
        fprintf(fs_log, "FAIL: Invalid length! Write error!\n");
        return -1;
    }
    // struct Inode* file = (struct Inode*)(disk + (INODE_BLOCK + *inode_block) * BLOCK_SIZE);
    struct Inode file;
    ReadBlock(*inode_block + INODE_BLOCK, (char*)&file, sizeof(file), 0, 0);
    if(file.file_type == 1){
        sprintf(output+strlen(output), "FAIL: Not a file!\nWrite error!\n");
        fprintf(fs_log, "FAIL: Not a file! Write error!\n");
        return -1;
    }
    FreeFile(&file);
    int len = MIN(strlen(args[3]), atoi(args[2]));
    WriteAux(&file, args[3], len);
    file.block_num = len / BLOCK_SIZE + (len % BLOCK_SIZE != 0);
    file.file_size = len;
    // sprintf(output+strlen(output), "write to block %ld\n", file->direct[0]);
    WriteBlock(*inode_block + INODE_BLOCK, (char*)&file, sizeof(file), 0, 0);
}

// i f pos l data 
// an easy implementation  
// write to a char array and call write
int Insert(char **args){
    // char* file_name = args[1];
    uint64_t pos = atoi(args[2]);
    uint64_t* inode_block = FindEntry(Current_Dir, args[1]);
    if(inode_block == NULL){
        sprintf(output+strlen(output), "FAIL: No such file!\nInsert error!\n");
        fprintf(fs_log, "FAIL: No such file! Insert error!\n");   
        return -1;
    }
    if(pos < 0){
        sprintf(output+strlen(output), "FAIL: Invalid position!\nInsert error!\n");
        fprintf(fs_log, "FAIL: Invalid position! Insert error!\n");
        return -1;
    }
    if(atoi(args[3]) < 0){
        sprintf(output+strlen(output), "FAIL: Invalid length!\nInsert error!\n");
        fprintf(fs_log, "FAIL: Invalid length! Insert error!\n");
        return -1;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       
    }
    // struct Inode* file = (struct Inode*)(disk + (INODE_BLOCK + *inode_block) * BLOCK_SIZE);
    struct Inode file;
    ReadBlock(*inode_block + INODE_BLOCK, (char*)&file, sizeof(file), 0, 0);
    if(file.file_type == 1){
        sprintf(output+strlen(output), "FAIL: %s is a dir!\nInsert error!\n", args[1]);
        fprintf(fs_log, "FAIL: %s is a dir! Insert error!\n", args[1]);
    }
    int len = MIN(strlen(args[4]), atoi(args[3]));
    char* content = (char*)malloc(file.file_size + len + 1);
    memset(content, 0, file.file_size + len);
    int data_before_blk_num = pos / BLOCK_SIZE;
    // sprintf(output+strlen(output), "data_before_blk_num: %d\n", data_before_blk_num);
    // int data_before_blk_num = atoi(args[2]) / BLOCK_SIZE + (atoi(args[2]) % BLOCK_SIZE != 0);
    // sprintf(output+strlen(output), "physical block num: %d\n", *GetPhysicalBlockNum(file, data_before_blk_num));
    for(int i = 0;i < data_before_blk_num;++i){
        // sprintf(output+strlen(output), "physical block num: %ld\n", *GetPhysicalBlockNum(file, i));
        // memcpy(content + i * BLOCK_SIZE, disk + (*GetPhysicalBlockNum(file, i)) * BLOCK_SIZE, BLOCK_SIZE);
        ReadBlock(*GetPhysicalBlockNum(&file, i), content + i * BLOCK_SIZE, BLOCK_SIZE, 0, 0);
    }
    int offset = data_before_blk_num * BLOCK_SIZE;  
    // sprintf(output+strlen(output), "%d\n", offset);
    // sprintf(output+strlen(output), "physical block num: %ld\n", *GetPhysicalBlockNum(file, 0));
    // memcpy(content + offset, disk + (*GetPhysicalBlockNum(file, data_before_blk_num) * BLOCK_SIZE), pos % BLOCK_SIZE);
    ReadBlock(*GetPhysicalBlockNum(&file, data_before_blk_num), content + offset, pos % BLOCK_SIZE, 0, 0);
    // sprintf(output+strlen(output), "%s\n", content);
    printf("offset: %d\tcontent1: %s\n", offset, content);
    offset += pos % BLOCK_SIZE;
    // sprintf(output+strlen(output), "%d\n", offset);
    memcpy(content + offset, args[4], len);
    printf("offset: %d\tcontent2: %s\n", offset, content);
    offset += len;
    // sprintf(output+strlen(output), "%d\n", offset);
    // sprintf(output+strlen(output), "%s\n", content);
    int trunc_sz = MIN(file.file_size - data_before_blk_num * BLOCK_SIZE, BLOCK_SIZE);
    int copy_sz = trunc_sz - pos % BLOCK_SIZE;
    // if(pos % BLOCK_SIZE != 0)
    if(copy_sz && pos < BLOCK_SIZE)
        // memcpy(content + offset, disk + (*GetPhysicalBlockNum(file, data_before_blk_num)) * BLOCK_SIZE + pos, copy_sz);
        ReadBlock(*GetPhysicalBlockNum(&file, data_before_blk_num), content + offset, copy_sz, pos % BLOCK_SIZE, 0);
    // sprintf(output+strlen(output), "%s\n", content);  
    printf("offset: %d\tcontent3: %s\n", offset, content);
    offset += BLOCK_SIZE - pos % BLOCK_SIZE;
    for(int i = data_before_blk_num + 1;i < file.block_num;++i){
        // memcpy(content + offset, disk + (*GetPhysicalBlockNum(file, i)) * BLOCK_SIZE, BLOCK_SIZE);
        ReadBlock(*GetPhysicalBlockNum(&file, i), content + offset, BLOCK_SIZE, 0, 0);
        offset += BLOCK_SIZE;
    }
    // sprintf(output+strlen(output), "%s\n", content);
    content[file.file_size + len] = '\0';
    int tmp_sz = file.file_size;
    FreeFile(&file);
    WriteAux(&file, content, tmp_sz + len);
    // sprintf(output+strlen(output), "write to block %ld\n", file->direct[0]);
    printf("content: %s\n", content);
    printf("file size: %d\n", file.file_size);
    printf("content: %s\n", content + strlen(content));
    free(content);
    WriteBlock(*inode_block + INODE_BLOCK, (char*)&file, sizeof(file), 0, 0);
}

// d f pos l 
int Delete(char **args){
    // char* file_name = args[1];
    uint64_t pos = atoi(args[2]);
    uint64_t* inode_block = FindEntry(Current_Dir, args[1]);
    if(inode_block == NULL){
        sprintf(output+strlen(output), "FAIL: No such file %s!\nDelete error!\n", args[1]);
        fprintf(fs_log, "FAIL: No such file %s! Delete error!\n", args[1]);
        return -1;
    }
    // struct Inode* file = (struct Inode*)(disk + (INODE_BLOCK + *inode_block) * BLOCK_SIZE);
    struct Inode file;
    ReadBlock(INODE_BLOCK + *inode_block, &file, sizeof(struct Inode), 0, 0);
    if(file.file_type == 1){
        sprintf(output+strlen(output), "FAIL: Not a file!\nDelete error!\n");
        fprintf(fs_log, "FAIL: Not a file! Delete error!\n");
        return -1;
    }
    int len = MIN(file.file_size - pos, atoi(args[3]));
    assert(file.file_size >= len);
    char* content = (char*)malloc(file.file_size - len  + 1);
    int data_before_blk_num = pos / BLOCK_SIZE;
    for(int i = 0;i < data_before_blk_num;++i){
        // memcpy(content + i * BLOCK_SIZE, disk + (*GetPhysicalBlockNum(file, i)) * BLOCK_SIZE, BLOCK_SIZE);
        ReadBlock(*GetPhysicalBlockNum(&file, i), content + i * BLOCK_SIZE, BLOCK_SIZE, 0, 0);
    }
    int offset = data_before_blk_num * BLOCK_SIZE;
    // offset += len;
    // data_before_blk_num = (atoi(args[2]) + len) / BLOCK_SIZE + ((atoi(args[2]) + len) % BLOCK_SIZE != 0);
    // memcpy(content + offset, disk + (*GetPhysicalBlockNum(file, data_before_blk_num)) * BLOCK_SIZE, pos % BLOCK_SIZE);
    ReadBlock(*GetPhysicalBlockNum(&file, data_before_blk_num), content + offset, pos % BLOCK_SIZE, 0, 0);
    offset += pos % BLOCK_SIZE;
    int trunc_sz = MIN(file.file_size - data_before_blk_num * BLOCK_SIZE, BLOCK_SIZE);
    // memcpy(content + offset, disk + (*GetPhysicalBlockNum(file, data_before_blk_num - (atoi(args[2]) % BLOCK_SIZE != 0))) * BLOCK_SIZE, BLOCK_SIZE - atoi(args[2]) % BLOCK_SIZE);
    // offset += BLOCK_SIZE - atoi(args[2]) % BLOCK_SIZE;
    if(pos % BLOCK_SIZE + len < BLOCK_SIZE)
        // memcpy(content + offset, disk + (*GetPhysicalBlockNum(file, data_before_blk_num)) * BLOCK_SIZE + pos % BLOCK_SIZE + len, trunc_sz - pos % BLOCK_SIZE - len);
        ReadBlock(*GetPhysicalBlockNum(&file, data_before_blk_num), content + offset, trunc_sz - pos % BLOCK_SIZE - len, pos % BLOCK_SIZE + len, 0);
    for(int i = data_before_blk_num + 1;i < file.block_num;++i){
        // memcpy(content + offset, disk + (*GetPhysicalBlockNum(file, i)) * BLOCK_SIZE, BLOCK_SIZE);
        ReadBlock(*GetPhysicalBlockNum(&file, i), content + offset, BLOCK_SIZE, 0, 0);
        offset += BLOCK_SIZE;
    }
    // sprintf(output+strlen(output), "%s\n", content);
    content[file.file_size - len] = '\0';
    int tmp_sz = file.file_size;
    FreeFile(&file);
    WriteAux(&file, content, tmp_sz - len);
    free(content);
    // sprintf(output+strlen(output), "Delete success!\n");
    WriteBlock(*inode_block + INODE_BLOCK, (char*)&file, sizeof(file), 0, 0);
}

// exit 
int Exit(char **args){
    ReturnBlock();
    sprintf(output+strlen(output), "Goodbye\n");
    printf("output: %s", output);
    fprintf(fs_log, "Goodbye\n");
    send(client_fd, output, sizeof(output), 0);
    // free(disk);
    // disk = NULL;
    fclose(fs_log);
    fs_log = NULL;
    close(client_fd);
    printf("closed\n");
    close(socket_fd);
    exit(0);
}

int PrintHelp(char**);

// for all cmds
static struct {
    const char* cmd_name;
    const char* cmd_usage;
    int (*handler) (char**);
    // record the number of args
    int argc;
} cmd_table [] = {
    {"f", "f\n", Format, 1},
    {"mk", "mk f\n", MakeFile, 2},
    {"mkdir", "mkdir d\n", MakeDir, 2},
    {"rm", "rm f\n", RemoveFile, 2},
    //  TODO 
    {"cd", "cd path", ChangeDir, 2},
    {"rmdir", "rmdir d\n", RemoveDir, 2},
    {"ls", "ls\n", List, 1},
    {"cat", "cat f\n", Cat, 2},
    {"w", "write f pos l data\n", Write, 4},
    {"i", "insert f pos l data\n", Insert, 5},
    {"d", "delete f pos l\n", Delete, 4},
    {"e", "exit\n", Exit, 1},
    {"pwd", "print current working dir\n", PrintCurDir, 1},
    {"help", "show help information\n", PrintHelp, 1},
};

int PrintHelp(char** args){
    #define CMD_NUM sizeof(cmd_table) / sizeof(cmd_table[0])
    for(int i = 0;i < CMD_NUM;++i){
        sprintf(output+strlen(output), "%s: %s", cmd_table[i].cmd_name, cmd_table[i].cmd_usage);
    }
}

void FileSystem(){
    printf("enter the fs\n");
    char cmd[CMD_MAX_LEN];
    char* args[5];
    int sz = sizeof(cmd_table) / sizeof(cmd_table[0]);

    int res;

    fs_log = fopen("fs.log", "w");
    if(fs_log == NULL){
        printf("Open fs.log error!\n");
        exit(0);
    }

    setbuf(fs_log, NULL);

    fprintf(fs_log, "Welcome to the file system!\n");
    fprintf(fs_log, "disk size: %d\n", DISK_SIZE);
    Format(NULL);

    while(1){
        client_fd = accept(socket_fd, (struct sockaddr*)&client_addr, &client_len);
        printf("one connected\n");
        if(client_fd < 0){
            printf("Accept error!\n");
            exit(0);
        }

        if(fork() == 0){

    while(1){
        // sprintf(output+strlen(output), "$");
        memset(output, 0, sizeof(output));
        // printf("\033[34m%s\033[0m \033[32m$\033[0m ", CurrentDir_Inode->filename);
        sprintf(output, "\033[34m%s\033[0m \033[32m$\033[0m ", CurrentDir_Inode.filename);
        // sprintf(output, "\033[34m%s\033[0m \033[32m$\033[0m ", pwd);
        // scanf("%s", cmd);
        write(client_fd, output, sizeof(output));
        // send(client_fd, output, sizeof(output), 0);
        output[0] = '\0';
        memset(cmd, 0, sizeof(cmd));
        recv(client_fd, cmd, CMD_MAX_LEN, 0);
        printf("get cmd: %s\n", cmd);
        printf("get cmd: %s\n", cmd + 5);
        fprintf(fs_log, "\033[34m%s\033[0m", cmd);
        // printf("get here\n");
        // fgets(cmd, CMD_MAX_LEN, stdin);
        cmd[strcspn(cmd, "\r\n")] = '\0';
        int argc = Get_Args(cmd ,args);
        // sprintf(output+strlen(output), "argc: %d\tcmd: %s\n", argc, cmd);
        int i;
        for(i = 0;i < sz;++i){
            if(strcmp(args[0], cmd_table[i].cmd_name) == 0){
                if(cmd_table[i].handler != NULL){
                    if(argc != cmd_table[i].argc){
                        sprintf(output+strlen(output), "%s\n", error_message[i]);
                        break;
                    }
                    res = cmd_table[i].handler(args);
                    if(res == -1){
                        sprintf(output+strlen(output), "\033[31mNO\033[0m\n");
                    }
                    else{
                        sprintf(output+strlen(output), "\033[32mYES\033[0m\n");
                    }
                }
                // assert(strlen(output+strlen(output)) > 0);
                // printf("%s", output);
                break;
            }
        }
        if(i == sz)
            sprintf(output+strlen(output), "FAIL: No such command!\n");
        //send(client_fd, output, sizeof(output), 0);
        printf("here\n");
        write(client_fd, output, sizeof(output));
        fprintf(fs_log, "%s", output);
    }
    }
    close(client_fd);
    }
}

__attribute__((destructor)) void CleanUp(){
    // if(disk != NULL)
        // free(disk);
    if(fs_log != NULL)
        fclose(fs_log);
}

int main(int argc, char* argv[]) {
    if(argc != 3){
        printf("Usage: ./fs <DiskPort> <FSPort>\n");
        exit(0);
    }

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_fd < 0){
        printf("Create server socket error!\n");
        exit(0);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[2]));
    
    if(bind(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        printf("Bind error!\n");
        exit(0);
    }

    if(listen(socket_fd, 10) < 0){
        printf("Listen error!\n");
        exit(0);
    }

    fs_fd= socket(AF_INET, SOCK_STREAM, 0);
    memset(&disk_addr, 0, sizeof(disk_addr)); // 初始化服务器地址为零
    disk_addr.sin_family = AF_INET;
    disk_addr.sin_port = htons(atoi(argv[1]));
    if(connect(fs_fd, (struct sockaddr*)&disk_addr, sizeof(disk_addr)) == -1){
        printf("Connect error!\n");
        exit(0);
    }

    client_len = sizeof(client_addr);

    setbuf(stdout, NULL);
    // sprintf(output, "%ld\n", sizeof(struct Inode));
    sprintf(output, "Welcome to the file system!\n");
    printf("Welcome to the file system!\n");
    assert(sizeof(struct Inode) == BLOCK_SIZE);

    printf("offset:\ntime: %ld", CurrentDir_Inode.Time - (char*)&CurrentDir_Inode);

    printf("disk size: %d\n", DISK_SIZE);
    sprintf(output, "disk size: %d\n", DISK_SIZE);
    FileSystem();
    // free(disk);
    // disk = NULL;
    fclose(fs_log);
    fs_log = NULL;
    close(socket_fd);
    return 0;
}