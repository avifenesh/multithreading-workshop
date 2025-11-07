#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>

#define NUM_THREADS 10
#define INCREMENTS 1000000

atomic_int atomic_counter = 0;
int regular_counter = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void* atomic_increment(void* arg) {
    for (int i = 0; i < INCREMENTS; i++) {
        atomic_fetch_add(&atomic_counter, 1);
    }
    return NULL;
}

void* mutex_increment(void* arg) {
    for (int i = 0; i < INCREMENTS; i++) {
        pthread_mutex_lock(&mutex);
        regular_counter++;
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

double measure_time(void* (*func)(void*)) {
    pthread_t threads[NUM_THREADS];
    struct timespec start, end;
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (long i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, func, NULL);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
}

int main() {
    printf("Comparing atomic vs mutex performance...\n");
    printf("Threads: %d, Increments per thread: %d\n\n", NUM_THREADS, INCREMENTS);
    
    // Test mutex version
    regular_counter = 0;
    double mutex_time = measure_time(mutex_increment);
    printf("Mutex version:  %.3f seconds (counter = %d)\n", mutex_time, regular_counter);
    
    // Test atomic version
    atomic_store(&atomic_counter, 0);
    double atomic_time = measure_time(atomic_increment);
    int final_atomic = atomic_load(&atomic_counter);
    printf("Atomic version: %.3f seconds (counter = %d)\n", atomic_time, final_atomic);
    
    printf("\nSpeedup: %.2fx faster with atomics\n", mutex_time / atomic_time);
    
    return 0;
}
