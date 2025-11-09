/**
 * Exercise 08: Summary Capstone
 *
 * This exercise combines everything you've learned. You must implement three
 * concurrent counter variants from scratch:
 *
 *   A) Shared counter protected by synchronization (mutex or spinlock)
 *   B) Per-thread counters with and without cache-line padding
 *   C) Producer-consumer using an SPSC lock-free queue
 *
 * CHALLENGE: Implement each variant by recalling concepts from exercises 01-07.
 * No code is provided - you must write the implementations yourself.
 *
 * SUCCESS CRITERIA:
 * - All variants produce correct totals
 * - Variant B shows measurable speedup with padding (2-10x typical)
 * - Variant C passes messages without data races (verify with tsan-08)
 * - You understand WHY each performs differently
 */

#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdalign.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include "benchmark.h"

#define NUM_THREADS 8
#define INCREMENTS 2000000

#define QUEUE_SIZE 1024
#define MASK (QUEUE_SIZE - 1)
#define NUM_MESSAGES 1000000

static atomic_int start_flag = 0;

// ============================================================
// Variant A — Shared counter with synchronization
// ============================================================

// TODO: Add your synchronization primitive here
// Hint: You can use pthread_mutex_t, pthread_spinlock_t, or implement
// your own spinlock using atomics (TAS or TTAS from exercise 05)

static long shared_counter = 0;

static void* variant_a_worker(void* arg) {
    // TODO: Wait for start_flag to become 1
    // Hint: Use memory_order_acquire for proper synchronization

    // TODO: Loop INCREMENTS times
    // TODO: For each iteration:
    //   1. Acquire the lock
    //   2. Increment shared_counter
    //   3. Release the lock

    (void)arg;
    return NULL;
}

static void run_variant_a(void) {
    pthread_t threads[NUM_THREADS];
    (void)threads;  // Remove this line once you create threads

    // TODO: Initialize your synchronization primitive

    shared_counter = 0;
    atomic_store_explicit(&start_flag, 0, memory_order_relaxed);

    printf("═══════════════════════════════════════════════════════════\n");
    printf("Variant A: Shared counter with synchronization\n");

    TIME_BLOCK("Variant A: synchronized counter") {
        // TODO: Create NUM_THREADS threads running variant_a_worker

        // TODO: Signal threads to start by setting start_flag to 1
        // Hint: Use memory_order_release

        // TODO: Join all threads
    }

    long expected = (long)NUM_THREADS * INCREMENTS;
    printf("  Result: %ld (expected %ld) %s\n",
           shared_counter, expected,
           shared_counter == expected ? "✓" : "✗ INCORRECT");

    // TODO: Cleanup your synchronization primitive

    printf("\n");
}

// ============================================================
// Variant B — Per-thread counters (packed vs padded)
// ============================================================

// TODO: Define two counter types:
// 1. packed_counter_t - just an atomic_long value (will have false sharing)
// 2. padded_counter_t - atomic_long with alignas(64) to avoid false sharing
//
// Hint: Review exercise 03 for cache line concepts

static void* variant_b_worker_packed(void* arg) {
    // TODO: Cast arg to your packed counter type

    // TODO: Wait for start_flag (memory_order_acquire)

    // TODO: Loop INCREMENTS times
    // TODO: Increment YOUR thread's counter using atomic operations
    // Hint: Use memory_order_relaxed since no cross-thread synchronization needed

    (void)arg;
    return NULL;
}

static void* variant_b_worker_padded(void* arg) {
    // TODO: Same as packed version, but cast to padded counter type

    (void)arg;
    return NULL;
}

static void run_variant_b(void) {
    pthread_t threads[NUM_THREADS];
    (void)threads;  // Remove this line once you create threads

    printf("═══════════════════════════════════════════════════════════\n");
    printf("Variant B: Per-thread counters (false sharing comparison)\n");

    // TODO: Print sizes of your counter structures
    // Expected: packed ~8 bytes, padded ~64 bytes

    // ----- Packed (false sharing) -----
    // TODO: Allocate array of NUM_THREADS packed counters
    // TODO: Initialize each counter to 0

    atomic_store(&start_flag, 0);

    TIME_BLOCK("Variant B: packed (false sharing)") {
        // TODO: Create threads, each getting pointer to its counter
        // TODO: Signal start
        // TODO: Join threads
    }

    // TODO: Sum all packed counters
    long packed_total = 0;

    printf("  Packed total: %ld (expected %ld) %s\n",
           packed_total, (long)NUM_THREADS * INCREMENTS,
           packed_total == (long)NUM_THREADS * INCREMENTS ? "✓" : "✗");

    // TODO: Free packed counters

    // ----- Padded (no false sharing) -----
    // TODO: Same as above but with padded counters

    atomic_store(&start_flag, 0);

    TIME_BLOCK("Variant B: padded (cache-line isolated)") {
        // TODO: Create threads with padded counters
    }

    long padded_total = 0;

    printf("  Padded total: %ld (expected %ld) %s\n",
           padded_total, (long)NUM_THREADS * INCREMENTS,
           padded_total == (long)NUM_THREADS * INCREMENTS ? "✓" : "✗");

    // TODO: Free padded counters

    printf("\n");
}

// ============================================================
// Variant C — SPSC lock-free queue
// ============================================================

// TODO: Define spsc_queue_t structure with:
// - Fixed-size buffer (int buffer[QUEUE_SIZE])
// - atomic_size_t head (producer writes, consumer reads)
// - atomic_size_t tail (consumer writes, producer reads)
// - Use alignas(64) to keep head/tail on separate cache lines
//
// Hint: Review exercise 07 for SPSC queue patterns

// TODO: Implement spsc_init(spsc_queue_t* q)
// Initialize head and tail to 0

// TODO: Implement spsc_enqueue(spsc_queue_t* q, int value)
// Returns: true if enqueued, false if full
// Key points:
// - Load head with memory_order_relaxed (single producer)
// - Load tail with memory_order_acquire (synchronize with consumer)
// - Check if queue is full: (head + 1) & MASK == tail
// - Store value to buffer[head]
// - Store new head with memory_order_release (publish to consumer)

// TODO: Implement spsc_dequeue(spsc_queue_t* q, int* value)
// Returns: true if dequeued, false if empty
// Key points:
// - Load tail with memory_order_relaxed (single consumer)
// - Load head with memory_order_acquire (synchronize with producer)
// - Check if queue is empty: head == tail
// - Read value from buffer[tail]
// - Store new tail with memory_order_release (publish to producer)

static void* producer(void* arg) {
    // TODO: Cast arg to spsc_queue_t*

    // TODO: Wait for start_flag (memory_order_acquire)

    // TODO: Enqueue values 1 through NUM_MESSAGES
    // Use a busy-wait loop with CPU_PAUSE() when queue is full

    (void)arg;
    return NULL;
}

static void* consumer(void* arg) {
    // TODO: Cast arg to spsc_queue_t*

    // TODO: Initialize counters:
    int received = 0;
    long sum = 0;
    (void)received; (void)sum;  // Remove these once implemented

    // TODO: Wait for start_flag (memory_order_acquire)

    // TODO: Dequeue NUM_MESSAGES values
    // For each value: add it to sum, increment received
    // Use a busy-wait loop with CPU_PAUSE() when queue is empty

    // TODO: Return sum as void* (cast via intptr_t)

    (void)arg;
    return NULL;
}

static void run_variant_c(void) {
    pthread_t prod, cons;
    (void)prod; (void)cons;  // Remove these once you create threads

    // TODO: Declare and initialize your spsc_queue_t

    atomic_store(&start_flag, 0);

    long expected = ((long)NUM_MESSAGES * (NUM_MESSAGES + 1)) / 2;

    printf("═══════════════════════════════════════════════════════════\n");
    printf("Variant C: SPSC lock-free queue\n");

    TIME_BLOCK("Variant C: message passing") {
        // TODO: Create consumer thread
        // TODO: Create producer thread

        // TODO: Signal start (memory_order_release)

        // TODO: Join both threads
        // TODO: Extract sum from consumer's return value

        long observed = 0;  // TODO: Get actual sum from consumer

        printf("  Sum: %ld (expected %ld) %s\n",
               observed, expected,
               observed == expected ? "✓" : "✗");
    }

    printf("\n");
}

int main(void) {
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  Exercise 08: Summary Capstone\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("\n");
    printf("IMPLEMENT all three variants below:\n");
    printf("  A) Synchronized shared counter (mutex or spinlock)\n");
    printf("  B) Per-thread counters (observe false sharing!)\n");
    printf("  C) SPSC queue with acquire/release ordering\n");
    printf("\n");

    run_variant_a();
    run_variant_b();
    run_variant_c();

    printf("═══════════════════════════════════════════════════════════\n");
    printf("Analysis tasks:\n");
    printf("  1. Run 'make perf-08' - compare cache-misses between variants\n");
    printf("  2. Run 'make asm-08' - verify LOCK prefix and memory barriers\n");
    printf("  3. Run 'make tsan-08' - check for data races\n");
    printf("  4. Why is padded faster than packed?\n");
    printf("  5. Why is SPSC faster than mutex?\n");
    printf("═══════════════════════════════════════════════════════════\n");

    return 0;
}
