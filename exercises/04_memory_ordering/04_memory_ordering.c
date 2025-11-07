/**
 * Exercise 4: Memory Ordering and Reordering
 * 
 * Demonstrates compiler and CPU reordering issues, and how
 * memory ordering constraints prevent them.
 * 
 * Shows broken code, seq_cst fix, and acquire-release patterns.
 */

#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>
#include <assert.h>

// Shared variables
atomic_int data = 0;
atomic_int flag = 0;

// Version 1: BROKEN - no synchronization
int data_broken = 0;
int flag_broken = 0;

void *producer_broken(void *arg) {
    data_broken = 42;  // Can be reordered!
    flag_broken = 1;   // Can be reordered!
    return NULL;
}

void *consumer_broken(void *arg) {
    while (flag_broken == 0) {
        // Busy wait - might never see flag=1 due to caching
    }
    // Might see data=0 even after flag=1 due to reordering
    printf("Broken version - data=%d (may be 0 or 42)\n", data_broken);
    return NULL;
}

// Version 2: seq_cst - strongest ordering
void *producer_seqcst(void *arg) {
    atomic_store_explicit(&data, 42, memory_order_seq_cst);
    atomic_store_explicit(&flag, 1, memory_order_seq_cst);
    return NULL;
}

void *consumer_seqcst(void *arg) {
    while (atomic_load_explicit(&flag, memory_order_seq_cst) == 0) {
        // Busy wait with seq_cst ordering
    }
    int value = atomic_load_explicit(&data, memory_order_seq_cst);
    printf("Seq_cst version - data=%d (always 42)\n", value);
    assert(value == 42);
    return NULL;
}

// Version 3: acquire-release - efficient synchronization
void *producer_acqrel(void *arg) {
    atomic_store_explicit(&data, 42, memory_order_relaxed);
    atomic_store_explicit(&flag, 1, memory_order_release); // Release barrier
    return NULL;
}

void *consumer_acqrel(void *arg) {
    while (atomic_load_explicit(&flag, memory_order_acquire) == 0) {
        // Acquire barrier on successful read
    }
    int value = atomic_load_explicit(&data, memory_order_relaxed);
    printf("Acquire-release version - data=%d (always 42)\n", value);
    assert(value == 42);
    return NULL;
}

void test_version(const char *name, void *(*producer)(void*), void *(*consumer)(void*)) {
    printf("\n=== %s ===\n", name);
    
    // Reset state
    data_broken = 0;
    flag_broken = 0;
    atomic_store(&data, 0);
    atomic_store(&flag, 0);
    
    pthread_t prod, cons;
    pthread_create(&cons, NULL, consumer, NULL);
    pthread_create(&prod, NULL, producer, NULL);
    
    pthread_join(prod, NULL);
    pthread_join(cons, NULL);
}

int main() {
    printf("=== Memory Ordering Demo ===\n");
    printf("This demonstrates why memory ordering matters.\n");
    
    // WARNING: Broken version may appear to work due to timing
    printf("\n--- Testing BROKEN version (no synchronization) ---");
    for (int i = 0; i < 5; i++) {
        test_version("Broken (no atomics)", producer_broken, consumer_broken);
    }
    
    printf("\n--- Testing seq_cst version ---");
    test_version("Sequential consistency", producer_seqcst, consumer_seqcst);
    
    printf("\n--- Testing acquire-release version ---");
    test_version("Acquire-release", producer_acqrel, consumer_acqrel);
    
    printf("\nKey insights:\n");
    printf("1. Without atomics, compiler/CPU can reorder operations\n");
    printf("2. seq_cst provides total order but is expensive\n");
    printf("3. acquire-release provides necessary synchronization efficiently\n");
    printf("\nUse 'make asm-04' to see actual assembly differences\n");
    
    return 0;
}
