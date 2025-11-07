/**
 * Exercise 5: Spinlock Internals
 * 
 * Implement spinlocks from scratch and understand their internals:
 * - Test-and-set (TAS) spinlock
 * - Test-and-test-and-set (TTAS) spinlock  
 * - Compare with pthread_spinlock_t
 * 
 * Learn why TTAS is better for contention.
 */

#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include "benchmark.h"

#define NUM_THREADS 4
#define ITERATIONS 100000

// Simple TAS spinlock - always does atomic exchange
typedef struct {
    atomic_flag lock;
} tas_spinlock_t;

void tas_lock(tas_spinlock_t *lock) {
    while (atomic_flag_test_and_set_explicit(&lock->lock, memory_order_acquire)) {
        // Busy wait - keeps doing expensive atomic operations
    }
}

void tas_unlock(tas_spinlock_t *lock) {
    atomic_flag_clear_explicit(&lock->lock, memory_order_release);
}

// TTAS spinlock - test locally first
typedef struct {
    atomic_bool locked;
} ttas_spinlock_t;

void ttas_lock(ttas_spinlock_t *lock) {
    while (1) {
        // First, check locally (cheap read, can be cached)
        if (!atomic_load_explicit(&lock->locked, memory_order_relaxed)) {
            // Then try to acquire (expensive atomic exchange)
            bool expected = false;
            if (atomic_compare_exchange_weak_explicit(
                    &lock->locked, &expected, true,
                    memory_order_acquire, memory_order_relaxed)) {
                break;  // Got the lock
            }
        }
        // Optionally add CPU_PAUSE() here for better performance
    }
}

void ttas_unlock(ttas_spinlock_t *lock) {
    atomic_store_explicit(&lock->locked, false, memory_order_release);
}

// Shared counter
long shared_counter = 0;

// Test functions
void *tas_worker(void *arg) {
    tas_spinlock_t *lock = (tas_spinlock_t *)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        tas_lock(lock);
        shared_counter++;
        tas_unlock(lock);
    }
    return NULL;
}

void *ttas_worker(void *arg) {
    ttas_spinlock_t *lock = (ttas_spinlock_t *)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        ttas_lock(lock);
        shared_counter++;
        ttas_unlock(lock);
    }
    return NULL;
}

void *pthread_spin_worker(void *arg) {
    pthread_spinlock_t *lock = (pthread_spinlock_t *)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        pthread_spin_lock(lock);
        shared_counter++;
        pthread_spin_unlock(lock);
    }
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];
    
    printf("=== Spinlock Internals ===\n");
    printf("Threads: %d, Iterations: %d\n\n", NUM_THREADS, ITERATIONS);
    
    // Test TAS spinlock
    tas_spinlock_t tas_lock = { ATOMIC_FLAG_INIT };
    shared_counter = 0;
    
    TIME_BLOCK("TAS spinlock") {
        for (long i = 0; i < NUM_THREADS; i++) {
            pthread_create(&threads[i], NULL, tas_worker, &tas_lock);
        }
        for (int i = 0; i < NUM_THREADS; i++) {
            pthread_join(threads[i], NULL);
        }
    }
    printf("  Counter: %ld (expected %d)\n\n", shared_counter, NUM_THREADS * ITERATIONS);
    
    // Test TTAS spinlock
    ttas_spinlock_t ttas_lock = { false };
    shared_counter = 0;
    
    TIME_BLOCK("TTAS spinlock") {
        for (long i = 0; i < NUM_THREADS; i++) {
            pthread_create(&threads[i], NULL, ttas_worker, &ttas_lock);
        }
        for (int i = 0; i < NUM_THREADS; i++) {
            pthread_join(threads[i], NULL);
        }
    }
    printf("  Counter: %ld (expected %d)\n\n", shared_counter, NUM_THREADS * ITERATIONS);
    
    // Test pthread spinlock
    pthread_spinlock_t pthread_lock;
    pthread_spin_init(&pthread_lock, PTHREAD_PROCESS_PRIVATE);
    shared_counter = 0;
    
    TIME_BLOCK("pthread_spinlock_t") {
        for (long i = 0; i < NUM_THREADS; i++) {
            pthread_create(&threads[i], NULL, pthread_spin_worker, &pthread_lock);
        }
        for (int i = 0; i < NUM_THREADS; i++) {
            pthread_join(threads[i], NULL);
        }
    }
    printf("  Counter: %ld (expected %d)\n\n", shared_counter, NUM_THREADS * ITERATIONS);
    
    pthread_spin_destroy(&pthread_lock);
    
    printf("Key insights:\n");
    printf("1. TAS does atomic exchange on every iteration (expensive)\n");
    printf("2. TTAS reads locally first, only atomic on potential acquire\n");
    printf("3. TTAS reduces cache coherency traffic under contention\n");
    printf("\nUse 'make asm-05' to see the actual implementation\n");
    
    return 0;
}
