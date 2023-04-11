#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/time.h>

int n;
int matrix[4096][4096];
int res[4096][4096];
// (m,n) for the index of a block 
// sz for single block size
void multiply(int m, int n, int sz){
    int x = n * sz, y = m * sz;

    for(int i = x; i < x + sz; ++i){
        // int tmp = 0;
        for(int j = y; j < y + sz; ++j){
            int tmp = 0;
            for(int k = 0; k < sz ; ++k){
                tmp += matrix[i][k] * matrix[k][j];
            }
            res[i][j] += tmp;
        }
    }
}

void* Multiply(void* Args){
    int *args = (int*)Args;
    int m1 = args[0], n1 = args[1], m2 = args[2], n2 = args[3], sz = args[4];
    int x1 = m1 * sz, y1 = n1 * sz, x2 = m2 * sz, y2 = n2 * sz;
    // int u = m, v = n;
    for(int i = 0; i < sz; ++i){
        // int tmp = 0;
        for(int j = 0; j < sz; ++j){
            int tmp = 0;
            for(int k = 0; k < sz ; ++k){
                tmp += matrix[x1 + i][y1 + k] * matrix[x2 + k][y2 + j];
            }
            res[x1 + i][y2 + j] += tmp;
        }
    }

    // fprintf(stderr, "thread %d ends\n", m1 * n2 + n1);
    // pthread_exit(NULL);
}


int main(int argc, char* argv[]){
    if(argc != 1){
        fprintf(stderr, "Command line input error[1]\nUsage: ./single\n");
        exit(1);
    }

    FILE* fp = fopen("./data.in", "r");

    if(fp == NULL)
        perror("file open error");

    // memset(buf, 0, 4096);

    fscanf(fp, "%d", &n);

    assert(n > 0 & n <= 4096);

    fprintf(stderr, "%d\n", n);

    for(int i = 0; i < n ; ++i){
        for(int j = 0;j < n;++j)
            fscanf(fp, "%d", &matrix[i][j]);
    }

    fprintf(stderr, "read over\n");

    int args[5] = {0, 0, 0, 0, n};

    // multiply(0, 0, n);
    struct timeval tv1, tv2;
    struct timezone tz1, tz2;
    gettimeofday(&tv1, &tz1);

    Multiply((void*)args);

    gettimeofday(&tv2, &tz2);
    printf("time consumed: %lums\n",tv2.tv_usec - tv1.tv_usec);


    FILE* out = fopen("./data.out", "w");

    for(int i = 0;i < n;++i){
        for(int j = 0;j < n; ++j){
            fprintf(out, "%d ", res[i][j]);
        }
        fprintf(out, "\n");
    }

    return 0;
}