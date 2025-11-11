/**
 * Exercise 00: Quick Review - Core Concepts
 * 
 * Fast repetition of the most important multithreading concepts.
 * Run this first thing each day to keep knowledge sharp.
 * 
 * Covers: thread creation, mutex, atomics, memory ordering
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>   // POSIX Threads API
#include <stdatomic.h>
#include <unistd.h>
#include <sched.h>

#define THREADS 4
#define ITERATIONS 100000

/*
 * Terms used in this exercise:
 * - Futex: Fast Userspace muTEX (Linux). User-mode fast path; kernel parks threads on contention.
 * - CAS:   Compare-And-Swap (compare-exchange). Atomic primitive used to implement locks/lock-free ops.
 */

// ============================================================================
// Part 1: Thread Creation Basics
// ============================================================================

void *simple_thread(void *arg) {
    long tid = (long)arg;
    printf("Thread %ld running on CPU %d\n", tid, sched_getcpu());
    return (void*)(tid * 2);
}

void demo_thread_creation() {
    printf("\n=== Thread Creation ===\n");
    pthread_t threads[THREADS];
    
    for (long i = 0; i < THREADS; i++) {
        pthread_create(&threads[i], NULL, simple_thread, (void*)i);
    }
    
    for (int i = 0; i < THREADS; i++) {
        void *result;
        pthread_join(threads[i], &result);
        printf("Thread %d returned: %ld\n", i, (long)result);
    }
    
    printf("Key: pthread_create() -> clone(CLONE_VM|CLONE_THREAD)\n");
    printf("     Shared: address space, file descriptors, signal handlers\n");
    printf("     Unique: stack, TLS, thread ID\n");
}

// ============================================================================
// Part 2: Race Condition Example
// ============================================================================

int shared_counter = 0;

void *race_increment(void *arg) {
    (void)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        shared_counter++;  // RACE! Not atomic
    }
    return NULL;
}

void demo_race() {
    printf("\n=== Race Condition (BROKEN) ===\n");
    shared_counter = 0;
    pthread_t threads[THREADS];
    
    for (long i = 0; i < THREADS; i++) {
        pthread_create(&threads[i], NULL, race_increment, NULL);
    }
    for (int i = 0; i < THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("Expected: %d, Got: %d ", THREADS * ITERATIONS, shared_counter);
    if (shared_counter != THREADS * ITERATIONS) {
        printf("← RACE!\n");
    } else {
        printf("(got lucky)\n");
    }
    printf("Assembly: movl (%%rax), %%edx; incl %%edx; movl %%edx, (%%rax)\n");
    printf("          ^^^ Not atomic, can interleave\n");
}

// ============================================================================
// Part 3: Mutex Solution
// ============================================================================

int mutex_counter = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void *mutex_increment(void *arg) {
    (void)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        pthread_mutex_lock(&mutex);
        mutex_counter++;
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

void demo_mutex() {
    printf("\n=== Mutex (Correct) ===\n");
    mutex_counter = 0;
    pthread_t threads[THREADS];
    
    for (long i = 0; i < THREADS; i++) {
        pthread_create(&threads[i], NULL, mutex_increment, NULL);
    }
    for (int i = 0; i < THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("Expected: %d, Got: %d ✓\n", THREADS * ITERATIONS, mutex_counter);
    printf("How it works:\n");
    printf("  Fast path: atomic CAS on futex word (~10ns)\n");
    printf("  Slow path: futex(FUTEX_WAIT) syscall, sleep (~1µs)\n");
    printf("  Unlock: atomic store, futex(FUTEX_WAKE) if waiters\n");
}

// ============================================================================
// Part 4: Atomic Solution
// ============================================================================

atomic_int atomic_counter = 0;

void *atomic_increment(void *arg) {
    (void)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        atomic_fetch_add(&atomic_counter, 1);
    }
    return NULL;
}

void demo_atomic() {
    printf("\n=== Atomics (Correct + Fast) ===\n");
    atomic_store(&atomic_counter, 0);
    pthread_t threads[THREADS];
    
    for (long i = 0; i < THREADS; i++) {
        pthread_create(&threads[i], NULL, atomic_increment, NULL);
    }
    for (int i = 0; i < THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("Expected: %d, Got: %d ✓\n", THREADS * ITERATIONS, atomic_load(&atomic_counter));
    printf("Assembly: lock incl (%%rax)  \u2190 LOCK prefix ensures atomicity\n");
    printf("No syscall, but LOCK causes cache coherency traffic\n");
}

// ============================================================================
// Part 5: Memory Ordering
// ============================================================================

atomic_int data = 0;
atomic_int ready = 0;

void *producer(void *arg) {
    (void)arg;
    atomic_store_explicit(&data, 42, memory_order_relaxed);
    atomic_store_explicit(&ready, 1, memory_order_release);  // Barrier
    return NULL;
}

void *consumer(void *arg) {
    (void)arg;
    while (!atomic_load_explicit(&ready, memory_order_acquire)) {  // Barrier
        // Wait
    }
    int value = atomic_load_explicit(&data, memory_order_relaxed);
    printf("Consumer saw data=%d\n", value);
    return NULL;
}

void demo_memory_ordering() {
    printf("\n=== Memory Ordering (Acquire-Release) ===\n");
    atomic_store(&data, 0);
    atomic_store(&ready, 0);
    
    pthread_t cons, prod;
    pthread_create(&cons, NULL, consumer, NULL);
    usleep(1000);  // Let consumer start waiting
    pthread_create(&prod, NULL, producer, NULL);
    
    pthread_join(prod, NULL);
    pthread_join(cons, NULL);
    
    printf("Release barrier: prevents earlier ops from moving after\n");
    printf("Acquire barrier: prevents later ops from moving before\n");
    printf("Happens-before: data=42 guaranteed visible after ready=1\n");
}

// ============================================================================
// Part 6: Quick Reference
// ============================================================================

void print_reference() {
    printf("\n=== Quick Reference ===\n");
    printf("Mutex:        Fast path ~10ns, slow path ~1µs (futex syscall)\n");
    printf("Atomic:       ~10-50ns (LOCK prefix, cache coherency)\n");
    printf("Context sw:   ~1-3µs (save/restore regs, TLB flush)\n");
    printf("Cache line:   64 bytes (false sharing threshold)\n");
    printf("L1 hit:       ~4 cycles (~1ns)\n");
    printf("L3 hit:       ~40 cycles (~10ns)\n");
    printf("RAM:          ~200 cycles (~60ns)\n");
    printf("\nMemory orders:\n");
    printf("  relaxed:    No sync, just atomicity\n");
    printf("  acquire:    Load barrier (prevents reorder forward)\n");
    printf("  release:    Store barrier (prevents reorder backward)\n");
    printf("  seq_cst:    Total order (slowest, default)\n");
    printf("\nTools:\n");
    printf("  make asm-XX      View assembly\n");
    printf("  make tsan-XX     Detect races (ThreadSanitizer)\n");
    printf("  make perf-XX     Cache misses, context switches\n");
    printf("  objdump -d       Disassemble binary\n");
}

// ============================================================================
// Main
// ============================================================================

int main() {
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║   Exercise 00: Quick Review - Core Multithreading       ║\n");
    printf("║   Run this daily for fast knowledge reinforcement       ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    
    demo_thread_creation();
    demo_race();
    demo_mutex();
    demo_atomic();
    demo_memory_ordering();
    print_reference();
    
    printf("\n✓ Review complete. Now dive into exercises 01-06!\n\n");
    return 0;
}
