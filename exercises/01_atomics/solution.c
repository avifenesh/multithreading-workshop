/**
 * Exercise 01: Solution - Atomic Operations and Memory Ordering
 */

#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
#include <unistd.h>

#define NUM_THREADS 8
#define INCREMENTS 5000000

long broken_counter = 0;
atomic_long seqcst_counter = 0;
atomic_long relaxed_counter = 0;

void* broken_increment(void* arg) {
    for (int i = 0; i < INCREMENTS; i++) {
        broken_counter++;
    }
    return NULL;
}

void* seqcst_increment(void* arg) {
    for (int i = 0; i < INCREMENTS; i++) {
        atomic_fetch_add_explicit(&seqcst_counter, 1, memory_order_seq_cst);
    }
    return NULL;
}

void* relaxed_increment(void* arg) {
    for (int i = 0; i < INCREMENTS; i++) {
        atomic_fetch_add_explicit(&relaxed_counter, 1, memory_order_relaxed);
    }
    return NULL;
}

// Message passing
atomic_int message_ready = 0;
int message_data = 0;

void* message_producer_broken(void* arg) {
    message_data = 42;
    atomic_store_explicit(&message_ready, 1, memory_order_relaxed);
    return NULL;
}

void* message_producer_correct(void* arg) {
    message_data = 42;
    atomic_store_explicit(&message_ready, 1, memory_order_release);
    return NULL;
}

void* message_consumer_broken(void* arg) {
    while (atomic_load_explicit(&message_ready, memory_order_relaxed) == 0);
    int value = message_data;
    printf("Consumer saw (broken): %d\n", value);
    return NULL;
}

void* message_consumer_correct(void* arg) {
    while (atomic_load_explicit(&message_ready, memory_order_acquire) == 0);
    int value = message_data;
    printf("Consumer saw (correct): %d\n", value);
    return NULL;
}

double measure_time(void* (*func)(void*), void* arg) {
    pthread_t threads[NUM_THREADS];
    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (long i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, func, arg);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
}

int main() {
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  Exercise 01: Atomic Operations & Memory Ordering\n");
    printf("  Threads: %d, Increments per thread: %d\n", NUM_THREADS, INCREMENTS);
    printf("═══════════════════════════════════════════════════════════\n\n");

    // Test 1: Broken counter
    printf("1. BROKEN COUNTER (race condition)\n");
    broken_counter = 0;
    double broken_time = measure_time(broken_increment, NULL);
    printf("   Time: %.3f s\n", broken_time);
    printf("   Expected: %ld\n", (long)NUM_THREADS * INCREMENTS);
    printf("   Got:      %ld", broken_counter);
    if (broken_counter != (long)NUM_THREADS * INCREMENTS) {
        printf(" ← RACE! Lost %ld increments\n",
               (long)NUM_THREADS * INCREMENTS - broken_counter);
        printf("   Why: mov/inc/mov is 3 instructions, can interleave\n");
        printf("   Run 'make asm-01' and search for 'broken_increment'\n");
    } else {
        printf(" (got lucky - run again or increase INCREMENTS)\n");
    }
    printf("\n");

    // Test 2: Sequential consistency
    printf("2. SEQUENTIAL CONSISTENCY (memory_order_seq_cst)\n");
    atomic_store(&seqcst_counter, 0);
    double seqcst_time = measure_time(seqcst_increment, NULL);
    printf("   Time: %.3f s\n", seqcst_time);
    printf("   Got: %ld ✓\n", atomic_load(&seqcst_counter));
    printf("   Cost: Full memory barrier (expensive)\n");
    printf("\n");

    // Test 3: Relaxed ordering
    printf("3. RELAXED ORDERING (memory_order_relaxed)\n");
    atomic_store(&relaxed_counter, 0);
    double relaxed_time = measure_time(relaxed_increment, NULL);
    printf("   Time: %.3f s\n", relaxed_time);
    printf("   Got: %ld ✓\n", atomic_load(&relaxed_counter));
    printf("   Speedup: %.2fx faster than seq_cst\n", seqcst_time / relaxed_time);
    printf("   Why safe: Independent increments, no synchronization needed\n");
    printf("\n");

    // Test 4: Message passing
    printf("4. MESSAGE PASSING PATTERN\n");
    printf("   Testing BROKEN version (relaxed):\n");
    for (int i = 0; i < 5; i++) {
        message_data = 0;
        atomic_store(&message_ready, 0);
        pthread_t prod, cons;
        pthread_create(&cons, NULL, message_consumer_broken, NULL);
        usleep(1000);
        pthread_create(&prod, NULL, message_producer_broken, NULL);
        pthread_join(prod, NULL);
        pthread_join(cons, NULL);
    }

    printf("\n   Testing CORRECT version (acquire/release):\n");
    for (int i = 0; i < 5; i++) {
        message_data = 0;
        atomic_store(&message_ready, 0);
        pthread_t prod, cons;
        pthread_create(&cons, NULL, message_consumer_correct, NULL);
        usleep(1000);
        pthread_create(&prod, NULL, message_producer_correct, NULL);
        pthread_join(prod, NULL);
        pthread_join(cons, NULL);
    }

    printf("\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  KEY INSIGHTS:\n");
    printf("  • Race condition visible in assembly (non-atomic RMW)\n");
    printf("  • seq_cst: Full barrier, total order, slow\n");
    printf("  • relaxed: Fast, but needs careful reasoning\n");
    printf("  • acquire/release: Perfect for message passing\n");
    printf("\n");
    printf("  ANALYSIS:\n");
    printf("  make asm-01     - See 'lock add' instruction\n");
    printf("  make tsan-01    - Detect the race condition\n");
    printf("  make objdump-01 - Full disassembly\n");
    printf("═══════════════════════════════════════════════════════════\n");

    return 0;
}
