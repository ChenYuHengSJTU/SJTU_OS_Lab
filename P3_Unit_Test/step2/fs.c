#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<stdint.h>
#include<stdbool.h>
#include<assert.h>
#include<string.h>
#include<time.h>

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
int Get_Args(char* cmd, char* args[]){
    int cnt = 0;
    char* tmp = strtok(cmd, " ");
    while(tmp != NULL){
        args[cnt++] = tmp;
        tmp = strtok(NULL, " ");
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
struct Inode* CurrentDir_Inode = NULL;
char pwd[FILENAME_SIZE * MAX_DIR_DEPTH];

// the disk ,we should divide it into blocks too 
char* disk = NULL;
FILE* fs_log = NULL;
// to share the code 
#define CYLINDER 50
#define SEC_PER_CYLINDER 200
#define DISK_SIZE CYLINDER * SEC_PER_CYLINDER * BLOCK_SIZE

// we may not need super block and log block here 
#define INODE_BLOCK 0
#define DATA_BLOCK CYLINDER * SEC_PER_CYLINDER / 3

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
        uint64_t* second_indirect = (uint64_t*)(disk + (INODE_BLOCK + second_indirect_block_num) * BLOCK_SIZE);
        return &(second_indirect[block_num_in_second_indirect]);
    }
    else{
        // TODO
        assert(0);
    }
}

// write to the block (if can combined with read?)
// block num is the physical block num
// TODO consider the parameter, offset should be a must
// flag means the inode block or the data block
// 0 - inode block, 1 - data block
int WriteBlock(uint64_t block_num, char* data, size_t n, size_t offset, int flag){
    // get the physical block num 
    // int physical_block_num = GetPhysicalBlockNum(block_num);
    // write to the disk 
    if(offset >= BLOCK_SIZE){
        fprintf(stderr, "FAIL: offset is too large\n");
        return -1;
    }
    if(!flag)
        memcpy(disk + (INODE_BLOCK + block_num) * BLOCK_SIZE + offset, data, n);
    else 
        memcpy(disk + (DATA_BLOCK + block_num) * BLOCK_SIZE + offset, data, n);
}

// read from the block
int ReadBlock(uint64_t block_num, char* data, size_t n, size_t offset, int flag){
    if(offset >= BLOCK_SIZE){
        fprintf(stderr, "FAIL: offset is too large\n");
        return -1;
    }
    if(!flag)
        memcpy(data, disk + (INODE_BLOCK + block_num) * BLOCK_SIZE + offset, n);
    else 
        memcpy(data, disk + (DATA_BLOCK + block_num) * BLOCK_SIZE + offset, n);
}

// Initialize the Disk 
void Init_Block(){
    fprintf(fs_log, "InitBlock: Init the blocks\n");
    int block_sum = CYLINDER * SEC_PER_CYLINDER;
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
    printf("allocate a inode block: %ld\n", ret);
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
    printf("return a inode block: %ld\n", block_num);
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
    printf("allocate a data block: %ld\n", ret);
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
    printf("return a data block: %ld\n", block_num);
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
        printf("Format: the disk has been initialized\nNothing to do...\n");
        fprintf(fs_log, "Format: Do nothing since the disk has been initialized\n");
        return -1;
    }
    // Here should create the root dir 
    disk = (char*)malloc(DISK_SIZE);
    memset(disk, 0, sizeof(disk));
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
    WriteBlock(Current_Dir, (char*)&root, sizeof(root), 0, 0);
    // printf("end formatting\n");
    fprintf(fs_log, "Format: Finish formatting the disk\n");
    CurrentDir_Inode = (struct Inode*)(disk +(INODE_BLOCK + Current_Dir) * BLOCK_SIZE);
}

// add an entry to the dir
void AddEntry(struct Inode* dir, uint64_t inode_num){
    // find the first empty entry
    // printf("add entry to %s\n", dir->filename);
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
            uint64_t* indirect_block = (uint64_t*)(disk + (INODE_BLOCK + dir->second_indirect[j]) * BLOCK_SIZE);
            indirect_block[0] = inode_num;
            dir->block_num++;
            return;
        }
        uint64_t* indirect_block = (uint64_t*)(disk + (INODE_BLOCK + dir->second_indirect[j]) * BLOCK_SIZE);
        for(int i = 0;i < ENTRY_NUM;++i){
            if(indirect_block[i] == -1){
                indirect_block[i] = inode_num;
                dir->block_num++;
                return;
            }
        }
    }
    printf("Fail: Current Dir has no space for new entry\n");
    fprintf(fs_log, "Fail: Current Dir has no space for new entry\n");
    return;
}

// mk f 
// TODO we need to find a inode entry for each file and dir too by traversing the dir-inode
int MakeFile(char **args){
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
        printf("FAIL: No enough space to create file!\nDir and File number is up to the limit\n");
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
    AddEntry((struct Inode*)(disk + (INODE_BLOCK + Current_Dir) * BLOCK_SIZE), Inode_num);
}

// mkdir d 
int MakeDir(char **args){
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
        printf("FAIL: No enough space to create dir!\nDir and File number is up to the limit\n");
        fprintf(fs_log, "FAIL: No enough space to create dir! Dir and File number is up to the limit\n");
        return -1;
    }
    // add the new dir to the current dir
    AddEntry((struct Inode*)(disk + (INODE_BLOCK + Current_Dir) * BLOCK_SIZE), Inode_num);
    // AddEntry(&new_dir, Inode_num);
    new_dir.direct[0] = Inode_num;
    new_dir.direct[1] = Current_Dir;

    GetTime(new_dir.Time);
    // write the inode
    WriteBlock(Inode_num, (char*)(&new_dir), sizeof(new_dir), 0, 0);
}

// maybe block num of Inode is more useful
uint64_t* FindEntry(uint64_t inode_num ,char* entry_name){
    struct Inode *dir, tmp; 
    dir = (struct Inode*)(disk + (INODE_BLOCK + inode_num) * BLOCK_SIZE);
    if(dir->file_type == 0){
        printf("FAIL: %s is not a dir\n", dir->filename);
        fprintf(fs_log, "FAIL: %s is not a dir\n", dir->filename);
        return NULL;
    }
    // memcpy(&dir, disk + (INODE_BLOCK + Current_Dir) * BLOCK_SIZE, sizeof(dir));
    for(int i = 2;i < DIRECT_BLOCK;++i){
        if(dir->direct[i] == -1)
            continue;
        memcpy(&tmp, disk + (INODE_BLOCK + dir->direct[i]) * BLOCK_SIZE, sizeof(tmp));
        if(strcmp(tmp.filename, entry_name) == 0)
            return (&dir->direct[i]);
    }
    for(int j = 0;j < SECOND_INDIRECT;++j){
        if(dir->second_indirect[j] == -1)
            continue;
        uint64_t* second_block = (uint64_t*)(disk + (INODE_BLOCK + dir->second_indirect[j]) * BLOCK_SIZE);
        for(int i = 0;i < ENTRY_NUM;++i){
            if(second_block[i] == -1)
                continue;
            memcpy(&tmp, disk + (INODE_BLOCK + second_block[i]) * BLOCK_SIZE, sizeof(tmp));
            if(strcmp(tmp.filename, entry_name) == 0)
                return (&second_block[i]);
        }
    }
    return NULL;
}

int FreeFile(struct Inode* file){
    int block_num = file->block_num;
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
        printf("FAIL: No such file!\nDeletion error!\n");
        fprintf(fs_log, "FAIL: No such file! Deletion error!\n");
        return -1;
    }
    // delete the file from the current dir
    // TODO maybe it is not a good practice, but maybe faster
    struct Inode* file = (struct Inode*)(disk + (INODE_BLOCK + *Inode_num) * BLOCK_SIZE);
    if(file->file_type == 1){
        printf("FAIL: %s is not a file\n", file->filename);
        fprintf(fs_log, "FAIL: %s is not a file\n", file->filename);
        return -1;
    }
    FreeFile(file);
    I_ReturnBlock(*Inode_num);
    *Inode_num = -1;
}

// TODO 
int FreeDir(struct Inode* dir){
    int block_num = dir->block_num;
    for(int i = 0;i < block_num;++i){
        I_ReturnBlock(*GetPhysicalBlockNum(dir, i));
    }
    dir->block_num = 0;
    dir->file_size = 0;
}

// Dir has to be recursively deleted
// rmdir d
int RemoveDir(char **args){
    uint64_t* Inode_num = FindEntry(Current_Dir, args[1]);
    if(Inode_num == NULL){
        printf("FAIL: No such dir!\nDeletion error!\n");
        fprintf(fs_log, "FAIL: No such dir! Deletion error!\n");
        return -1;
    }
    // delete the dir from the current dir
    // TODO maybe it is not a good practice
    struct Inode* dir = (struct Inode*)(disk + (INODE_BLOCK + *Inode_num) * BLOCK_SIZE);
    if(dir->file_type == 0){
        printf("FAIL: %s is not a dir\n", dir->filename);
        fprintf(fs_log, "FAIL: %s is not a dir\n", dir->filename);
        return -1;
    }
    I_ReturnBlock(*Inode_num);
    *Inode_num = -1;
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
    struct Inode cur_dir = *(struct Inode*)(disk + (INODE_BLOCK + Current_Dir) * BLOCK_SIZE);
    // printf("Current_Dir: %ld, dir-direct[0]: %ld\n", Current_Dir, cur_dir.direct[0]);
    assert(Current_Dir == cur_dir.direct[0]);
    if(strcmp(dirs[0], "/") == 0){
        tmp_cur_dir = 0;
        cnt++;
    }
    else if(strcmp(dirs[0], "..") == 0){
        // printf("back to the fa\n");
        tmp_cur_dir = cur_dir.direct[1];
        // printf("tmp_cur_dir: %ld\n", tmp_cur_dir);
        cnt++;
    }
    else if(strcmp(dirs[0], ".") == 0){
        tmp_cur_dir = Current_Dir;
        cnt++;
    }
    else{
        // printf("working in %ld\n", Current_Dir);
        tmp_cur_dir = Current_Dir;
    }
    cur_dir = *(struct Inode*)(disk + (INODE_BLOCK + tmp_cur_dir) * BLOCK_SIZE);
    while(dirs[cnt] != NULL){
        // printf("dirs[%d]: %s\n", cnt, dirs[cnt]);
        uint64_t* Inode_num = FindEntry(tmp_cur_dir, dirs[cnt]);
        if(Inode_num == NULL){
            printf("FAIL: No such dir %s!\n", dirs[cnt]);
            fprintf(fs_log, "FAIL: No such dir %s!\n", dirs[cnt]);
            return -1;
        }
        struct Inode* inode = (struct Inode*)(disk + (INODE_BLOCK + *Inode_num) * BLOCK_SIZE);
        if(inode->file_type == 0){
            printf("FAIL: %s is not a dir!\n", inode->filename);
            fprintf(fs_log, "FAIL: %s is not a dir!\n", inode->filename);
            return -1;
        }
        // printf("inode . %ld .. %ld\n", inode->direct[0], inode->direct[1]);
        tmp_cur_dir = *Inode_num;
        memcpy(&cur_dir, disk + (INODE_BLOCK + *Inode_num) * BLOCK_SIZE, sizeof(struct Inode));
        cnt++;
    }
    Current_Dir = tmp_cur_dir;
    CurrentDir_Inode = (struct Inode*)(disk +(INODE_BLOCK + Current_Dir) * BLOCK_SIZE);
    memcpy(pwd, args[1], strlen(args[1]));
    cur_dir = *(struct Inode*)(disk + (INODE_BLOCK + tmp_cur_dir) * BLOCK_SIZE);
    // printf("after Current_Dir: %ld, dir-direct[0]: %ld\n", Current_Dir, cur_dir.direct[0]);
}

void PrintEntry(struct Inode* inode){
    assert(inode != NULL);
    // TODO we should consider the format using *. to implement it
    printf("%s\t", inode->filename);
    printf("%ld\t", inode->file_size);
    if(inode->file_type == 0)
        printf("%s\t", "F");
    else 
        printf("%s\t", "D");
    printf("%s\t", inode->Time);
    printf("\n");
}

int PrintCurDir(char **args){
    printf("Current dir: %ld\n", Current_Dir);
    struct Inode* inode = (struct Inode*)(disk + (INODE_BLOCK + Current_Dir) * BLOCK_SIZE);
    printf("%s\n", inode->filename);
    // printf("inode . %ld .. %ld\n", inode->direct[0], inode->direct[1]);
    PrintEntry(inode);
}

// ls
// for dir inode , we cannot use the block_num, we must use -1 to manage the invalid inode entry
int List(char **args){
    // printf("List called\n");
    struct Inode dir, tmp; 
    memcpy(&dir, disk + (INODE_BLOCK + Current_Dir) * BLOCK_SIZE, sizeof(dir));
    if(dir.file_type == 0){
        printf("FAIL: Not a dir!\n");
        printf("FAIL: Not a dir!\n");
        // ERROR!!!
        assert(0);
        return -1;
    }
    // printf(".:%ld\t..:%ld\n", dir.direct[0], dir.direct[1]);
    // printf("%s\n", dir.filename);
    for(int i = 0;i < DIRECT;++i){
        if(dir.direct[i] == -1)
            continue;
        // printf("%d\n%ld\t%ld\n", i, dir.direct[i], (INODE_BLOCK + dir.direct[i]) * BLOCK_SIZE);
        assert((INODE_BLOCK + dir.direct[i]) * BLOCK_SIZE <= DISK_SIZE);
        memcpy(&tmp, disk + (INODE_BLOCK + dir.direct[i]) * BLOCK_SIZE, sizeof(tmp));
        PrintEntry(&tmp);
    }
    for(int i = 0;i < SECOND_INDIRECT_BLOCK;++i){
        if(dir.second_indirect[i / ENTRY_NUM] == -1)
            continue;
        // printf("%d\n", i);
        assert((INODE_BLOCK + dir.second_indirect[i / ENTRY_NUM]) * BLOCK_SIZE < DISK_SIZE);
        uint64_t* second_block = (uint64_t*)(disk + (INODE_BLOCK + dir.second_indirect[i / ENTRY_NUM] * BLOCK_SIZE));
        if(second_block[i % ENTRY_NUM] == -1)
            continue;
        memcpy(&tmp, disk + (second_block[i % ENTRY_NUM] + INODE_BLOCK) * BLOCK_SIZE, sizeof(tmp));
        PrintEntry(&tmp);
    }
}

// cat f 
int Cat(char **args){
    // printf("Look file: %s\n", args[1]);
    uint64_t* Inode_num = FindEntry(Current_Dir, args[1]);
    // printf("Find Over\n");
    if(Inode_num == NULL){
        printf("FAIL: No such file! %s\nCat error\n", args[1]);
        fprintf(fs_log, "FAIL: No such file %s! Cat error\n", args[1]);
        return -1;
    }
    struct Inode file;
    memcpy(&file, disk + (INODE_BLOCK + *Inode_num) * BLOCK_SIZE, sizeof(file));
    if(file.file_type == 1){
        printf("FAIL: %s is a dir!\nCat error\n", args[1]);
        fprintf(fs_log, "FAIL: %s is a dir! Cat error\n", args[1]);
        return -1;
    }
    char* content = (char*)malloc(file.file_size + 1);
    memset(content, 0, file.file_size + 1);
    int sz = file.file_size, offset = 0;
    // printf("block num: %ld\n", file.block_num);
    assert(file.file_size / BLOCK_SIZE + (file.file_size % BLOCK_SIZE != 0) == file.block_num);
    for(int i = 0;i < DIRECT_BLOCK && sz > 0;++i){
        int len = MIN(sz, BLOCK_SIZE);
        memcpy(content + offset, disk + (file.direct[i]) * BLOCK_SIZE, len);
        sz -= BLOCK_SIZE;
        offset += BLOCK_SIZE;
    }
    for(int i = 0;i < SECOND_INDIRECT_BLOCK && sz > 0;++i){
        uint64_t* second_block = (uint64_t*)(disk + (INODE_BLOCK + file.second_indirect[i]) * BLOCK_SIZE);
        for(int j = 0;j < ENTRY_NUM;++j){
            int len = MIN(sz, BLOCK_SIZE);
            memcpy(content + offset, disk + (second_block[j]) * BLOCK_SIZE, len);
            sz -= BLOCK_SIZE;
            offset += BLOCK_SIZE;
        }
    }
    printf("%s\n", content);
    free(content);
}

int WriteAux(struct Inode* file, char* data, int len){
    printf("write aux: %s\n", data);
    int sz = len;
    if(len < 0){
        printf("FAIL: Invalid length!\nWrite error!\n");
        fprintf(fs_log, "FAIL: Invalid length! Write error!\n");
        return -1;
    }
    int block_num = len / BLOCK_SIZE + (len % BLOCK_SIZE != 0);
    for(int i = 0;i < block_num && len > 0;++i){
        uint64_t* block = GetPhysicalBlockNum(file, i);
        *block = D_GetFreeBlock();
        // printf("block: %ld\n", *block);
        if(*block == -1){
            printf("FAIL: No enough space!\nWrite error!\n");
            fprintf(fs_log, "FAIL: No enough space! Write error!\n");
            return -1;
        }
        // file.direct[i] = block;
        memcpy(disk + (*block) * BLOCK_SIZE, data + i * BLOCK_SIZE, MIN(BLOCK_SIZE, len));
        file->block_num++;
        len -= BLOCK_SIZE;
    }
    file->file_size = sz;
}

// w f l data  
int Write(char **args){
    // printf("Write called\n");
    char* file_name = args[1];
    uint64_t* inode_block = FindEntry(Current_Dir, file_name);
    if(inode_block == NULL){
        printf("FAIL: No such file!\nWrite error!\n");
        fprintf(fs_log, "FAIL: No such file! Write error!\n");
        return -1;
    }
    if(atoi(args[2]) < 0){
        printf("FAIL: Invalid length!\nWrite error!\n");
        fprintf(fs_log, "FAIL: Invalid length! Write error!\n");
        return -1;
    }
    struct Inode* file = (struct Inode*)(disk + (INODE_BLOCK + *inode_block) * BLOCK_SIZE);
    if(file->file_type == 1){
        printf("FAIL: Not a file!\nWrite error!\n");
        fprintf(fs_log, "FAIL: Not a file! Write error!\n");
        return -1;
    }
    FreeFile(file);
    int len = MIN(strlen(args[3]), atoi(args[2]));
    WriteAux(file, args[3], len);
    file->block_num = len / BLOCK_SIZE + (len % BLOCK_SIZE != 0);
    file->file_size = len;
    // printf("write to block %ld\n", file->direct[0]);
}

// i f pos l data 
// an easy implementation  
// write to a char array and call write
int Insert(char **args){
    // char* file_name = args[1];
    uint64_t pos = atoi(args[2]);
    uint64_t* inode_block = FindEntry(Current_Dir, args[1]);
    if(inode_block == NULL){
        printf("FAIL: No such file!\nInsert error!\n");
        fprintf(fs_log, "FAIL: No such file! Insert error!\n");   
        return -1;
    }
    if(pos < 0){
        printf("FAIL: Invalid position!\nInsert error!\n");
        fprintf(fs_log, "FAIL: Invalid position! Insert error!\n");
        return -1;
    }
    if(atoi(args[3]) < 0){
        printf("FAIL: Invalid length!\nInsert error!\n");
        fprintf(fs_log, "FAIL: Invalid length! Insert error!\n");
        return -1;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       
    }
    struct Inode* file = (struct Inode*)(disk + (INODE_BLOCK + *inode_block) * BLOCK_SIZE);
    if(file->file_type == 1){
        printf("FAIL: %s is a dir!\nInsert error!\n", args[1]);
        fprintf(fs_log, "FAIL: %s is a dir! Insert error!\n", args[1]);
    }
    int len = MIN(strlen(args[4]), atoi(args[3]));
    char* content = (char*)malloc(file->file_size + len + 1);
    memset(content, 0, file->file_size + len);
    int data_before_blk_num = pos / BLOCK_SIZE;
    // printf("data_before_blk_num: %d\n", data_before_blk_num);
    // int data_before_blk_num = atoi(args[2]) / BLOCK_SIZE + (atoi(args[2]) % BLOCK_SIZE != 0);
    // printf("physical block num: %d\n", *GetPhysicalBlockNum(file, data_before_blk_num));
    for(int i = 0;i < data_before_blk_num;++i){
        // printf("physical block num: %ld\n", *GetPhysicalBlockNum(file, i));
        memcpy(content + i * BLOCK_SIZE, disk + (*GetPhysicalBlockNum(file, i)) * BLOCK_SIZE, BLOCK_SIZE);
    }
    int offset = data_before_blk_num * BLOCK_SIZE;  
    // printf("%d\n", offset);
    // printf("physical block num: %ld\n", *GetPhysicalBlockNum(file, 0));
    memcpy(content + offset, disk + (*GetPhysicalBlockNum(file, data_before_blk_num) * BLOCK_SIZE), pos % BLOCK_SIZE);
    // printf("%s\n", content);
    offset += pos % BLOCK_SIZE;
    // printf("%d\n", offset);
    memcpy(content + offset, args[4], len);
    offset += len;
    // printf("%d\n", offset);
    // printf("%s\n", content);
    int trunc_sz = MIN(file->file_size - data_before_blk_num * BLOCK_SIZE, BLOCK_SIZE);
    int copy_sz = trunc_sz - pos % BLOCK_SIZE;
    if(pos % BLOCK_SIZE != 0)
        memcpy(content + offset, disk + (*GetPhysicalBlockNum(file, data_before_blk_num)) * BLOCK_SIZE + pos, copy_sz);
    // printf("%s\n", content);  
    offset += BLOCK_SIZE - pos % BLOCK_SIZE;
    for(int i = data_before_blk_num + 1;i < file->block_num;++i){
        memcpy(content + offset, disk + (*GetPhysicalBlockNum(file, i)) * BLOCK_SIZE, BLOCK_SIZE);
        offset += BLOCK_SIZE;
    }
    // printf("%s\n", content);
    content[file->file_size + len] = '\0';
    int tmp_sz = file->file_size;
    FreeFile(file);
    WriteAux(file, content, tmp_sz + len);
    // printf("write to block %ld\n", file->direct[0]);
    free(content);
}

// d f pos l 
int Delete(char **args){
    // char* file_name = args[1];
    uint64_t pos = atoi(args[2]);
    uint64_t* inode_block = FindEntry(Current_Dir, args[1]);
    if(inode_block == NULL){
        printf("FAIL: No such file %s!\nDelete error!\n", args[1]);
        fprintf(fs_log, "FAIL: No such file %s! Delete error!\n", args[1]);
        return -1;
    }
    struct Inode* file = (struct Inode*)(disk + (INODE_BLOCK + *inode_block) * BLOCK_SIZE);
    if(file->file_type == 1){
        printf("FAIL: Not a file!\nDelete error!\n");
        fprintf(fs_log, "FAIL: Not a file! Delete error!\n");
        return -1;
    }
    int len = MIN(file->file_size - pos, atoi(args[3]));
    assert(file->file_size >= len);
    char* content = (char*)malloc(file->file_size - len  + 1);
    int data_before_blk_num = pos / BLOCK_SIZE;
    for(int i = 0;i < data_before_blk_num;++i){
        memcpy(content + i * BLOCK_SIZE, disk + (*GetPhysicalBlockNum(file, i)) * BLOCK_SIZE, BLOCK_SIZE);
    }
    int offset = data_before_blk_num * BLOCK_SIZE;
    // offset += len;
    // data_before_blk_num = (atoi(args[2]) + len) / BLOCK_SIZE + ((atoi(args[2]) + len) % BLOCK_SIZE != 0);
    memcpy(content + offset, disk + (*GetPhysicalBlockNum(file, data_before_blk_num)) * BLOCK_SIZE, pos % BLOCK_SIZE);
    offset += pos % BLOCK_SIZE;
    int trunc_sz = MIN(file->file_size - data_before_blk_num * BLOCK_SIZE, BLOCK_SIZE);
    // memcpy(content + offset, disk + (*GetPhysicalBlockNum(file, data_before_blk_num - (atoi(args[2]) % BLOCK_SIZE != 0))) * BLOCK_SIZE, BLOCK_SIZE - atoi(args[2]) % BLOCK_SIZE);
    // offset += BLOCK_SIZE - atoi(args[2]) % BLOCK_SIZE;
    if(pos % BLOCK_SIZE + len < BLOCK_SIZE)
        memcpy(content + offset, disk + (*GetPhysicalBlockNum(file, data_before_blk_num)) * BLOCK_SIZE + pos % BLOCK_SIZE + len, trunc_sz - pos % BLOCK_SIZE - len);
    for(int i = data_before_blk_num + 1;i < file->block_num;++i){
        memcpy(content + offset, disk + (*GetPhysicalBlockNum(file, i)) * BLOCK_SIZE, BLOCK_SIZE);
        offset += BLOCK_SIZE;
    }
    // printf("%s\n", content);
    content[file->file_size - len] = '\0';
    int tmp_sz = file->file_size;
    FreeFile(file);
    WriteAux(file, content, tmp_sz - len);
    free(content);
    // printf("Delete success!\n");
}

// exit 
int Exit(char **args){
    ReturnBlock();
    printf("Goodbye\n");
    fprintf(fs_log, "Goodbye\n");
    free(disk);
    fclose(fs_log);
    exit(0);
}

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
};

void FileSystem(){
    char cmd[CMD_MAX_LEN];
    char* args[5];
    int sz = sizeof(cmd_table) / sizeof(cmd_table[0]);
    fs_log = fopen("fs.log", "w");
    if(fs_log == NULL){
        printf("Open fs.log error!\n");
        exit(0);
    }
    fprintf(fs_log, "Welcome to the file system!\n");
    fprintf(fs_log, "disk size: %d\n", DISK_SIZE);
    Format(NULL);
    while(1){
        // printf("$");
        printf("\033[34m%s\033[0m \033[32m$\033[0m ", CurrentDir_Inode->filename);
        // printf("\033[34m%s\033[0m \033[32m$\033[0m ", pwd);
        // scanf("%s", cmd);
        fgets(cmd, CMD_MAX_LEN, stdin);
        cmd[strcspn(cmd, "\r\n")] = '\0';
        int argc = Get_Args(cmd, args);
        // printf("argc: %d\tcmd: %s\n", argc, cmd);
        int i;
        for(i = 0;i < sz;++i){
            if(strcmp(args[0], cmd_table[i].cmd_name) == 0){
                if(cmd_table[i].handler != NULL){
                    if(argc != cmd_table[i].argc){
                        printf("%s\n", error_message[i]);
                        break;
                    }
                    cmd_table[i].handler(args);
                }
                break;
            }
        }
        if(i == sz)
            printf("FAIL: No such command!\n");
    }
}

int main(int argc, char* argv[]) {
    setbuf(stdout, NULL);
    // printf("%ld\n", sizeof(struct Inode));
    printf("Welcome to the file system!\n");
    assert(sizeof(struct Inode) == BLOCK_SIZE);
    printf("disk size: %d\n", DISK_SIZE);
    FileSystem();
    free(disk);
    fclose(fs_log);
    return 0;
}