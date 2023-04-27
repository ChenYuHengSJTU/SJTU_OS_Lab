#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <sched.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

sem_t enter, to_confirm, leave, get_certificated, confirmed, sit_down;
pthread_mutex_t immigrant_lock, judge_lock, spectator_lock, lock;

#define NT_Imm 5
#define NT_Jg 1
#define NT_Sp 2

#define NT NT_Imm + NT_Jg + NT_Sp

pthread_t immigrants[NT_Imm];
pthread_t judges[NT_Jg];
pthread_t spectators[NT_Sp];

int confirmed_immigrants[NT_Imm];

// int immigrant = 0, judge = 0, spectator = 0, enter_in = 0, to_confirm = 0, confirmed = 0, checkin = 0, sitdown = 0, certificated = 0, left = 0, s_left = 0;
long entered = 0, tot = 0;
bool judge_in = false;
long imm_in_hall = 0;

static long ID = 0;
static long _ID = 0;

void atomic_inc(long *ptr) {
  asm volatile(
    "lock incq %0"
    : "+m"(*ptr) : : "memory"
  );
}

void atomic_dec(long *ptr) {
  asm volatile(
    "lock decq %0"
    : "+m"(*ptr) : : "memory"
  );
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

void* Immigrants(void* arg){
    pthread_mutex_lock(&immigrant_lock);
    long id = ID;
    atomic_inc(&ID);
    pthread_mutex_unlock(&immigrant_lock);

    while(1){
        sem_wait(&enter);
        printf("Immigrant #%ld enter\n", id);
        atomic_inc(&imm_in_hall);
        sem_post(&enter);

        // pthread_mutex_lock(&lock);
        // printf("Immigrant #%d enter\n", id);
        if(entered == 0){
            // printf("waiting...\n");
            sem_wait(&to_confirm);
        }
        atomic_inc(&entered);
        confirmed_immigrants[tot] = id;
        atomic_inc(&tot);
        // pthread_mutex_unlock(&lock);

        usleep(random_uint64() % 200);

        printf("Immigrant #%ld checkin\n", id);

        usleep(random_uint64() % 200);

        printf("Immigrant #%ld sitdown\n", id); 
        sem_post(&sit_down);
        printf("Immigrant #%ld swear\n", id);

        // pthread_mutex_lock(&lock);
        atomic_dec(&entered);
        if(entered == 0){
            // printf("can confirm\n");
            sem_post(&to_confirm);
        }
        // pthread_mutex_unlock(&lock);

        usleep(random_uint64() % 200);

        sem_wait(&confirmed);    
        printf("Immigrant #%ld get certificated\n", id);
        // sem_post(&get_certificated);

        usleep(random_uint64() % 200);

        sem_wait(&leave);
        printf("Immigrant #%ld leave\n", id);
        atomic_dec(&imm_in_hall);
        sem_post(&leave);

        id += NT_Imm;
        usleep(random_uint64() % 200);
    }
}

void* Judges(void* arg){
    long id = 0;
    while(1){
        sem_wait(&enter);
        sem_wait(&leave);

        printf("Judge #%ld enter\n", id);
        usleep(random_uint64() % 200);

        // pthread_mutex_lock(&lock);
        sem_wait(&to_confirm);
        // pthread_mutex_unlock(&lock);
        for(int i = 0;i < imm_in_hall;++i){
            sem_wait(&sit_down);
        }

        if(tot == 0)
            printf("judge #%ld has no immigrant to confirm\n", id);
        else{
            printf("Judge #%ld confirm immigrant ", id);
            for(int i = 0;i < tot;++i)
                printf("#%d ", confirmed_immigrants[i]);
            printf("\n");
        }

        int tmp = tot;
        while(tmp--){
            sem_post(&confirmed);
        }

        // while(tot--){
        //     sem_wait(&get_certificated);
        // }
        
        tot = 0;

        usleep(random_uint64() % 200);

        printf("Judge #%ld leave\n", id);
        sem_post(&to_confirm);
        sem_post(&leave);
        sem_post(&enter);

        atomic_inc(&id);

        usleep(random_uint64() % 200);
    }
}

void* Spectators(void* arg){
    pthread_mutex_lock(&spectator_lock);
    long id = _ID;
    atomic_inc(&_ID);
    pthread_mutex_unlock(&spectator_lock);

    while(1){
        sem_wait(&enter);
        printf("Spectator #%ld enter\n", id);
        sem_post(&enter);
        usleep(random_uint64() % 200);
        printf("Spectator #%ld leave\n", id);
        usleep(random_uint64() % 200);

        id += NT_Sp;
    }
}

__attribute__((destructor)) void cleanup() {
    for(int i = 0;i < NT_Imm;++i){
        pthread_join(immigrants[i], NULL);
    }
    for(int i = 0;i < NT_Sp;++i){
        pthread_join(spectators[i], NULL);
    }
    for(int i = 0;i < NT_Jg;++i){
        pthread_join(judges[i], NULL);
    }
    sem_destroy(&enter);
    sem_destroy(&confirmed);
    sem_destroy(&leave);
    sem_destroy(&to_confirm);
    sem_destroy(&get_certificated);
}

int main(int argc, char *argv[]){
    if(argc > 1){
        fprintf(stderr, "Too many arguments\nUsage:./Faneuil\n");
        exit(1);
    }

    setbuf(stdout, NULL);

    sem_init(&enter, 0, 1);
    sem_init(&to_confirm, 0, 1);
    sem_init(&leave, 0, 1);
    sem_init(&get_certificated, 0, 0);
    sem_init(&confirmed, 0, 0);

    pthread_mutex_init(&immigrant_lock, NULL);
    pthread_mutex_init(&judge_lock, NULL);
    pthread_mutex_init(&spectator_lock, NULL);
    pthread_mutex_init(&lock, NULL);

    for(int i = 0;i < NT_Imm;++i){
        pthread_create(&immigrants[i], NULL, Immigrants, NULL);
    }

    for(int i = 0;i < NT_Jg;++i){
        pthread_create(&judges[i], NULL, Judges, NULL);
    }

    for(int i = 0;i < NT_Sp;++i){
        pthread_create(&spectators[i], NULL, Spectators, NULL);
    }


    // printf("switch to main\n");
    // sleep(100);
    return 0;
}