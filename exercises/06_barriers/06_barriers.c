/**
 * Exercise 6: Barrier Synchronization
 * 
 * Implement a reusable barrier from scratch using:
 * - Mutex for mutual exclusion
 * - Condition variable for waiting
 * - Serial number (epoch) to handle reuse
 * 
 * Demonstrates phase synchronization patterns.
 */

#include <stdio.h>
#include <pthread.h>   // POSIX Threads API
#include <stdbool.h>
#include "benchmark.h"

#define NUM_THREADS 4
#define NUM_PHASES 3

// Manual barrier implementation
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int count;          // Threads at barrier
    int threshold;      // Total threads
    int serial;         // Generation number (aka epoch)
} barrier_t;

void barrier_init(barrier_t *barrier, int threshold) {
    pthread_mutex_init(&barrier->mutex, NULL);
    pthread_cond_init(&barrier->cond, NULL);
    barrier->count = 0;
    barrier->threshold = threshold;
    barrier->serial = 0;
}

void barrier_destroy(barrier_t *barrier) {
    pthread_mutex_destroy(&barrier->mutex);
    pthread_cond_destroy(&barrier->cond);
}

void barrier_wait(barrier_t *barrier) {
    pthread_mutex_lock(&barrier->mutex);
    
    int my_serial = barrier->serial;  // Remember my epoch
    barrier->count++;
    
    if (barrier->count == barrier->threshold) {
        // Last thread - wake everyone and advance epoch
        barrier->count = 0;
        barrier->serial++;
        pthread_cond_broadcast(&barrier->cond);
    } else {
        // Wait for my epoch to complete
        while (my_serial == barrier->serial) {
            pthread_cond_wait(&barrier->cond, &barrier->mutex);
        }
    }
    
    pthread_mutex_unlock(&barrier->mutex);
}

// Thread argument
typedef struct {
    int id;
    barrier_t *barrier;
} thread_arg_t;

void *worker(void *arg) {
    thread_arg_t *targ = (thread_arg_t *)arg;
    
    for (int phase = 0; phase < NUM_PHASES; phase++) {
        // Do some "work"
        printf("Thread %d: starting phase %d\n", targ->id, phase);
        usleep(100000 * (targ->id + 1));  // Simulate different work times
        
        // Synchronize at barrier
        printf("Thread %d: waiting at barrier (phase %d)\n", targ->id, phase);
        barrier_wait(targ->barrier);
        
        // All threads proceed together
        printf("Thread %d: passed barrier (phase %d)\n", targ->id, phase);
    }
    
    return NULL;
}

int main() {
    printf("=== Barrier Synchronization ===\n");
    printf("Threads: %d, Phases: %d\n\n", NUM_THREADS, NUM_PHASES);
    
    barrier_t barrier;
    barrier_init(&barrier, NUM_THREADS);
    
    pthread_t threads[NUM_THREADS];
    thread_arg_t args[NUM_THREADS];
    
    TIME_BLOCK("Multi-phase execution with barriers") {
        for (int i = 0; i < NUM_THREADS; i++) {
            args[i].id = i;
            args[i].barrier = &barrier;
            pthread_create(&threads[i], NULL, worker, &args[i]);
        }
        
        for (int i = 0; i < NUM_THREADS; i++) {
            pthread_join(threads[i], NULL);
        }
    }
    
    barrier_destroy(&barrier);
    
    printf("\nKey insights:\n");
    printf("1. Barriers synchronize threads at phase boundaries\n");
    printf("2. Serial number (epoch) allows barrier reuse\n");
    printf("3. Last thread wakes all others with broadcast\n");
    printf("4. pthread_barrier_t uses similar implementation\n");
    
    return 0;
}
