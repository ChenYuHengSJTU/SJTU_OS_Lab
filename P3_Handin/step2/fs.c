#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<stdint.h>
#include<stdbool.h>
#include<assert.h>
#include<string.h>
#include<time.h>
#include<regex.h>

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
#define CMD_MAX_LEN 1024
#define ENTRY_NUM 32
#define MAX_ENTRY_NUM 8 + 4 * 32
// this can be calculated from the max amount of the disk
#define MAX_DIR_DEPTH 128
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define ARR_LEN(array) (sizeof(array) / sizeof(array[0]))
#define ROUND_UP(n) (((n >> 8) + 1) << 8)
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
// int Get_Args(char* cmd, char* args[]){
//     // printf("cmd=%s\n", cmd);
//     int cnt = 0, tot = 3, flag = 0;
//     char p[CMD_MAX_LEN] = "";
//     strncpy(p, cmd, CMD_MAX_LEN);
//     // char* p = cmd;
//     char* tmp = strtok(p, " ");
//     // char* tmp = strsep(&p, " ");
//     if(tmp == NULL){
//         // printf("tmp is NULL\n");
//         return -1;
//     }
//     if(strcmp(tmp, "w") == 0){
//         tot = 2;
//         flag = 1;
//     }
//     else if(strcmp(tmp, "i") == 0){
//         flag = 1;
//     }
//     args[cnt++] = tmp;
//     // printf("tot=%d\n", tot);
//     while((tmp = strtok(NULL, " ")) != NULL && tot > 0){
//     // while((tmp = strsep(&p, " ")) != NULL && tot > 0){
//         args[cnt++] = cmd + (size_t)(tmp - p);
//         // tmp = strtok(NULL, " ");
//         cmd[tmp - p + strlen(tmp)] = '\0';
//         tot--;
//         // printf("cnt=%d\n", cnt);
//     }
//     fflush(stdout);
//     if(flag && tmp != NULL){
//         // printf("meet i or w\n");
//         // tmp = strtok(NULL, " ");
//         args[cnt++] = cmd + (size_t)(tmp - p);
//         // printf("args[cnt - 1]=%s\n", args[cnt - 1]);
//     }
//     return cnt;
// }

char* regex_cmd[]={
    "^(f)$",
    "^(mk) (\\w+)$",
    "^(rm) (\\w+)$",
    "^(cd) ([./a-zA-Z0-9]+)$",
    "^(mkdir) (\\w+)$",
    "^(rmdir) (\\w+)$",
    "^(cat) (\\w+)$",
    "^(ls)$",
    "^(w) (\\w+) ([0-9]+) ([^\n]+)$",
    "^(i) (\\w+) ([0-9]+) ([0-9]+) ([^\n]+)$",
    "^(d) (\\w+) ([0-9]+) ([0-9]+)$",
    "^(e)$"
};
int Argc[] = {1, 2, 2, 2, 2, 2, 2, 1, 4, 5, 4, 1};
regex_t regex[ARR_LEN(regex_cmd)];
regmatch_t pmatch[6];

int Init(){
    for(int i = 0; i < ARR_LEN(regex_cmd); i++){
        if(regcomp(&regex[i], regex_cmd[i], REG_EXTENDED) != 0){
            printf("regex compile %d error\n", i);
            return -1;
        }
    }
}

int Get_Args(char* cmd, char* args[]){
    // printf("cmd=%s\n", cmd);
    int cnt = 0;
    for(int i = 0; i < ARR_LEN(regex_cmd); i++){
        // printf("try to match %s %d\n", regex_cmd[i], Argc[i]);
        if(regexec(&regex[i], cmd, Argc[i] + 1, pmatch, 0) == 0){
            // printf("match %s\n", regex_cmd[i]);
            for(int j = 1; j < Argc[i] + 1; j++){
                if(pmatch[j].rm_so == -1){
                    break;
                }
                // printf("pmatch[%d].rm_so=%d\n", j, pmatch[j].rm_so);
                // printf("pmatch[%d].rm_eo=%d\n", j, pmatch[j].rm_eo);
                args[cnt++] = cmd + pmatch[j].rm_so;
                cmd[pmatch[j].rm_eo] = '\0';
                // printf("args[%d]=%s\n", cnt - 1, args[cnt - 1]);
                // printf("args[%d]=%s\n", cnt - 1, args[cnt - 1]);
            }
            // regfree(&regex[i]);
            // return Argc[i];
            break;
        }
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

// the inode bitmap and data bitmap
// the most significant bit is not in use 
// use tree array to organize the bitmap
// since the tree array represent the sum of a range of bits, so it is uint64, but single element is uint32
#define I_LEN 4096
#define D_LEN (4096 << 2)
uint64_t inode_bitmap[I_LEN + 1];
uint64_t data_bitmap[D_LEN + 1];

void InitBitmap(){
    printf("InitBitmap Done\nInode bitmap use %d blocks\nData bitmap use %d blocks\n", DATA_BLOCK / 31 + 1, (CYLINDER * SEC_PER_CYLINDER - DATA_BLOCK) / 31 + 1);
}

#define LOWBIT(x) ((x) & (-x))
#define I_BITMAP_LEN (DATA_BLOCK / 31 + 1)
#define D_BITMAP_LEN ((CYLINDER * SEC_PER_CYLINDER - DATA_BLOCK) / 31 + 1)

void I_AddAux(uint32_t x, int32_t k){
    while(x <= I_LEN){
        // printf("x = %d\n", x); 
        inode_bitmap[x] = (int32_t)inode_bitmap[x] + k;
        x += LOWBIT(x);
    }
}

uint64_t I_GetSumAux(uint32_t k){
    uint64_t sum = 0;
    assert(k <= I_LEN);
    while(k > 0){
        sum += inode_bitmap[k];
        k -= LOWBIT(k);
    }
    return sum;
}

uint32_t I_GetElem(uint32_t i){
    return I_GetSumAux(i) - I_GetSumAux(i- 1);
}

void D_AddAux(uint32_t x, int32_t k){
    while(x <= D_LEN){
        data_bitmap[x] = (uint32_t)data_bitmap[x] + k;
        x += LOWBIT(x);
    }
}

uint64_t D_GetSumAux(uint32_t k){
    uint64_t sum = 0;
    assert(k <= D_LEN);
    while(k > 0){
        sum += data_bitmap[k];
        k -= LOWBIT(k);
    }
    return sum;
}

uint32_t D_GetElem(uint32_t i){
    return D_GetSumAux(i) - D_GetSumAux(i- 1);
}

// get the sum of the first k bits
// TODO 
#define I_FULL 0x7fffffff
uint64_t I_GetFreeBlock(){
    int l = 1;
    int r = I_LEN;
    int mid;

    while(1){
        // l == r
        if(l == r){
            break;
        }
        mid = (l + r) >> 1;
        if(I_GetSumAux(mid) == I_FULL * mid){
            l = mid + 1;
        }
        else{
            r = mid;
        }
        // printf("mid = %d\n", mid);
    }

    // printf("l = %d\n", l);
    // printf("r = %d\n", r);
    // printf("I_GetElem(l) = %d\n", I_GetElem(l));
    // printf("%u %u %u\n" , inode_bitmap[1], inode_bitmap[2], inode_bitmap[3]);
    assert(l == r);

    int free_byte = r;
    for(;free_byte <= r + LOWBIT(r); ++free_byte){
        // printf("free_byte = %d\n", free_byte);
        if(I_GetElem(free_byte) != I_FULL){
            break;
        }
    }

    assert(I_GetElem(free_byte) != I_FULL);
    // printf("free_byte = %d\n", free_byte);
    assert(free_byte <= r + LOWBIT(r) && free_byte >= 0);
    int free_bit = 0;
    int tmp = I_GetElem(free_byte);
    while((tmp & (1 << free_bit)) && free_bit < 31){
        // printf("free_bit = %d\n", free_bit); 
        ++free_bit;
    }
    assert(free_bit < 31);

    I_AddAux(free_byte, 1 << free_bit);

    // printf("free_byte = %d, free_bit = %d\n", free_byte, free_bit);
    // printf("free block num: %d\n", free_byte * 31 + free_bit);

    // printf("free_bit = %d\n", free_bit); 
    fprintf(fs_log, "\033[32m\033[1mAllocate free inode block: %d\033[0m\n", (free_byte - 1) * 31 + free_bit);
    // printf("Allocate free inode block: %d\n", (free_byte - 1) * 31 + free_bit);

    return (free_byte - 1) * 31 + free_bit;
}

void I_ReturnBlock(uint64_t block_num){
    // printf("block_num = %d\n", block_num);
    int byte = block_num / 31 + 1;
    // block_num += 1;
    int bit = block_num % 31;

    // printf("byte = %d, bit = %d\n", byte, bit);
    // printf("I_GetElem(byte) = %d\n", I_GetElem(byte));

    assert(I_GetElem(byte) & (1 << bit));

    I_AddAux(byte, -(1 << bit));
}

#define D_FULL 0x7fffffff
uint64_t D_GetFreeBlock(){
    int l = 1;
    int r = D_LEN;
    int mid;

    while(1){
        // l == r
        if(l == r){
            break;
        }
        mid = (l + r) >> 1;
        if(D_GetSumAux(mid) == D_FULL * mid){
            l = mid + 1;
        }
        else{
            r = mid;
        }
        // printf("mid = %d\n", mid);
    }

    // printf("l = %d\n", l);
    // printf("r = %d\n", r);
    // printf("I_GetElem(l) = %d\n", D_GetElem(l));
    // printf("%u %u %u\n" , inode_bitmap[1], inode_bitmap[2], inode_bitmap[3]);
    assert(l == r);

    int free_byte = r;
    for(;free_byte <= r + LOWBIT(r); ++free_byte){
        // printf("free_byte = %d\n", free_byte);
        if(D_GetElem(free_byte) != D_FULL){
            break;
        }
    }

    assert(D_GetElem(free_byte) != D_FULL);
    // printf("free_byte = %d\n", free_byte);
    assert(free_byte <= r + LOWBIT(r) && free_byte >= 0);
    int free_bit = 0;
    int tmp = D_GetElem(free_byte);
    while((tmp & (1 << free_bit)) && free_bit < 31){
        // printf("free_bit = %d\n", free_bit); 
        ++free_bit;
    }
    assert(free_bit < 31);

    D_AddAux(free_byte, 1 << free_bit);

    // printf("free_byte = %d, free_bit = %d\n", free_byte, free_bit);
    // printf("free block num: %d\n", free_byte * 31 + free_bit);

    // printf("free_bit = %d\n", free_bit); 
    fprintf(fs_log, "\033[32m\033[1mAllocate free data block: %d\033[0m\n", (free_byte - 1) * 31 + free_bit + DATA_BLOCK);
    // printf("Allocate free inode block: %d\n", (free_byte - 1) * 31 + free_bit);

    return (free_byte - 1) * 31 + free_bit + DATA_BLOCK;
}

void D_ReturnBlock(uint64_t block_num){
    // block_num += 1;
    block_num -= DATA_BLOCK;
    int byte = block_num / 31 + 1;
    int bit = block_num % 31;
    assert(D_GetElem(byte) & (1 << bit));
    D_AddAux(byte, -(1 << bit));
}

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
    // if(!flag)
        memcpy(disk + (INODE_BLOCK + block_num) * BLOCK_SIZE + offset, data, n);
    // else 
        // memcpy(disk + (DATA_BLOCK + block_num) * BLOCK_SIZE + offset, data, n);
}

// read from the block
int ReadBlock(uint64_t block_num, char* data, size_t n, size_t offset, int flag){
    if(offset >= BLOCK_SIZE){
        fprintf(stderr, "FAIL: offset is too large\n");
        return -1;
    }
    // if(!flag)
        memcpy(data, disk + (INODE_BLOCK + block_num) * BLOCK_SIZE + offset, n);
    // else 
        // memcpy(data, disk + (DATA_BLOCK + block_num) * BLOCK_SIZE + offset, n);
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
// uint64_t I_GetFreeBlock(){
//     if(i_free_block_list == NULL){
//         return -1;
//     }
//     struct Block* tmp = i_free_block_list;
//     i_free_block_list = i_free_block_list->nxt;
//     uint64_t ret = tmp->block_num;
//     free(tmp);
// 	fprintf(fs_log ,"allocate a inode block: %ld\n", ret);
//     return ret;
// }

// void I_ReturnBlock(uint64_t block_num){
//     struct Block* new_block = (struct Block*)malloc(sizeof(struct Block));
//     new_block->block_num = block_num;
//     new_block->nxt = NULL;
//     i_free_block_tail->nxt = new_block;
//     i_free_block_tail = new_block;
//     if(i_free_block_list == NULL)
//         i_free_block_list = new_block;
// 	fprintf(fs_log ,"return a inode block: %ld\n", block_num);
// }

// uint64_t D_GetFreeBlock(){
//     if(d_free_block_list == NULL){
//         return -1;
//     }
//     struct Block* tmp = d_free_block_list;
//     d_free_block_list = d_free_block_list->nxt;
//     uint64_t ret = tmp->block_num;
//     // assert(tmp != NULL && ret >= DATA_BLOCK);
//     free(tmp);
// 	fprintf(fs_log ,"allocate a data block: %ld\n", ret);
//     return ret;
// }

// // return the block to the free block list
// void D_ReturnBlock(uint64_t block_num){
//     struct Block* new_block = (struct Block*)malloc(sizeof(struct Block));
//     new_block->block_num = block_num;
//     new_block->nxt = NULL;
//     d_free_block_tail->nxt = new_block;
//     d_free_block_tail = new_block;
//     if(d_free_block_list == NULL)
//         d_free_block_list = new_block;
// 	fprintf(fs_log ,"return a data block: %ld\n", block_num);
// }

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
    // Init_Block();
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
	fprintf(fs_log ,"\033[32mFormat Done!\033[0m\n");
    printf("\033[32mFormat Done!\033[0m\n");
    CurrentDir_Inode = (struct Inode*)(disk +(INODE_BLOCK + Current_Dir) * BLOCK_SIZE);
    return 0;
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
            dir->second_indirect[j] = I_GetFreeBlock();
            uint64_t* indirect_block = (uint64_t*)(disk + (INODE_BLOCK + dir->second_indirect[j]) * BLOCK_SIZE);
            indirect_block[0] = inode_num;
            // detail
            for(int i = 1;i < ENTRY_NUM;++i)
                indirect_block[i] = -1;
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
    return 0;
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
    return 0;
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
    // printf("block_num: %d\n", block_num);
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
    // int block_num = dir->block_num;
    assert(dir != NULL);
    assert(dir->file_type == 1);
    for(int i = 2;i < DIRECT_BLOCK;++i){
        if(dir->direct[i] == -1)
            continue;
        struct Inode* tmp = (struct Inode*)(disk + (INODE_BLOCK + dir->direct[i]) * BLOCK_SIZE);
        if(tmp->file_type == 1){
            FreeDir(tmp);
        }
        else{
            FreeFile(tmp);
        }
        I_ReturnBlock(dir->direct[i]);
        // I_ReturnBlock(*GetPhysicalBlockNum(dir, i));
    }
    for(int j = 0;j < 4;++j){
        if(dir->second_indirect[j] == -1)
            continue;
        // assert(0);
        uint64_t* second_block = (uint64_t*)(disk + (INODE_BLOCK + dir->second_indirect[j]) * BLOCK_SIZE);
        for(int i = 0;i < ENTRY_NUM;++i){
            if(second_block[i] == -1)
                continue;
            struct Inode* tmp = (struct Inode*)(disk + (INODE_BLOCK + second_block[i]) * BLOCK_SIZE);
            if(tmp->file_type == 1){
                FreeDir(tmp);
            }
            else{
                FreeFile(tmp);
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
        // Inode_num = FindEntry(Current_Dir, args[1]);
        // if(Inode_num != NULL){

        // }
        printf("FAIL: %s is not a dir\n", dir->filename);
        fprintf(fs_log, "FAIL: %s is not a dir\n", dir->filename);
        return -1;
    }
    FreeDir(dir);
    I_ReturnBlock(*Inode_num);
    *Inode_num = -1;
    return 0;
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
    return 0;
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
    return 0;
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

void ListAux(struct Inode dir){
    struct Inode dir_to_print[MAX_ENTRY_NUM], file_to_print[MAX_ENTRY_NUM];
    int dir_cnt = 0, file_cnt = 0;
    struct Inode tmp;
    for(int i = 2;i < DIRECT;++i){
        if(dir.direct[i] == -1)
            continue;
        // printf("%d\n%ld\t%ld\n", i, dir.direct[i], (INODE_BLOCK + dir.direct[i]) * BLOCK_SIZE);
        assert((INODE_BLOCK + dir.direct[i]) * BLOCK_SIZE <= DISK_SIZE);
        memcpy(&tmp, disk + (INODE_BLOCK + dir.direct[i]) * BLOCK_SIZE, sizeof(tmp));
        if(tmp.file_type == 0)
            file_to_print[file_cnt++] = tmp;
        else
            dir_to_print[dir_cnt++] = tmp;
        // PrintEntry(&tmp);
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
        // PrintEntry(&tmp);
        if(tmp.file_type == 0)
            file_to_print[file_cnt++] = tmp;
        else
            dir_to_print[dir_cnt++] = tmp;
    } 

    // in lexicography order
    for(int i = 0;i < dir_cnt;++i){
        for(int j = i + 1;j < dir_cnt;++j){
            if(strcmp(dir_to_print[i].filename, dir_to_print[j].filename) > 0){
                struct Inode tmp = dir_to_print[i];
                dir_to_print[i] = dir_to_print[j];
                dir_to_print[j] = tmp;
            }
        }
    }

    for(int i = 0;i < file_cnt;++i){
        for(int j = i + 1;j < file_cnt;++j){
            if(strcmp(file_to_print[i].filename, file_to_print[j].filename) > 0){
                struct Inode tmp = file_to_print[i];
                file_to_print[i] = file_to_print[j];
                file_to_print[j] = tmp;
            }
        }
    }

    for(int i = 0;i < file_cnt;++i){
        PrintEntry(&file_to_print[i]);
    }

    for(int i = 0;i < dir_cnt;++i){
        PrintEntry(&dir_to_print[i]);
    }
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

    assert(dir.direct[0] != -1 && dir.direct[1] != -1);
    memcpy(&tmp, disk + (INODE_BLOCK + dir.direct[0]) * BLOCK_SIZE, sizeof(tmp));
    printf(".\t");
    printf("%ld\t", tmp.file_size);
    printf("%s\t", "D");
    printf("%s\t", tmp.Time);
    printf("\n");

    memcpy(&tmp, disk + (INODE_BLOCK + dir.direct[1]) * BLOCK_SIZE, sizeof(tmp));
    printf("..\t");
    printf("%ld\t", tmp.file_size);
    printf("%s\t", "D");
    printf("%s\t", tmp.Time);
    printf("\n");

    // for(int i = 2;i < DIRECT;++i){
    //     if(dir.direct[i] == -1)
    //         continue;
    //     // printf("%d\n%ld\t%ld\n", i, dir.direct[i], (INODE_BLOCK + dir.direct[i]) * BLOCK_SIZE);
    //     assert((INODE_BLOCK + dir.direct[i]) * BLOCK_SIZE <= DISK_SIZE);
    //     memcpy(&tmp, disk + (INODE_BLOCK + dir.direct[i]) * BLOCK_SIZE, sizeof(tmp));
    //     PrintEntry(&tmp);
    // }
    // for(int i = 0;i < SECOND_INDIRECT_BLOCK;++i){
    //     if(dir.second_indirect[i / ENTRY_NUM] == -1)
    //         continue;
    //     // printf("%d\n", i);
    //     assert((INODE_BLOCK + dir.second_indirect[i / ENTRY_NUM]) * BLOCK_SIZE < DISK_SIZE);
    //     uint64_t* second_block = (uint64_t*)(disk + (INODE_BLOCK + dir.second_indirect[i / ENTRY_NUM] * BLOCK_SIZE));
    //     if(second_block[i % ENTRY_NUM] == -1)
    //         continue;
    //     memcpy(&tmp, disk + (second_block[i % ENTRY_NUM] + INODE_BLOCK) * BLOCK_SIZE, sizeof(tmp));
    //     PrintEntry(&tmp);
    // }

    ListAux(dir);
    return 0;
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
    // printf("file size: %ld\n", file.file_size);
    char* content = (char*)malloc(file.file_size + 1);
    // printf("content size: %ld\n", file.file_size + 1);
    memset(content, 0, file.file_size + 1);
    int sz = file.file_size, offset = 0;
    // printf("block num: %ld\n", file.block_num);
    assert(file.file_size / BLOCK_SIZE + (file.file_size % BLOCK_SIZE != 0) == file.block_num);
    for(int i = 0;i < DIRECT_BLOCK && sz > 0;++i){
        int len = MIN(sz, BLOCK_SIZE);
        memcpy(content + offset, disk + (file.direct[i]) * BLOCK_SIZE, len);
        sz -= len;
        offset += len;
    }
    for(int i = 0;i < SECOND_INDIRECT_BLOCK / ENTRY_NUM && sz > 0;++i){
        uint64_t* second_block = (uint64_t*)(disk + (INODE_BLOCK + file.second_indirect[i]) * BLOCK_SIZE);
        for(int j = 0;j < ENTRY_NUM && sz > 0;++j){
            int len = MIN(sz, BLOCK_SIZE);
            memcpy(content + offset, disk + (second_block[j]) * BLOCK_SIZE, len);
            sz -= len;
            offset += len;
        }
    }
    printf("%s\n", content);
    fprintf(fs_log, "%s\n", content);
    free(content);
    return 0;
}

int WriteAux(struct Inode* file, char* data, int len){
    // printf("write aux: %s\n", data);
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
    return 0;
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
    pos = MIN(pos, file->file_size);
    if(file->file_type == 1){
        printf("FAIL: %s is a dir!\nInsert error!\n", args[1]);
        fprintf(fs_log, "FAIL: %s is a dir! Insert error!\n", args[1]);
    }
    int len = MIN(strlen(args[4]), atoi(args[3]));
    // len = MIN(len, file->file_size - pos + 1);
    // printf("malloc %d\n", file->file_size + len + 1);
    char* content = (char*)malloc(ROUND_UP(file->file_size + len + 1));
    memset(content, 0, file->file_size + len + 1);
    int tmp_sz = file->file_size;
    for(int i = 0;i < file->block_num;++i){
        memcpy(content + i * BLOCK_SIZE, disk + (*GetPhysicalBlockNum(file, i)) * BLOCK_SIZE, BLOCK_SIZE);
    }
    memcpy(content + pos + len, content + pos, file->file_size - pos + 1);
    memcpy(content + pos, args[4], len);
    FreeFile(file);
    WriteAux(file, content, tmp_sz + len);
    // printf("write to block %ld\n", file->direct[0]);
    free(content);
    return 0;
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
    // int rest_len = file->file_size - atoi(args[3]);
    // if(file->file_size < len){
    //     // printf("FAIL: Invalid position!\nDelete error!\n");
    //     fprintf(stderr, "FAIL: Invalid position! Delete error!\n");
    //     fflush(stderr);
    //     return -1;
    // }
    assert(file->file_size >= len);
    // content[file->file_size - len] = '\0';
    int tmp_sz = file->file_size;
    char* content = (char*)malloc(ROUND_UP(file->file_size + 1));
    memset(content, 0, file->file_size + 1);
    for(int i = 0;i < file->block_num; ++i){
    // for(int offset = 0;offset <= file->file_size;offset += BLOCK_SIZE){{
        memcpy(content + i * BLOCK_SIZE, disk + (*GetPhysicalBlockNum(file, i)) * BLOCK_SIZE, BLOCK_SIZE);
    // }
    }
    memcpy(content + pos, content + MIN(file->file_size, pos + len), 1 + MAX(0, file->file_size - pos - len));
    FreeFile(file);
    WriteAux(file, content, tmp_sz - len);
    // printf("written!\n");
    free(content);
    // printf("Delete : %d\n", file->file_size);
    // memcpy(disk + (*inode_block + INODE_BLOCK) * BLOCK_SIZE, file, sizeof(struct Inode));
    // printf("Delete success!\n");
    return 0;
}

// exit 
int Exit(char **args){
    // ReturnBlock();
    printf("Goodbye\n");
    fprintf(fs_log, "Goodbye\n");
    free(disk);
    fclose(fs_log);
    for(int i = 0; i < ARR_LEN(regex_cmd); i++){
        regfree(&regex[i]);
    }
    exit(0);
}

int PrintHelp(char** args);

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
    {"w", "write f l data\n", Write, 4},
    {"i", "insert f pos l data\n", Insert, 5},
    {"d", "delete f pos l\n", Delete, 4},
    {"e", "exit\n", Exit, 1},
    {"pwd", "print current working dir\n", PrintCurDir, 1},
    {"help", "show help information\n", PrintHelp, 1},
};

int PrintHelp(char** args){
    #define CMD_NUM sizeof(cmd_table) / sizeof(cmd_table[0])
    for(int i = 0;i < CMD_NUM;++i){
        printf("%s: %s", cmd_table[i].cmd_name, cmd_table[i].cmd_usage);
    }
}

void FileSystem(){
    Init();

    char cmd[CMD_MAX_LEN];
    char* args[5];
    int sz = sizeof(cmd_table) / sizeof(cmd_table[0]);
    fs_log = fopen("fs.log", "w+");
    setbuf(fs_log, NULL);

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

        fprintf(fs_log, "\033[34m%s\033[0m", cmd);

        cmd[strcspn(cmd, "\r\n")] = '\0';
        int argc = Get_Args(cmd, args);
        // printf("argc: %d\tcmd: %s\n", argc, cmd);
        int i;
        int res;
        for(i = 0;i < sz;++i){
            if(strcmp(args[0], cmd_table[i].cmd_name) == 0){
                if(cmd_table[i].handler != NULL){
                    if(argc != cmd_table[i].argc){
                        printf("%s\n", error_message[i]);
                        break;
                    }
                    res = cmd_table[i].handler(args);
                    if(res == -1){
                        printf("\033[31mNO\033[0m\n");
                    } 
                    else{
                        printf("\033[32mYES\033[0m\n");
                    }
                }
                break;
            }
        }
        if(i == sz){
            printf("FAIL: No such command!\n%s\n", cmd);
        }
    }
}

__attribute((constructor))
void CleanUp(){
    for(int i = 0; i < ARR_LEN(regex_cmd); i++){
        regfree(&regex[i]);
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