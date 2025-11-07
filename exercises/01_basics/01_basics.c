#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#define NUM_THREADS 5

void* thread_function(void* arg) {
    long thread_id = (long)arg;
    printf("Thread %ld is running\n", thread_id);
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];
    
    // TODO: Create NUM_THREADS threads
    // Hint: Use pthread_create() in a loop
    // Pass the loop index as the thread argument
    for (int i = 0; i < NUM_THREADS; i++) {
        // TODO: Fill this in
        // pthread_create(&threads[i], NULL, thread_function, ???);
    }
    
    // TODO: Wait for all threads to complete
    // Hint: Use pthread_join() in a loop
    for (int i = 0; i < NUM_THREADS; i++) {
        // TODO: Fill this in
        // pthread_join(???, NULL);
    }
    
    printf("All threads completed!\n");
    return 0;
}
