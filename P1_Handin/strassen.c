#define _REENTRANT
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
#include <sys/time.h>
#include <stdbool.h>
#include <time.h>

#define Matrix_Num 25

int n;
int cnt = 2;
int sz;
int matrix[4096][4096];
int volatile res[4096][4096];
int*** Matrix;
pthread_mutex_t mutex;
int args[7][4]={
    {0, 4, 14, 0},
    {5, 3, 15, 1},
    {6, 0, 16, 2},
    {3, 7, 17, 3},
    {8, 9, 18, 4},
    {10, 11, 19, 5},
    {12, 13, 20, 6}
};
// (m,n) for the index of a block 
// sz for single block size
void* multiply(void* Args){
    int *args = (int*)Args;
    int m = args[0], n = args[1], t = args[2], index = args[3];
    // fprintf(stderr, "m1:%d n1:%d m2:%d n2:%d\n", m1, n1, m2, n2);
    int ** M1 = Matrix[m], **M2 = Matrix[n], **R = Matrix[t];
    for(int i = 0; i < sz; ++i){
        // int tmp = 0;
        for(int j = 0; j < sz; ++j){
            int tmp = 0;
            for(int k = 0; k < sz ; ++k){
                pthread_mutex_lock(&mutex);
                tmp += M1[i][k] * M2[k][j];
                pthread_mutex_unlock(&mutex);
            }
            pthread_mutex_lock(&mutex);
            R[i][j] = tmp;
            pthread_mutex_unlock(&mutex);
        }
    }
    
    // fprintf(stderr, "thread %d ends\n", index);
    pthread_exit(NULL);
}

void Generate_matrix(int m, int u, int v, bool flag){
    for(int i = 0;i < sz;++i){
        for(int j = 0;j < sz;++j){
            if(flag) 
                Matrix[m][i][j] = Matrix[u][i][j] + Matrix[v][i][j];
            else    
                Matrix[m][i][j] = Matrix[u][i][j] - Matrix[v][i][j];
        }
    }
}

int main(int argc, char* argv[]){
    FILE* fp = fopen("data.in", "r");

    if(fp == NULL)
        perror("file open error");

    fscanf(fp, "%d", &n);

    assert(n > 0 & n <= 4096);

    fprintf(stderr, "%d\n", n);

    for(int i = 0; i < n ; ++i){
        for(int j = 0;j < n;++j)
            fscanf(fp, "%d", &matrix[i][j]);
    }

    fprintf(stderr, "read over\n");

    int thread_num = 7;
    pthread_t* threads = (pthread_t*)malloc(thread_num * sizeof(pthread_t));
    pthread_attr_t* attr = (pthread_attr_t*)malloc(thread_num * sizeof(pthread_attr_t));
    // int** args = (int**)malloc(thread_num * sizeof(int*));

    if(pthread_mutex_init(&mutex, NULL) != 0)
        perror("Init mutex error");

    sz = n / cnt;

    Matrix = (int***)malloc(sizeof(int**) * Matrix_Num);
    for(int i = 0;i < Matrix_Num; ++i){
        Matrix[i] = (int**)malloc(sizeof(int*) * sz);
        for(int j = 0;j < sz; ++j){
            Matrix[i][j] = (int*)malloc(sizeof(int) * sz);
        }
    }

    for(int k = 0;k < 4; ++k){
        int m =(k % cnt) * sz, n = (k / cnt) * sz;
        for(int i = 0;i < sz; ++i){
            for(int j = 0;j < sz; ++j){
                Matrix[k][i][j] = matrix[i + n][j + m];
            }
        }
    }
    
    // fprintf(stderr, "malloc over\n");

    Generate_matrix(4, 1, 3, false);
    Generate_matrix(5, 0, 1, true);
    Generate_matrix(6, 2, 3, true);
    Generate_matrix(7, 2, 0, false);
    Generate_matrix(8, 0, 3, true);
    Generate_matrix(9, 0, 3, true);
    Generate_matrix(10, 1, 3, false);
    Generate_matrix(11, 2, 3, true);
    Generate_matrix(12, 0, 2, false);
    Generate_matrix(13, 0, 1, true);

    // fprintf(stderr, "generate s p over\n");
    struct timeval tv1, tv2;
    struct timezone tz1, tz2;
    gettimeofday(&tv1, &tz1);

    for(int i = 0;i < 7; ++i){
        pthread_attr_setdetachstate(&attr[i], PTHREAD_CREATE_JOINABLE);
        pthread_attr_setscope(&attr[i], PTHREAD_SCOPE_SYSTEM);

        if(pthread_create(&threads[i], &attr[i], multiply, (void*)(args[i])) != 0){
            perror("create thread");
        }
    }

    for(int i = 0;i < thread_num; ++i)
        pthread_join(threads[i], NULL);

    gettimeofday(&tv2, &tz2);

    for(int i = 0; i < sz; ++i){
        for(int j = 0; j < sz; ++j){
            Matrix[21][i][j] = Matrix[18][i][j] + Matrix[17][i][j] - Matrix[15][i][j] + Matrix[19][i][j];
            Matrix[22][i][j] = Matrix[14][i][j] + Matrix[15][i][j];
            Matrix[23][i][j] = Matrix[16][i][j] + Matrix[17][i][j];
            Matrix[24][i][j] = Matrix[18][i][j] + Matrix[14][i][j] - Matrix[16][i][j] - Matrix[20][i][j];
        }
    }

    for(int i = 0; i < sz ; ++i){
        for(int j = 0; j < sz; ++j){
            res[i][j] = Matrix[21][i][j];
            res[i][j + sz] = Matrix[22][i][j];
            res[i + sz][j] = Matrix[23][i][j];
            res[i + sz][j + sz] = Matrix[24][i][j];
        }
    }

    FILE* out = fopen("data_s.out", "w");

    for(int i = 0;i < n;++i){
        for(int j = 0;j < n; ++j){
            fprintf(out, "%d ", res[i][j]);
        }
        fprintf(out, "\n");
    }

    for(int i = 0;i <= 24;++i){
        if(Matrix[i][0][0] == 0)
            printf("%d\t", i);
    }

    pthread_mutex_destroy(&mutex);
    printf("time consumed: %lums\n",tv2.tv_usec - tv1.tv_usec);

    return 0;
}