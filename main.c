#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include "osqueue.h"
#include "threadPool.h"

#define THREADS_COUNT 4
#define TASKS_PER_THREAD 30
#define TASKS_PRI_THREAD 10
#define TP_WORKER_THREADS 3

#define DEBUG 0 // Change to 1 for debug info

pthread_mutex_t TasksDoneLock;
pthread_mutex_t TasksInsertedLock;

volatile int tasksDoneCount;
volatile int tasksInsertedCount;

void incTaskAdded() {
    pthread_mutex_lock(&TasksInsertedLock);
    tasksInsertedCount++;
    pthread_mutex_unlock(&TasksInsertedLock);
}

void incTaskDone() {
    pthread_mutex_lock(&TasksDoneLock);
    tasksDoneCount++;
    pthread_mutex_unlock(&TasksDoneLock);
}

int getCurrentThread() {
    return ((long) pthread_self() % 1000);
}


void task1(void *_) {
    int r;
    r = (rand() % 100) + 20;
    if (DEBUG) printf("TASK1 thread %d. sleeping %dms\n",(getCurrentThread()),r);
    usleep(r);
    incTaskDone();
}

void task2(void *_) {
    if (DEBUG) printf("TASK2 thread %d\n",getCurrentThread());
    incTaskDone();
}

void* poolDestoyer (void *arg) {
    ThreadPool* pool = (ThreadPool*) arg;
    tpDestroy(pool,1);
    return NULL;
}

void* tasksAdder(void *arg)
{
    ThreadPool* pool = (ThreadPool*) arg;
    int i;

    for(i=0; i<TASKS_PER_THREAD; ++i) {
        if (!tpInsertTask(pool, task1, NULL)) {
            incTaskAdded();
        }
    }

    return NULL;
}

int shouldWaitForTasksTest(int shouldWait) {
    pthread_t t1[THREADS_COUNT];
    int i,j,result;
    ThreadPool* tp = tpCreate(TP_WORKER_THREADS);
    for (j=0; j<THREADS_COUNT; j++) {
        pthread_create(&t1[j], NULL, tasksAdder, tp);
    }

    for (i=0; i<TASKS_PRI_THREAD ; i++) {
        if (!tpInsertTask(tp, task2, NULL)) {
            incTaskAdded();
        }
    }

    if (DEBUG) printf("-->tp will be destroyed!<--\n");
    tpDestroy(tp,shouldWait);
    if (DEBUG) printf("-->tp destroyed!<--\n");

    if (DEBUG) printf("waiting for other pthreads to end..\n");
    for (j=0; j<THREADS_COUNT; j++) {
        pthread_join(t1[j], NULL);
    }
    pthread_mutex_lock(&TasksInsertedLock);
    pthread_mutex_lock(&TasksDoneLock);
    if (DEBUG) printf("\nSUMMRAY:\nTasks inserted:%d\nTasks done:%d\n",tasksInsertedCount,tasksDoneCount);
    if (DEBUG) printf("Graceful? %d\n",shouldWait);
    if ((shouldWait && tasksInsertedCount == tasksDoneCount) ||
        (!shouldWait && tasksInsertedCount != tasksDoneCount)) {
        result = 0;
    } else {
        result = 1;
    }

    tasksDoneCount = 0;
    tasksInsertedCount = 0;
    pthread_mutex_unlock(&TasksInsertedLock);
    pthread_mutex_unlock(&TasksDoneLock);

    return result;
}

int insertAfterDestroyTest() {
    ThreadPool* tp = tpCreate(TP_WORKER_THREADS);
    int i;
    usleep(50);
    for (i=0; i<TASKS_PRI_THREAD ; i++) {
        tpInsertTask(tp, task1, NULL);
    }
    tpDestroy(tp,1);
    return !tpInsertTask(tp, task1, NULL);
}

int doubleDestroy() {
    pthread_t t1;
    ThreadPool *tp = tpCreate(TP_WORKER_THREADS);
    int i;
    for (i = 0; i < TASKS_PRI_THREAD; i++) {
        tpInsertTask(tp, task1, NULL);
    }
    printf("Going to destroy pool from 2 different pthreads...\n");
    pthread_create(&t1, NULL, poolDestoyer, tp);
    tpDestroy(tp,1);
    pthread_join(t1,NULL);
    printf("Done, did anything break?\n");
    return 0;
}

int main() {
    srand(time(NULL));
    pthread_mutex_init(&TasksDoneLock, NULL);
    pthread_mutex_init(&TasksInsertedLock, NULL);
    tasksDoneCount = 0;
    tasksInsertedCount = 0;
    int i;
    printf("---Tester Running---\n");

    for (i=0; i<10; i++) {
        if (insertAfterDestroyTest())
            printf("Could insert function_info after tp destroyed!\n");
        if (shouldWaitForTasksTest(0))
            printf("Failed on shouldWaitForTasks = 0, tasks created = tasks done. This should rarely happen..\n");
        if (shouldWaitForTasksTest(1))
            printf("Failed on destroy with shouldWaitForTasks = 1. tasks created != tasks done\n");
    }

    doubleDestroy();

    printf("---Tester Done---\n");
    return 0;
}