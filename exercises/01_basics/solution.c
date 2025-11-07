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
    
    // Create threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, thread_function, (void*)(long)i);
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("All threads completed!\n");
    return 0;
}
