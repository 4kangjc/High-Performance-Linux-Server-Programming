#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <wait.h>

pthread_mutex_t mutex;

void* another(void* arg) {
    printf("in child thread, lock the mutex\n");
    pthread_mutex_lock(&mutex);
    sleep(5);
    pthread_mutex_unlock(&mutex);
}

void prepare() {
    pthread_mutex_lock(&mutex);
}

void infork() {
    pthread_mutex_unlock(&mutex);
}

int main() {
    pthread_mutex_init(&mutex, NULL);
    pthread_t id;
    pthread_create(&id, NULL, another, NULL);
    pthread_atfork(prepare, infork, infork);
    sleep(1);
    int pid = fork();
    if (pid < 0) {
        pthread_join(id, NULL);
        pthread_mutex_destroy(&mutex);
        return 1;
    } else if (pid == 0) {
        printf("I am in the child, want to get the lock\n");
        /* 子进程从父进程继承了互斥锁mutex 的状态，该互斥锁处于锁住的状态，这是由父进程中子线程执行pthread_mutex_lock引起的，因此，下面这句加锁操作会一直阻塞 
        尽管从逻辑上来说它是不应该阻塞的 */
        pthread_mutex_lock(&mutex);
        printf("I can not run to hear, oop...\n");
        pthread_mutex_unlock(&mutex);
        exit(0);
    } else {
        wait(NULL);
    }
    pthread_join(id, NULL);
    pthread_mutex_destroy(&mutex);
    return 0;
}