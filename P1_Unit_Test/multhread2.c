#define _REENTRANT
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
#include <sys/time.h>
// #include <atomic>

int n;
int cnt = 2;
#define MAXLEN 4096
int matrix[MAXLEN][MAXLEN];
int res[MAXLEN][MAXLEN];
pthread_mutex_t mutex;
// atomic_t res [4096][4096];
// (m,n) for the index of a block 
// sz for single block size
void* multiply(void* Args){
    int *args = (int*)Args;
    asm volatile ("" ::: "memory");
    int volatile m1 = args[0], n1 = args[1], m2 = args[2], n2 = args[3], sz = args[4];
    // fprintf(stderr, "m1:%d n1:%d m2:%d n2:%d\n", m1, n1, m2, n2);
    asm volatile ("" ::: "memory");
    int volatile x1 = m1 * sz, y1 = n1 * sz, x2 = m2 * sz, y2 = n2 * sz;
    for(int i = 0; i < sz; ++i){
        // int tmp = 0;
        for(int j = 0; j < sz; ++j){
            // asm volatile ("" ::: "memory");
            int volatile tmp = 0;
            // int volatile xx1 = x1 + i, yy2 = y2 + j;
            for(int k = 0; k < sz ; ++k){
                // asm volatile ("" ::: "memory");
                // int volatile yy1 = y1 + k, xx2 = x2 + k;
                // asm volatile ("" ::: "memory");
                // tmp += matrix[xx1][yy1] * matrix[xx2][yy2];
                // __sync_fetch_and_add()
                pthread_mutex_lock(&mutex);
                tmp += matrix[x1 + i][y1 + k] * matrix[x2 + k][y2 + j];
                pthread_mutex_unlock(&mutex);
            }
            // asm volatile ("" ::: "memory");
            pthread_mutex_lock(&mutex);
            res[x1 + i][y2 + j] += tmp;
            pthread_mutex_unlock(&mutex);
            // asm volatile ("" ::: "memory");
        }
    }

    fprintf(stderr, "thread %d ends\n", m1 * cnt * cnt + n2 * cnt + n1);
    // pthread_detach(pthread_self());
    pthread_exit(NULL);
}

void Generate(int sz){
    srand((unsigned)time(NULL));

    for(int i = sz;i < sz; ++i){
        for(int j = sz;j < sz; ++j){
            matrix[i][j] = rand() % 30;
        }
    }
}

int main(int argc, char* argv[]){
    for(int i = 0;i < MAXLEN; ++i){
        for(int j = 0;j < MAXLEN; ++j)
            res[i][j] = 0;
    }

    FILE* fp = fopen("P1/sample/sample_data.in", "r");
    FILE* out = fopen("random.out", "w");

    if(argc == 2){
        // goto generate;
        Generate(atoi(argv[1]));

        int n = atoi(argv[1]);

        fprintf(out, "%d\n", n);


        for(int i = 0;i < n; ++i){
            for(int j = 0;j < n;++j)
                fprintf(out, "%d %d %d\n", i, j, matrix[i][j]);
        }

        fclose(fp);
    }
    else{

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
        fclose(fp);
    }

    int thread_num = cnt * cnt * cnt;
    pthread_t* threads = (pthread_t*)malloc(thread_num * sizeof(pthread_t));
    pthread_attr_t* attr = (pthread_attr_t*)malloc(thread_num * sizeof(pthread_attr_t));
    int** args = (int**)malloc(thread_num * sizeof(int*));

    for(int i = 0;i < thread_num; ++i){
        args[i] = (int*)malloc(5 * sizeof(int));
    }

    if(pthread_mutex_init(&mutex, NULL) != 0)
        perror("Init mutex error");

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

    for(int i = 0;i < thread_num; ++i){
        pthread_join(threads[i], NULL);
        // sleep(1);
    }
    for(int i = 0;i < thread_num; ++i){
        pthread_attr_destroy(&attr[i]);
    }

    // FILE* out = fopen("sample_data_m.out", "w");

    for(int i = 0;i < n;++i){
        for(int j = 0;j < n; ++j){
            if(argc == 2)
                fprintf(out, "%d %d %d\n", i, j, res[i][j]);
            else{
                fprintf(out, "%d", res[i][j]);

                if(j == n-1){
                    fprintf(out, "\n");
                }
                else fprintf(out, " ");
            }
        }
    }

    free(threads);
    free(attr);
    for(int i = 0;i < thread_num;++i)
        free(args[i]);
    free(args);

    pthread_mutex_destroy(&mutex);
    fclose(out);

    // fclose(fp);
    return 0;
}