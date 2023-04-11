#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <sched.h>
#include <unistd.h>

sem_t enter, confirm, leave;
pthread_mutex_t immigrant_lock, judge_lock, spectator_lock;

#define NT_Imm 5
#define NT_Jg 1
#define NT_Sp 2

#define NT NT_Imm + NT_Jg + NT_Sp

pthread_t immigrants[NT_Imm];
pthread_t judges[NT_Jg];
pthread_t spectators[NT_Sp];

int immigrant = 0, judge = 0, spectator = 0, enter_in = 0, to_confirm = 0, confirmed = 0, checkin = 0, sitdown = 0, certificated = 0, left = 0, s_left = 0;

void atomic_inc(int *ptr) {
  asm volatile(
    "lock incq %0"
    : "+m"(*ptr) : : "memory"
  );
}

void atomic_dec(int *ptr) {
  asm volatile(
    "lock decq %0"
    : "+m"(*ptr) : : "memory"
  );
}

void* Immigrants(void* arg){
    while(1){
        sem_wait(&enter);
        sem_post(&enter);
        pthread_mutex_lock(&immigrant_lock);
        printf("Immigrant #%d enter\n", immigrant);
        // immigrant++;
        atomic_inc(&immigrant);
        // pthread_mutex_unlock(&immigrant_lock);
        if(enter_in == 0)
            sem_wait(&confirm);
        // enter_in++;
        atomic_inc(&enter_in);
        pthread_mutex_unlock(&immigrant_lock);
        usleep(100);
        pthread_mutex_lock(&immigrant_lock);
        printf("Immigrant #%d checkin\n", checkin);
        // checkin++;
        atomic_inc(&checkin);
        pthread_mutex_unlock(&immigrant_lock);
        usleep(100);
        pthread_mutex_lock(&immigrant_lock);
        // to_confirm++;
        atomic_inc(&to_confirm);
        printf("Immigrant #%d sitdown\n", sitdown); 
        printf("Immigrant #%d swear\n", sitdown);
        // sitdown++;
        atomic_inc(&sitdown);
        // enter_in--;
        atomic_dec(&enter_in);
        if(enter_in == 0)
            sem_post(&confirm);
        pthread_mutex_unlock(&immigrant_lock);
        
        usleep(100);

        pthread_mutex_lock(&immigrant_lock);
        if(confirmed){
            printf("Immigrant #%d get certificated\n", certificated);
            // confirmed--;
            atomic_inc(&certificated);
            atomic_dec(&confirmed);
        }
        pthread_mutex_unlock(&immigrant_lock);
        usleep(100);
        pthread_mutex_lock(&immigrant_lock);
        sem_wait(&leave);
        printf("Immigrant #%d leave\n", leave);
        sem_post(&leave);
        // left++;
        atomic_inc(&left);
        pthread_mutex_unlock(&immigrant_lock);
    }
}

void* Judges(void* arg){
    while(1){
        sem_wait(&enter);
        sem_wait(&leave);

        printf("Judge #%d enter\n", judge);
        usleep(100);

        sem_wait(&confirm);

        confirmed = to_confirm;

        printf("Judge #%d confirm immigrant ", judge);


        pthread_mutex_lock(&judge_lock);
        for(int i = 0;i < to_confirm;++i)
            printf("#%d ", i + immigrant - to_confirm);
        
        printf("\n");
        pthread_mutex_unlock(&judge_lock);
        usleep(100);
        sem_post(&confirm);
        sem_post(&leave);
        printf("Judge #%d leave\n", judge);
        sem_post(&enter);
        atomic_inc(&judge);
        usleep(100);
    }
}

void* Spectators(void* arg){
    while(1){
        sem_wait(&enter);
        printf("Spectators #%d enter\n", spectator);
        spectator++;
        sem_post(&enter);
        usleep(200);
        printf("Spectators #%d leave\n", s_left);
        atomic_inc(&s_left);
        usleep(100);
    }
}

__attribute__((destructor)) void cleanup() {
    // for(int i = 0;i < NT;++i){
    //     pthread_join(&threads[i], NULL);
    // }
    sem_destroy(&enter);
    sem_destroy(&confirm);
    sem_destroy(&leave);
}

int main(int argc, char *argv[]){
    sem_init(&enter, 0 , 1);
    sem_init(&confirm, 0, 1);
    sem_init(&leave, 0, 1);

    pthread_mutex_init(&immigrant_lock, NULL);
    pthread_mutex_init(&judge_lock, NULL);
    pthread_mutex_init(&spectator_lock, NULL);

    for(int i = 0;i < NT_Imm;++i){
        pthread_create(&immigrants[i], NULL, Immigrants, NULL);
    }

    for(int i = 0;i < NT_Jg;++i){
        pthread_create(&judges[i], NULL, Judges, NULL);
    }

    for(int i = 0;i < NT_Sp;++i){
        pthread_create(&spectators[i], NULL, Spectators, NULL);
    }


    sleep(100);
    return 0;
}