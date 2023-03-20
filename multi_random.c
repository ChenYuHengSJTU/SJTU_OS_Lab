#define _REENTRANT
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>

int n;
int cnt = 2;
#define MAXLEN 4096
int matrix[MAXLEN][MAXLEN];
int res[MAXLEN][MAXLEN];
// char filepath[MAXLEN] = "data.in";
// (m,n) for the index of a block 
// sz for single block size
void* multiply(void* Args){
    int *args = (int*)Args;
    int m1 = args[0], n1 = args[1], m2 = args[2], n2 = args[3], sz = args[4];
    // fprintf(stderr, "m1:%d n1:%d m2:%d n2:%d\n", m1, n1, m2, n2);
    int x1 = m1 * sz, y1 = n1 * sz, x2 = m2 * sz, y2 = n2 * sz;
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

    fprintf(stderr, "thread %d ends\n", m1 * cnt * cnt + n2 * cnt + n1);
    pthread_exit(NULL);
}

// use x86-64 rarand directive to generate real random number
uint64_t random_uint64(void) {
    uint64_t result;
    unsigned char ok;
    __asm__ __volatile__("rdrand %0; setc %1"
                         : "=r" (result), "=qm" (ok));
    if (ok) {
        return result;
    } else {
        // RDRAND failed, fall back to something else
        fprintf(stderr, "generate random number failed\n");
        return 0;
    }
}

// generate random matrix
void Generate(int sz){
    // srand((unsigned)time(NULL));

    // for(int i = 0;i < sz; ++i){
    //     for(int j = 0;j < sz; ++j){
    //         matrix[i][j] = rand() % 30;
    //     }
    // }

    for(int i = 0;i < sz;++i){
        for(int j = 0;j < sz;++j){
            matrix[i][j] = random_uint64() % 100;
        }
    }
    // uint64_t random_number = random_uint64() % 100;
}

static inline void __attribute__((destructor))
Clean(){
    // close(fp);
    // close(out);
}


int main(int argc, char* argv[]){
    for(int i = 0;i < MAXLEN; ++i){
        for(int j = 0;j < MAXLEN; ++j)
            res[i][j] = 0;
    }
    assert(argc == 2);
    Generate(atoi(argv[1]));

    int thread_num = cnt * cnt * cnt;
    pthread_t* threads = (pthread_t*)malloc(thread_num * sizeof(pthread_t));
    pthread_attr_t* attr = (pthread_attr_t*)malloc(thread_num * sizeof(pthread_attr_t));
    int** args = (int**)malloc(thread_num * sizeof(int*));

    for(int i = 0;i < thread_num; ++i){
        args[i] = (int*)malloc(5 * sizeof(int));
    }

    int sz = n/cnt;

    for(int i = 0;i < cnt; ++i){
        for(int j = 0;j < cnt; ++j){
            for(int k = 0;k < cnt; ++k){
                pthread_attr_setdetachstate(&attr[i * j + k], PTHREAD_CREATE_JOINABLE);
                pthread_attr_setscope(&attr[i * j + k], PTHREAD_SCOPE_SYSTEM);

                int index = i * cnt * cnt + j * cnt + k;

                args[index][0] = i;
                args[index][1] = k;
                args[index][2] = k;
                args[index][3] = j;
                args[index][4] = sz;

                if(pthread_create(&threads[index], &attr[index], multiply, (void*)(args[index])) != 0){
                    perror("create thread");
                }
            }
        }
    }

    struct timeval tv1, tv2;
    struct timezone tz1, tz2;
    gettimeofday(&tv1, &tz1);

    for(int i = 0;i < thread_num; ++i)
        pthread_join(threads[i], NULL);

    gettimeofday(&tv2, &tz2);

    fprintf(stderr, "over\n");

    FILE* out = fopen("sample_data_m.out", "w");

    for(int i = 0;i < n;++i){
        for(int j = 0;j < n; ++j){
            fprintf(out, "%d", res[i][j]);
            if(j == n-1){
                fprintf(out, "\n");
            }
            else fprintf(out, " ");
        }
    }

    fclose(out);

    printf("time consumed: %lums\n",tv2.tv_usec - tv1.tv_usec);

    free(threads);
    free(attr);
    for(int i = 0;i < thread_num;++i)
        free(args[i]);
    free(args);

    return 0;
}