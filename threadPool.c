// ================================================= Libraries and Headers ============================================
#include "threadPool.h"
// ================================================= Function Implementations =========================================
// The function will create the thread pool by allocating the memory needed
// for all the variables and by creating and starting the threads.
ThreadPool* tpCreate(int numOfThreads) {
    // Allocate memory for the ThreadPool.
    ThreadPool* threadPool = (ThreadPool*) malloc(sizeof(ThreadPool));
    // If allocation failed return NULL.
    if (threadPool == NULL) {
        return NULL;
    }
    // Set the amount of threads we will create, the size of the pool.
    threadPool->size = numOfThreads;
    // Create the osQueue.
    threadPool->osQueue = osCreateQueue();
    // Allocate memory for the array of pthread's.
    threadPool->pthreads = (pthread_t*) malloc(sizeof(pthread_t) * numOfThreads);
    // If allocation failed return NULL.
    if (threadPool->pthreads == NULL) {
        return NULL;
    }
    // Set the boolean integer variables.
    threadPool->available = 1;
    threadPool->stop = 0;
    // Initialize the mutexes.
    // If anything fails, write the error message and call
    // tpDestroy to free the memory and return NULL.
    if (pthread_mutex_init(&(threadPool->mutex), NULL) != 0) {
        tpError();
        tpDestroy(threadPool, 0);
        return NULL;
    }
    if (pthread_mutex_init(&(threadPool->queueMutex), NULL) != 0) {
        tpError();
        tpDestroy(threadPool, 0);
        return NULL;
    }
    if (pthread_mutex_init(&(threadPool->queueMutex), NULL) != 0) {
        tpError();
        tpDestroy(threadPool, 0);
        return NULL;
    }
    // Initialize the condition and handle error.
    if (pthread_cond_init(&(threadPool->condition), NULL) != 0) {
        tpError();
        tpDestroy(threadPool, 0);
        return NULL;
    }
    // Create all the pthread's and start them using the tpRun function.
    int index;
    for (index = 0; index < numOfThreads; ++index) {
        if(pthread_create(&(threadPool->pthreads[index]), NULL, tpRun, (void *) threadPool) != 0) {
            // Handle creation error.
            tpError();
            tpDestroy(threadPool, 0);
            return NULL;
        }
    }
    // Finally, return the fully allocated and initialized ThreadPool
    // if nothing has failed up to this point.
    return threadPool;
}
// ====================================================================================================================
// The function will free all allocated memory for the thread pool.
// It may also wait for all threads to finish based on the input.
void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks) {
    // If the ThreadPool was not allocated properly return immediately.
    if (threadPool == NULL) {
        return;
    }
    // Lock the critical section.
    pthread_mutex_lock(&(threadPool->endMutex));
    // Set the availability to 0 if the destroy function was called
    // since we will not be able to add more tasks for the pthread's.
    if ( threadPool->available == 1) {
        threadPool->available = 0;
        // Since the function can only be called once
        // do nothing if it is called again.
    } else {
        return;
    }
    // Unlock the critical section.
    pthread_mutex_unlock(&(threadPool->endMutex));
    // If we do not need to wait for  the tasks set the stopping
    // condition to 1.
    if (shouldWaitForTasks == 0) {
        threadPool->stop = 1;
    }
    // Index for the 'for' loop.
    int index;
    // Lock the critical section.
    pthread_mutex_lock(&(threadPool->queueMutex));
    // Start running all the threads to finish them before stopping the program
    // using the broadcast command.
    if((pthread_cond_broadcast(&(threadPool->condition)) != 0) ||
       (pthread_mutex_unlock(&(threadPool->queueMutex)) != 0)) {
        tpError();
        tpDestroy(threadPool, 0);
        return;
    }
    // Join all the threads to let them finish their respective tasks.
    for (index = 0; index < threadPool->size; ++index) {
        pthread_join(threadPool->pthreads[index], NULL);
    }
    // Once the threads are done set the stopping condition to true.
    // (This is if we want to wait for them to finish.)
    threadPool->stop = 1;
    // While the queue is not empty deque each data member.
    while (!osIsQueueEmpty(threadPool->osQueue)) {
        // Deque.
        Func* func = osDequeue(threadPool->osQueue);
        // And free each data member separately.
        free(func);
    }
    // Free the OSQueue.
    osDestroyQueue(threadPool->osQueue);
    // Free the pthread array.
    free(threadPool->pthreads);
    // Destroy all the mutexes.
    pthread_mutex_destroy(&(threadPool->mutex));
    pthread_mutex_destroy(&(threadPool->queueMutex));
    pthread_mutex_destroy(&(threadPool->endMutex));
    // Finally, free the ThreadPool struct.
    free(threadPool);
}
// ====================================================================================================================
// The function will insert a new task into the task queue if possible
// and will return 0 upon success or -1 on failure.
int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param) {
    // If the ThreadPool has not been allocated return failure.
    if(threadPool == NULL) {
        return -1;
    }
    // If there is no function to add return failure.
    if (computeFunc == NULL) {
        return -1;
    }
    // If we can't insert a function at this time return failure.
    if (!(threadPool->available)) {
        return -1;
    }
    // Allocate the function structure.
    Func* func = (Func*) malloc(sizeof(Func));
    // If allocation failed return failure.
    if (func == NULL) {
        return -1;
    }
    // Set the parameters of the function structure.
    func->myFunc = computeFunc;
    func->params = param;
    // Add the function to the queue.
    osEnqueue(threadPool->osQueue, (void *) func);
    // Lock the critical section.
    pthread_mutex_lock(&(threadPool->queueMutex));
    // Signal one of the threads to start running the function.
    if(pthread_cond_signal(&(threadPool->condition)) != 0) {
        // Handle error.
        tpError();
        tpDestroy(threadPool, 0);
        return -1;
    }
    // Unlock the critical section.
    pthread_mutex_unlock(&(threadPool->queueMutex));
    // Return 0 means everything worked.
    return 0;
}
// ====================================================================================================================
// Function removes a function from the queue in the form a a Func structure and runs
// it on the current thread using the parameters in the structure.
void* tpRun(void* voidPool) {
    // Convert the ThreadPool from void.
    ThreadPool* threadPool = (ThreadPool*) voidPool;
    // While the program is running, we don't need to stop, we can still
    // insert and there are tasks in the queue.
    while (!threadPool->stop && !(threadPool->available == 0 && osIsQueueEmpty(threadPool->osQueue))) {
        // Lock the critical section.
        pthread_mutex_lock(&(threadPool->queueMutex));
        // If the there are no tasks and we don't need to stop yet
        // we will wait for the condition variable.
        if((osIsQueueEmpty(threadPool->osQueue)) && (!threadPool->stop)) {
            pthread_cond_wait(&(threadPool->condition), &(threadPool->queueMutex));
        }
        // Unlock critical section.
        pthread_mutex_unlock(&(threadPool->queueMutex));
        // Lock critical section.
        pthread_mutex_lock(&(threadPool->mutex));
        // If there are tasks in the queue.
        if (!(osIsQueueEmpty(threadPool->osQueue))) {
            // Remove a task from the queue.
            Func* func = osDequeue(threadPool->osQueue);
            // Unlock the critical section.
            pthread_mutex_unlock(&(threadPool->mutex));
            // Run the task on a thread.
            func->myFunc(func->params);
            // Free the task.
            free(func);
        }
        // Otherwise there are no tasks and we do nothing yet.
        else if (osIsQueueEmpty(threadPool->osQueue)) {
            // Unlock critical section and leave.
            pthread_mutex_unlock(&(threadPool->mutex));
        }
    }
}
// ====================================================================================================================
// The function writes an error message to the global file
// descriptor using the write system call.
void tpError() {
    // The number of the STD_ERROR file descriptor.
    int stdError = 2;
    // Store the error message in a constant string.
    const char* error;
    error = "Error in system call\n";
    // Write it to the descriptor using the write system call.
    write(stdError, error, strlen(error));
}
// ====================================================================================================================
