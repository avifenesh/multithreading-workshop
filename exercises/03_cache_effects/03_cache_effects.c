/**
 * Exercise 3: Cache Effects and False Sharing
 * 
 * Demonstrates false sharing - when threads update separate variables
 * that share the same cache line, causing cache coherency traffic.
 * 
 * Compare packed vs cache-aligned counter arrays.
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdalign.h>
#include "benchmark.h"

#define NUM_THREADS 4
#define ITERATIONS 10000000

// Packed counters - share cache lines (false sharing)
typedef struct {
    atomic_long counter;
} packed_counter_t;

// Cache-aligned counters - each on separate cache line
typedef struct {
    alignas(64) atomic_long counter;
} aligned_counter_t;

packed_counter_t *packed_counters;
aligned_counter_t *aligned_counters;

void *packed_worker(void *arg) {
    long tid = (long)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        atomic_fetch_add_explicit(&packed_counters[tid].counter, 1, memory_order_relaxed);
    }
    return NULL;
}

void *aligned_worker(void *arg) {
    long tid = (long)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        atomic_fetch_add_explicit(&aligned_counters[tid].counter, 1, memory_order_relaxed);
    }
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];
    
    printf("=== Cache Effects: False Sharing Demo ===\n");
    printf("Threads: %d, Iterations per thread: %d\n\n", NUM_THREADS, ITERATIONS);
    
    // Test packed counters (false sharing)
    packed_counters = calloc(NUM_THREADS, sizeof(packed_counter_t));
    printf("Packed counters (false sharing):\n");
    printf("  Counter size: %zu bytes\n", sizeof(packed_counter_t));
    printf("  Array addresses: %p to %p\n", 
           (void*)&packed_counters[0], 
           (void*)&packed_counters[NUM_THREADS-1]);
    
    TIME_BLOCK("Packed (with false sharing)") {
        for (long i = 0; i < NUM_THREADS; i++) {
            pthread_create(&threads[i], NULL, packed_worker, (void*)i);
        }
        for (int i = 0; i < NUM_THREADS; i++) {
            pthread_join(threads[i], NULL);
        }
    }
    
    long packed_total = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        packed_total += atomic_load(&packed_counters[i].counter);
    }
    printf("  Total: %ld\n\n", packed_total);
    
    // Test cache-aligned counters (no false sharing)
    aligned_counters = calloc(NUM_THREADS, sizeof(aligned_counter_t));
    printf("Cache-aligned counters (no false sharing):\n");
    printf("  Counter size: %zu bytes (padded to cache line)\n", sizeof(aligned_counter_t));
    printf("  Array addresses: %p to %p\n",
           (void*)&aligned_counters[0],
           (void*)&aligned_counters[NUM_THREADS-1]);
    
    TIME_BLOCK("Aligned (no false sharing)") {
        for (long i = 0; i < NUM_THREADS; i++) {
            pthread_create(&threads[i], NULL, aligned_worker, (void*)i);
        }
        for (int i = 0; i < NUM_THREADS; i++) {
            pthread_join(threads[i], NULL);
        }
    }
    
    long aligned_total = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        aligned_total += atomic_load(&aligned_counters[i].counter);
    }
    printf("  Total: %ld\n\n", aligned_total);
    
    printf("Expected: %d total increments\n", NUM_THREADS * ITERATIONS);
    printf("\nRun with: make perf-03\n");
    printf("Look for cache-misses and LLC-load-misses\n");
    
    free(packed_counters);
    free(aligned_counters);
    return 0;
}
