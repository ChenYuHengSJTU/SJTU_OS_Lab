#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <sched.h>
#include <unistd.h>
#include <stdint.h>

sem_t seed, shovel, fill, dig;
long hole_num, dug, planted, filled;

#define NT 9

pthread_t threads[NT];

void atomic_inc(long *ptr) {
  asm volatile(
    "lock incq %0"
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

void* Larry(void* arg){
    while(1){
        sem_wait(&dig);
        sem_wait(&shovel);
        atomic_inc(&dug);
        printf("Larry digs another hole #%ld\n", dug);
        fflush(stdout);
        sem_post(&shovel);
        sem_post(&seed);
        // sched_yield();
        usleep(random_uint64() % 200);
    }
    pthread_exit(NULL);
}

void* Curly(void* arg){
    while(1){
        sem_wait(&fill);
        sem_wait(&shovel);
        atomic_inc(&planted);
        printf("Curly fills another hole#%ld\n", planted);
        fflush(stdout);
        sem_post(&shovel);
        sem_post(&dig);
        // sched_yield();
        usleep(random_uint64() % 200);
    }
    pthread_exit(NULL);
}

void* Moe(void* arg){
    while(1){
        sem_wait(&seed);
        atomic_inc(&filled);
        printf("Moe plants a seed in hole#%ld\n", filled);
        fflush(stdout);
        sem_post(&fill);
        // sched_yield();
        usleep(random_uint64() % 200);
    }
    pthread_exit(NULL);
}

__attribute__((destructor)) void cleanup() {
    for(int i = 0;i < NT;++i){
        pthread_join(threads[i], NULL);
    }
    sem_destroy(&seed);
    sem_destroy(&fill);
    sem_destroy(&shovel);
    sem_destroy(&dig);
}

int main(int argc, char *argv[]){
    if(argc <= 1){
        fprintf(stderr, "Too few arguments\nUsage: ./LCM n\n");
        exit(1);
    }
    if(argc > 2){
        fprintf(stderr, "Too many arguments\nUsage: ./LCM n\n");
        exit(1);
    }

    hole_num = atoi(argv[1]);

    sem_init(&seed, 0, 0);
    sem_init(&shovel, 0, 1);
    sem_init(&fill, 0, 0);
    sem_init(&dig, 0, hole_num);

    hole_num = 0;

    pthread_create(&threads[0], NULL, Larry, NULL);
    pthread_create(&threads[1], NULL, Moe, NULL);
    pthread_create(&threads[2], NULL, Curly, NULL);
    pthread_create(&threads[3], NULL, Larry, NULL);
    pthread_create(&threads[4], NULL, Moe, NULL);
    pthread_create(&threads[5], NULL, Curly, NULL);
    pthread_create(&threads[6], NULL, Larry, NULL);
    pthread_create(&threads[7], NULL, Moe, NULL);
    pthread_create(&threads[8], NULL, Curly, NULL);

    for(int i = 0;i < NT;++i)
        pthread_join(threads[i], NULL);

    // sleep(100);

    sem_destroy(&fill);
    sem_destroy(&dig);
    sem_destroy(&shovel);
    sem_destroy(&seed);

    return 0;
}