#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
// #include <mutex>
#include <pthread.h>
#include <assert.h>

#define COND_INIT PTHREAD_COND_INITIALIZER
#define MUTEX_INIT PTHREAD_MUTEX_INITIALIZER
#define WAIT pthread_cond_wait
#define BROADCAST pthread_cond_broadcast
#define LOCK pthread_mutex_lock
#define UNLOCK pthread_mutex_unlock

#define CAN_DIG (dig > 0)
#define CAN_FILL (fill > 0)
#define CAN_SEED (seed > 0)

int dig, fill = 0, seed = 0;
int dig_cnt = 0, fill_cnt = 0, seed_cnt = 0;
#define COND pthread_cond_t
#define MUTEX pthread_mutex_t

COND DIG = COND_INIT, SEED = COND_INIT, FILL = COND_INIT, WHOLE = COND_INIT;
MUTEX larry = MUTEX_INIT, curly = MUTEX_INIT, moe = MUTEX_INIT;

#define L 3
#define C 6
#define M 9
#define NT (L + C + M)

pthread_t threads[NT];

void* Larry(void* arg){
    while(1){
        LOCK(&larry);
        while(!CAN_DIG){
            WAIT(&DIG, &larry);
        }
        dig--;
        seed++;
        printf("Larry dig hole #%d\n", dig_cnt++);
        assert(dig >= 0);
        BROADCAST(&SEED);
        UNLOCK(&larry);
    }
    pthread_exit(NULL);
}

void* Curly(void* arg){
    while(1){
        LOCK(&curly);
        while(!CAN_FILL){
            WAIT(&FILL, &curly);
        }
        dig++;
        fill--;
        assert(fill >= 0);
        printf("Curly fill hole #%d\n", fill_cnt++);
        assert(dig >= 0);
        BROADCAST(&DIG);
        UNLOCK(&curly);
    }
    pthread_exit(NULL);
}

void* Moe(void* arg){
    while(1){
        LOCK(&moe);
        while(!CAN_SEED){
            WAIT(&SEED, &moe);
        }
        fill++;
        seed--;
        assert(fill >= 0);
        printf("Moe plant a seed in #%d\n", seed_cnt++);
        assert(dig >= 0);
        BROADCAST(&FILL);
        UNLOCK(&moe);
    }
    pthread_exit(NULL);
}

int main(int argc, char* argv[]){
    setbuf(stdout, NULL);
    assert(argc == 2);
    dig = atoi(argv[1]);
    for(int i = 0;i < L;++i){
        pthread_create(&threads[i], NULL, Larry, NULL);
    }
    for(int i = L + 1;i < C;++i){
        pthread_create(&threads[i], NULL, Curly, NULL);
    }
    for(int i = C + 1;i < M;++i){
        pthread_create(&threads[i], NULL, Moe, NULL);
    }

    for(int i = 0;i < M;++i){
        pthread_join(threads[i], NULL);
    }

}