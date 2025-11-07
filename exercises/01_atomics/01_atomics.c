/**
 * Exercise 01: Atomic Operations and Memory Ordering
 *
 * CHALLENGE: Understand performance implications of different memory orders.
 * You'll implement three versions and compare:
 * 1. Regular counter (BROKEN - race condition)
 * 2. Sequential consistency (CORRECT but slow)
 * 3. Relaxed ordering (CORRECT and fast - understand why)
 *
 * LEARNING GOALS:
 * - See race condition in assembly (read-modify-write not atomic)
 * - Understand seq_cst overhead (MFENCE on x86)
 * - Learn when relaxed is safe (independent increments)
 * - Measure actual performance difference
 */

#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>

#define NUM_THREADS 8
#define INCREMENTS 5000000

// Global counters for testing
long broken_counter = 0;                    // Race condition
atomic_long seqcst_counter = 0;             // Sequential consistency
atomic_long relaxed_counter = 0;            // Relaxed ordering

// =============================================================================
// PART 1: BROKEN - Race Condition
// =============================================================================

void* broken_increment(void* arg) {
    for (int i = 0; i < INCREMENTS; i++) {
        broken_counter++;  // NOT ATOMIC
        // Assembly on x86_64:
        //   mov    (%rax), %edx      ← Read
        //   inc    %edx              ← Modify
        //   mov    %edx, (%rax)      ← Write
        // These 3 instructions can interleave between threads!
    }
    return NULL;
}

// =============================================================================
// PART 2: TODO - Sequential Consistency
// =============================================================================

void* seqcst_increment(void* arg) {
    for (int i = 0; i < INCREMENTS; i++) {
        // TODO: Use atomic_fetch_add with memory_order_seq_cst
        // Hint: atomic_fetch_add_explicit(&seqcst_counter, 1, memory_order_seq_cst);
        //
        // This will generate on x86_64:
        //   lock add QWORD PTR [rax], 0x1
        // The LOCK prefix:
        //   - Asserts LOCK# signal on memory bus
        //   - Forces cache line to Exclusive state (MESI protocol)
        //   - Provides full memory barrier (no reordering across it)
    }
    return NULL;
}

// =============================================================================
// PART 3: TODO - Relaxed Ordering (The Fast Path)
// =============================================================================

void* relaxed_increment(void* arg) {
    for (int i = 0; i < INCREMENTS; i++) {
        // TODO: Use atomic_fetch_add with memory_order_relaxed
        // Hint: atomic_fetch_add_explicit(&relaxed_counter, 1, memory_order_relaxed);
        //
        // WHY IS THIS SAFE?
        // - Each increment is independent (no happens-before needed)
        // - We only care about final value, not intermediate states
        // - No other memory operations to order relative to
        //
        // Assembly on x86_64: Same as seq_cst (lock add)
        // On ARM: May use LDXR/STXR without DMB (data memory barrier)
        // Performance win: Compiler can reorder, CPU can use store buffer
    }
    return NULL;
}

// =============================================================================
// PART 4: TODO - Message Passing (Relaxed is WRONG here!)
// =============================================================================

// This is where memory ordering actually matters
atomic_int message_ready = 0;
int message_data = 0;  // NOT atomic, protected by ordering

void* message_producer_broken(void* arg) {
    message_data = 42;
    // TODO: This is BROKEN - use relaxed ordering
    // Hint: atomic_store_explicit(&message_ready, 1, memory_order_relaxed);
    //
    // BUG: Compiler or CPU might reorder these two stores!
    // Consumer might see message_ready=1 but message_data=0
    return NULL;
}

void* message_producer_correct(void* arg) {
    message_data = 42;
    // TODO: Fix with release ordering
    // Hint: atomic_store_explicit(&message_ready, 1, memory_order_release);
    //
    // CORRECT: Release barrier prevents earlier stores from moving after
    // All stores before this are visible to thread doing acquire load
    return NULL;
}

void* message_consumer_broken(void* arg) {
    // TODO: BROKEN - use relaxed ordering
    // Hint: while (atomic_load_explicit(&message_ready, memory_order_relaxed) == 0);
    int value = message_data;
    printf("Consumer saw (broken): %d\n", value);
    return NULL;
}

void* message_consumer_correct(void* arg) {
    // TODO: Fix with acquire ordering
    // Hint: while (atomic_load_explicit(&message_ready, memory_order_acquire) == 0);
    //
    // CORRECT: Acquire barrier prevents later loads from moving before
    // Synchronized-with the release store in producer
    int value = message_data;
    printf("Consumer saw (correct): %d\n", value);
    return NULL;
}

// =============================================================================
// Measurement Infrastructure
// =============================================================================

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

// =============================================================================
// Main
// =============================================================================

int main() {
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  Exercise 01: Atomic Operations & Memory Ordering\n");
    printf("  Threads: %d, Increments per thread: %d\n", NUM_THREADS, INCREMENTS);
    printf("═══════════════════════════════════════════════════════════\n\n");

    // Test 1: Broken counter (race condition)
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
    printf("2. TODO: SEQUENTIAL CONSISTENCY (memory_order_seq_cst)\n");
    printf("   Implement seqcst_increment() first!\n");
    // TODO: Uncomment after implementing seqcst_increment
    /*
    atomic_store(&seqcst_counter, 0);
    double seqcst_time = measure_time(seqcst_increment, NULL);
    printf("   Time: %.3f s\n", seqcst_time);
    printf("   Got: %ld ✓\n", atomic_load(&seqcst_counter));
    printf("   Cost: Full memory barrier (expensive)\n");
    printf("\n");
    */

    // Test 3: Relaxed ordering
    printf("3. TODO: RELAXED ORDERING (memory_order_relaxed)\n");
    printf("   Implement relaxed_increment() second!\n");
    // TODO: Uncomment after implementing relaxed_increment
    /*
    atomic_store(&relaxed_counter, 0);
    double relaxed_time = measure_time(relaxed_increment, NULL);
    printf("   Time: %.3f s\n", relaxed_time);
    printf("   Got: %ld ✓\n", atomic_load(&relaxed_counter));
    printf("   Speedup: %.2fx faster than seq_cst\n", seqcst_time / relaxed_time);
    printf("   Why safe: Independent increments, no synchronization needed\n");
    printf("\n");
    */

    // Test 4: Message passing (demonstrate ordering matters)
    printf("4. TODO: MESSAGE PASSING PATTERN\n");
    printf("   This shows where memory ordering REALLY matters!\n");
    printf("   Implement the producer/consumer functions.\n");
    printf("\n");

    // TODO: Uncomment after implementing message passing
    /*
    printf("   Testing BROKEN version (relaxed):\n");
    for (int i = 0; i < 5; i++) {
        message_data = 0;
        atomic_store(&message_ready, 0);
        pthread_t prod, cons;
        pthread_create(&cons, NULL, message_consumer_broken, NULL);
        usleep(1000);  // Let consumer start waiting
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
    */

    printf("\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  NEXT STEPS:\n");
    printf("  1. Complete all TODOs above\n");
    printf("  2. Run: make asm-01\n");
    printf("     Look for 'lock add' vs plain 'add' instructions\n");
    printf("  3. Run: make tsan-01\n");
    printf("     ThreadSanitizer will catch the broken_counter race\n");
    printf("  4. Compare assembly for seq_cst vs relaxed\n");
    printf("     On x86: Might be same (strong memory model)\n");
    printf("     On ARM: Relaxed omits DMB barriers\n");
    printf("═══════════════════════════════════════════════════════════\n");

    return 0;
}
