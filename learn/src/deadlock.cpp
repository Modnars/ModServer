#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

/**
 * 说明:
 *     对a和b的修改分别申请互斥锁mutex_a和mutex_b。
 *     本程序执行过程中，两个线程分别修改a和b，其中修改a的线程，先对mutex_a进行锁定，
 * 并修改a，然后对b进行锁定(被阻塞，因为另一进程已获得互斥锁mutex_b)；同时，另一线程
 * 因为相同的原因被阻塞，从而造成两个线程互相等待(均被阻塞)，导致死锁。
 */

int a = 0, b = 0;
pthread_mutex_t mutex_a, mutex_b;

void *another(void *arg) {
    pthread_mutex_lock(&mutex_b);
    printf("In child thread: Got MUTEX_B, Waiting for MUTEX_A.\n");
    sleep(5);
    ++b;
    pthread_mutex_lock(&mutex_a);
    b += a++;
    pthread_mutex_unlock(&mutex_a);
    pthread_mutex_unlock(&mutex_b);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    pthread_t id;
    
    pthread_mutex_init(&mutex_a, NULL);
    pthread_mutex_init(&mutex_b, NULL);
    pthread_create(&id, NULL, another, NULL);

    pthread_mutex_lock(&mutex_a);
    printf("In parent thread: Got MUTEX_A, Waiting for MUTEX_B.\n");
    sleep(5);
    ++a;
    pthread_mutex_lock(&mutex_b);
    a += b++;
    pthread_mutex_unlock(&mutex_b);
    pthread_mutex_unlock(&mutex_a);

    pthread_join(id, NULL);
    pthread_mutex_destroy(&mutex_a);
    pthread_mutex_destroy(&mutex_b);

    return 0;
}
