// ====================================================================================================================
#ifndef __THREAD_POOL__
#define __THREAD_POOL__
// ================================================= Libraries and Headers ============================================
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include "osqueue.h"
// ================================================= Function Structure ===============================================
// The Func structure will be used to save information about the task function
// and will be stored as data on the osQueue to be used by the threads.
typedef struct function_info {
    // The function pointer.
    void (*myFunc)(void *params);
    // The function parameters.
    void* params;
} Func;
// ================================================= ThreadPool Structure =============================================
// The ThreadPool structure will contain all the information needed to be able to create
// a functional thread pool, such as:
typedef struct thread_pool {
    // The amount of pthread's in the pool.
    int size;
    // An array of pthread's the tasks will be run on.
    pthread_t* pthreads;
    // The queue the function struct will be stored on as data.
    OSQueue* osQueue;
    // The mutexes that will be used to lock critical sections.
    pthread_mutex_t mutex;
    pthread_mutex_t queueMutex;
    pthread_mutex_t endMutex;
    // The condition variable we will use to lock threads.
    pthread_cond_t condition;
    // Boolean integers that will be used to determine:
    // Should we stop the program.
    int stop;
    // Can we insert a new task into the pool,
    int available;
} ThreadPool;
// ================================================= Function Declarations ============================================
// The function will create the thread pool by allocating the memory needed
// for all the variables and by creating and starting the threads.
ThreadPool* tpCreate(int numOfThreads);
// ====================================================================================================================
// The function will free all allocated memory for the thread pool.
// It may also wait for all threads to finish based on the input.
void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks);
// ====================================================================================================================
// The function will insert a new task into the task queue if possible
// and will return 0 upon success or -1 on failure.
int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param);
// ====================================================================================================================
// Function removes a function from the queue in the form a a Func structure and runs
// it on the current thread using the parameters in the structure.
void* tpRun(void* voidPool);
// ====================================================================================================================
// The function writes an error message to the global file
// descriptor using the write system call.
void tpError();
// ====================================================================================================================
#endif
