#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<stdint.h>
#include<stdbool.h>
#include<assert.h>
#include<string.h>

#define FILENAMESIZE 128

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

#define FILENAME_SIZE 128
#define BLOCK_SIZE 256 
#define DIRECT_BLOCK 256
#define SECOND_INDIRECT_BLOCK 4 * 32 * 32




struct Inode{
    char filename[FILENAME_SIZE];
    uint64_t file_size;
    // here are only two types of file: dir and file
    // 32-bits size for expansion
    uint32_t file_type;
    // permission word , R or W or X 
    // need some more design 
    uint32_t permission;
    // reference for the file 
    // uint64_t link_count;
    // TODO get the time information
    uint32_t time;
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

// the disk ,we should divide it into blocks too 
char* disk = NULL;
// to share the code 
#define CYLINDER 50
#define SEC_PER_CYLINDER 200
#define DISK_SIZE CYLINDER * SEC_PER_CYLINDER * BLOCK_SIZE

// we may not need super block and log block here 
#define INODE_BLOCK 0
#define DATA_BLOCK CYLINDER * SEC_PER_CYLINDER / 3

// some block-related functions
// calculate the physical block num 
int GetPhysicalBlockNum(struct Inode* inode, int block_num){
    return inode->direct[block_num];
}

// write to the block (if can combined with read?)
// block num is the physical block num
// TODO consider the parameter, offset should be a must
int WriteBlock(uint64_t block_num, char* data, size_t n, size_t offset){
    // get the physical block num 
    // int physical_block_num = GetPhysicalBlockNum(block_num);
    // write to the disk 
    if(offset >= BLOCK_SIZE){
        fprintf(stderr, "FAIL: offset is too large\n");
        return -1;
    }
    memcpy(disk + block_num * BLOCK_SIZE + offset, data, BLOCK_SIZE);
}

// Initialize the Disk 
void Init_Block(){
    int block_sum = CYLINDER * SEC_PER_CYLINDER;
    struct Block* new_block = (struct Block*)malloc(sizeof(struct Block));
    i_free_block_list = new_block;
    i_free_block_tail = new_block;
    for(int i = 1;i < DATA_BLOCK;++i){
        new_block = (struct Block*)malloc(sizeof(struct Block));
        new_block->block_num = i;
        i_free_block_tail->nxt = new_block;
        i_free_block_tail = new_block;
    }

    new_block = (struct Block*)malloc(sizeof(struct Block));
    d_free_block_list = new_block;
    d_free_block_tail = new_block;
    for(int i = DATA_BLOCK;i < block_sum;++i){
        new_block = (struct Block*)malloc(sizeof(struct Block));
        new_block->block_num = i;
        d_free_block_tail->nxt = new_block;
        d_free_block_tail = new_block;
    }
}
// also some functions to maintain the free block list 
// get the free block
int I_GetFreeBlock(){
    if(i_free_block_list == NULL){
        return -1;
    }
    struct Block* tmp = i_free_block_list;
    i_free_block_list = i_free_block_list->nxt;
    int ret = tmp->block_num;
    free(tmp);
    return ret;
}

void I_ReturnBlock(int block_num){
    struct Block* new_block = (struct Block*)malloc(sizeof(struct Block));
    new_block->block_num = block_num;
    new_block->nxt = NULL;
    i_free_block_tail->nxt = new_block;
    i_free_block_tail = new_block;
    if(i_free_block_list == NULL)
        i_free_block_list = new_block;
}

int D_GetFreeBlock(){
    if(d_free_block_list == NULL){
        return -1;
    }
    struct Block* tmp = d_free_block_list;
    d_free_block_list = d_free_block_list->nxt;
    int ret = tmp->block_num;
    free(tmp);
    return ret;
}

// return the block to the free block list
void D_ReturnBlock(int block_num){
    struct Block* new_block = (struct Block*)malloc(sizeof(struct Block));
    new_block->block_num = block_num;
    new_block->nxt = NULL;
    d_free_block_tail->nxt = new_block;
    d_free_block_tail = new_block;
    if(d_free_block_list == NULL)
        d_free_block_list = new_block;
}

// TODO the duplicating create inode code should be integrated into one function
// to format the fs on the disk 
int Format(char *args){
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
    memset(root.direct, 0, sizeof(root.direct));
    memset(root.second_indirect, 0, sizeof(root.second_indirect));
    // root.third_indirect = 0;
    // write the root dir to the disk
    // use the unified API 
    // memcpy(disk + INODE_BLOCK * BLOCK_SIZE, &root, sizeof(root));
    WriteBlock(0, (char*)&root, sizeof(root), 0);
    Current_Dir = 0;
    root.direct[0] = 0;
    // -1 means no file
    // TODO may 0 is a no-worse choice
    root.direct[1] = -1;
}

// mk f 
// TODO we need to find a inode entry for each file and dir too by traversing the dir-inode
int MakeFile(char *args){
    struct Inode new_file;
    strcpy(new_file.filename, args);
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
        return -1;
    }
    // write the inode 
    WriteBlock(Inode_num, (char*)&new_file, sizeof(new_file), 0);
    // add the new file to the current dir
    // find the last block
    // int last_block = Current_Dir.block_num;
    // TODO how to use unified API is a difficult here, add file to a dir is the same with add sub-dir to a dir, both them could be implemented by writing blocks 
    // int physical_block = GetPhysicalBlockNum(&Current_Dir, last_block);
}

// mkdir d 
int MakeDir(char *args){
    struct Inode new_dir;
    strcpy(new_dir.filename, args);
    new_dir.file_size = 0;
    new_dir.file_type = 1;
    new_dir.permission = 0;
    // new_dir.link_count = 0;
    new_dir.block_num = 2;
    memset(new_dir.direct, 0, sizeof(new_dir.direct));
    memset(new_dir.second_indirect, 0, sizeof(new_dir.second_indirect));
    // new_dir.third_indirect = 0;
    // write the new dir to the disk
    // use the unified API
    int Inode_num = I_GetFreeBlock();
    if(Inode_num == -1){
        printf("FAIL: No enough space to create dir!\nDir and File number is up to the limit\n");
        return -1;
    }
    // write the inode
    WriteBlock(Inode_num, (char*)&new_dir, sizeof(new_dir), 0);
    // add the new dir to the current dir

}

// maybe block num of Inode is more useful
uint64_t* FindEntry(char* entry_name){
    struct Inode dir, tmp; 
    memcpy(&dir, disk + (INODE_BLOCK + Current_Dir) * BLOCK_SIZE, sizeof(dir));
    for(int i = 0;i < DIRECT_BLOCK;++i){
        if(dir.direct[i] == -1)
            continue;
        memcpy(&tmp, disk + (INODE_BLOCK + dir.direct[i]) * BLOCK_SIZE, sizeof(tmp));
        if(strcmp(tmp.filename, entry_name) == 0)
            return (&dir.direct[i]);
    }
    for(int i = 0;i < SECOND_INDIRECT_BLOCK;++i){
        uint64_t* second_block = (uint64_t*)(disk + (INODE_BLOCK + dir.second_indirect[i / (BLOCK_SIZE / 8)]));
        if(second_block[i % (BLOCK_SIZE / 8)] == -1)
            continue;
        memcpy(&tmp, second_block[i % (BLOCK_SIZE / 8)], sizeof(tmp));
        if(strcmp(tmp.filename, entry_name) == 0)
            return (&second_block[i % (BLOCK_SIZE / 8)]);
    }
    return NULL;
}


// TODO should consider the relative path and the absolute path
//  be integrated into the find function
bool EntryExist(char* entry_name){
    return FindEntry(entry_name) != NULL;
}
// rm f 
int RemoveFile(char *args){
    uint64_t* Inode_num = FindEntry(args);
    if(Inode_num == NULL){
        printf("FAIL: No such file!\nDeletion error!\n");
        return -1;
    }
    // delete the file from the current dir
    // TODO maybe it is not a good practice, but maybe faster
    *Inode_num = -1;
}

// rmdir d
int RemoveDir(char *args){
    uint64_t* Inode_num = FindEntry(args);
    if(Inode_num == NULL){
        printf("FAIL: No such dir!\nDeletion error!\n");
        return -1;
    }
    // delete the dir from the current dir
    // TODO maybe it is not a good practice
    *Inode_num = -1;
}

void PrintEntry(struct Inode* inode){
    assert(inode != NULL);
    // TODO we should consider the format using *. to implement it
    printf("%s\t", inode->filename);
    printf("%d\t", inode->file_size);
    if(inode->file_type == 0)
        printf("%d\t", "F");
    else 
        printf("%d\t", "D");
    
}

// ls
int List(char *args){
    struct Inode dir, tmp; 
    memcpy(&dir, disk + (INODE_BLOCK + Current_Dir) * BLOCK_SIZE, sizeof(dir));
    for(int i = 0;i < DIRECT_BLOCK;++i){
        if(dir.direct[i] == -1)
            continue;
        memcpy(&tmp, disk + (INODE_BLOCK + dir.direct[i]) * BLOCK_SIZE, sizeof(tmp));
        PrintEntry(&tmp);
    }
    for(int i = 0;i < SECOND_INDIRECT_BLOCK;++i){
        uint64_t* second_block = (uint64_t*)(disk + (INODE_BLOCK + dir.second_indirect[i / (BLOCK_SIZE / 8)]));
        if(second_block[i % (BLOCK_SIZE / 8)] == -1)
            continue;
        memcpy(&tmp, second_block[i % (BLOCK_SIZE / 8)], sizeof(tmp));
        PrintEntry(&tmp);
    }
}

// cat f 
int Cat(char *args){
    uint64_t* Inode_num = FindEntry(args);
    if(Inode_num == NULL){
        printf("FAIL: No such file!\nCat error\n");
        return -1;
    }
    struct Inode file;
    memcpy(&file, disk + (INODE_BLOCK + *Inode_num) * BLOCK_SIZE, sizeof(file));
    if(file.file_type == 1){
        printf("FAIL: %s is a dir!\nCat error\n", args);
        return -1;
    }
    char* content = (char*)malloc(file.file_size);
    int sz = file.file_size, i = 0;
    while(sz > 0){
        if(i < DIRECT_BLOCK){
            memcpy(content + (file.file_size - sz), disk + (INODE_BLOCK + file.direct[i]) * BLOCK_SIZE, BLOCK_SIZE);
            sz -= BLOCK_SIZE;
        }

    }
    memcpy(content, disk + (INODE_BLOCK + *Inode_num) * BLOCK_SIZE, file.file_size);
    printf("%s\n", content);
    free(content);
}

// w f l data  
int Write(char *args){

}

// i f pos l data 
int Insert(char *args){

}

// d f pos l 
int Delete(char *args){

}

// exit 
int Exit(char *args){

}

// for all cmds
static struct {
    const char* cmd_name;
    const char* cmd_usage;
    int (*handler) (char*);
} cmd_table [] = {
    {},
    {}
};

int main(int argc, char* argv[]) {
    assert(sizeof(struct Inode) == BLOCK_SIZE);
    return 0;
}