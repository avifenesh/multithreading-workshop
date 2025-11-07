#include <stdio.h>
#include <pthread.h>

#define NUM_THREADS 10
#define INCREMENTS_PER_THREAD 1000

int counter = 0;
pthread_mutex_t mutex;

void* increment_counter(void* arg) {
    for (int i = 0; i < INCREMENTS_PER_THREAD; i++) {
        // TODO: Add mutex lock here
        
        counter++;
        
        // TODO: Add mutex unlock here
    }
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];
    
    // TODO: Initialize the mutex
    // pthread_mutex_init(&mutex, NULL);
    
    // Create threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, increment_counter, NULL);
    }
    
    // Wait for all threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("Final counter value: %d\n", counter);
    
    int expected = NUM_THREADS * INCREMENTS_PER_THREAD;
    if (counter == expected) {
        printf("Success! Counter is correct.\n");
    } else {
        printf("Error! Expected %d but got %d\n", expected, counter);
    }
    
    // TODO: Destroy the mutex
    // pthread_mutex_destroy(&mutex);
    
    return 0;
}
