#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

sem_t sem1, sem2;
pthread_t threads[2];

void* f1(void* arg){
    // printf("enter 1");
    while(1){
        sem_wait(&sem1);
        printf("1");
        // sleep(1);
        sem_post(&sem2);
    }
}

void* f2(void* arg){
    // printf("enter f2\n");
    while(1){
        sem_wait(&sem2);
        printf("2");
        // sleep(1);
        sem_post(&sem1);
    }
}

__attribute__((destructor)) void cleanup() {
//   join();
    // pthread_join(threads[0], NULL);
    // pthread_join(threads[1], NULL);
    // sem_destroy(&sem1);
    // sem_destroy(&sem2);
}

int main(int argc, char *argv[]){

    sem_init(&sem1, 0, 0);
    sem_init(&sem2, 0, 1);

    // for(int i = 0;i < 1;++i){

    pthread_create(&threads[0], NULL, f2, NULL);
    // }
    pthread_create(&threads[1], NULL, f1, NULL);


    for(int i = 0;i < 2;++i)
        pthread_join(threads[i], NULL);



    // sleep(100);

    return 0;
}