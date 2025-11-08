#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define NUM_READERS 8
#define NUM_WRITERS 2
#define OPERATIONS 10

int shared_data = 0;
pthread_rwlock_t rwlock;

void* reader(void* arg) {
    long id = (long)arg;
    
    for (int i = 0; i < OPERATIONS; i++) {
        pthread_rwlock_rdlock(&rwlock);
        
        printf("Reader %ld read: %d\n", id, shared_data);
        usleep(100000); // Simulate read operation

        pthread_rwlock_unlock(&rwlock);
        
        usleep(50000);
    }
    
    return NULL;
}

void* writer(void* arg) {
    long id = (long)arg;
    
    for (int i = 0; i < OPERATIONS; i++) {
        pthread_rwlock_wrlock(&rwlock);

        shared_data++;
        printf("Writer %ld wrote: %d\n", id, shared_data);
        usleep(200000); // Simulate write operation

        pthread_rwlock_unlock(&rwlock);
        
        usleep(300000);
    }
    
    return NULL;
}

int main() {
    pthread_t readers[NUM_READERS];
    pthread_t writers[NUM_WRITERS];
    
    pthread_rwlock_init(&rwlock, NULL);
    
    printf("Starting read-write lock demo (readers: %d, writers: %d)\n\n", 
           NUM_READERS, NUM_WRITERS);
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // Create readers
    for (long i = 0; i < NUM_READERS; i++) {
        pthread_create(&readers[i], NULL, reader, (void*)i);
    }
    
    // Create writers
    for (long i = 0; i < NUM_WRITERS; i++) {
        pthread_create(&writers[i], NULL, writer, (void*)i);
    }
    
    // Wait for all threads
    for (int i = 0; i < NUM_READERS; i++) {
        pthread_join(readers[i], NULL);
    }
    for (int i = 0; i < NUM_WRITERS; i++) {
        pthread_join(writers[i], NULL);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    
    printf("\nCompleted in %.2f seconds\n", elapsed);
    printf("Final value: %d\n", shared_data);
    
    pthread_rwlock_destroy(&rwlock);
    
    return 0;
}
