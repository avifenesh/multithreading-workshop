/**
 * Exercise 08: Summary Capstone
 *
 * Three variants to reinforce atomics, synchronization, and cache effects:
 *   A) Contended counter protected by a mutex/spinlock
 *   B) Per-thread counters (packed vs alignas(64) padded) + merge phase
 *   C) SPSC queue (producer/consumer) using acquire/release ordering
 *
 * ✅ This file is intentionally incomplete. Every TODO/FIXME must be replaced
 *    with a working implementation. Use exercises 01, 03, 05, and 07 as guides.
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

// Flip to 1 once Variant C is implemented (prevents hanging while queue code is incomplete)
#ifndef ENABLE_VARIANT_C
#define ENABLE_VARIANT_C 0
#endif

static atomic_int start_flag = 0;
static long shared_counter = 0;

// ============================================================
// Variant A — Mutex/spinlock protected counter (baseline)
// ============================================================

static void* variant_a_worker_mutex(void* arg) {
    pthread_mutex_t* lock = (pthread_mutex_t*)arg;

    // Wait until main thread flips start_flag (message ordering: release/acquire)
    while (atomic_load_explicit(&start_flag, memory_order_acquire) == 0) {
        CPU_PAUSE();
    }

    // TODO:
    //   • Loop INCREMENTS times.
    //   • Guard shared_counter++ with pthread_mutex_lock/unlock (or TAS/TTAS spinlock).
    //   • Optional: replace pthread_mutex_t with pthread_spinlock_t or custom lock.
    for (int i = 0; i < INCREMENTS; i++) {
        // pthread_mutex_lock(lock);
        // shared_counter++;
        // pthread_mutex_unlock(lock);
    }

    (void)lock;  // Remove once lock/unlock is hooked up
    return NULL;
}

static void run_variant_a_mutex(void) {
    pthread_t threads[NUM_THREADS];
    pthread_mutex_t lock;
    pthread_mutex_init(&lock, NULL);
    shared_counter = 0;
    atomic_store_explicit(&start_flag, 0, memory_order_relaxed);

    printf("═══════════════════════════════════════════════════════════\n");
    printf("Variant A: Mutex/spinlock baseline\n");

    TIME_BLOCK("Variant A: contended counter") {
        for (long i = 0; i < NUM_THREADS; i++) {
            pthread_create(&threads[i], NULL, variant_a_worker_mutex, &lock);
        }

        atomic_store_explicit(&start_flag, 1, memory_order_release);

        for (int i = 0; i < NUM_THREADS; i++) {
            pthread_join(threads[i], NULL);
        }
    }

    long expected = (long)NUM_THREADS * INCREMENTS;
    printf("  Result: %ld (expected %ld)\n", shared_counter, expected);
    printf("  TODO: shared_counter should match expected once lock is implemented.\n\n");

    pthread_mutex_destroy(&lock);
}

// ============================================================
// Variant B — Per-thread counters (packed vs padded)
// ============================================================

typedef struct {
    atomic_long value;  // False sharing when contiguous
} packed_counter_t;

typedef struct {
    alignas(64) atomic_long value;  // Each counter sits on its own cache line
} padded_counter_t;

static void* variant_b_worker_packed(void* arg) {
    packed_counter_t* slot = (packed_counter_t*)arg;

    while (atomic_load_explicit(&start_flag, memory_order_acquire) == 0) {
        CPU_PAUSE();
    }

    // TODO: Increment slot->value INCREMENTS times using memory_order_relaxed.
    for (int i = 0; i < INCREMENTS; i++) {
        // atomic_fetch_add_explicit(&slot->value, 1, memory_order_relaxed);
    }

    (void)slot;
    return NULL;
}

static void* variant_b_worker_padded(void* arg) {
    padded_counter_t* slot = (padded_counter_t*)arg;

    while (atomic_load_explicit(&start_flag, memory_order_acquire) == 0) {
        CPU_PAUSE();
    }

    // TODO: Same as packed version but padding eliminates false sharing.
    for (int i = 0; i < INCREMENTS; i++) {
        // atomic_fetch_add_explicit(&slot->value, 1, memory_order_relaxed);
    }

    (void)slot;
    return NULL;
}

static void run_variant_b(void) {
    pthread_t threads[NUM_THREADS];

    printf("═══════════════════════════════════════════════════════════\n");
    printf("Variant B: Per-thread counters (packed vs padded)\n");
    printf("  packed size: %zu bytes, padded size: %zu bytes\n",
           sizeof(packed_counter_t), sizeof(padded_counter_t));

    // ----- Packed (false sharing) -----
    packed_counter_t* packed = calloc(NUM_THREADS, sizeof(packed_counter_t));
    for (int i = 0; i < NUM_THREADS; i++) atomic_store(&packed[i].value, 0);
    atomic_store(&start_flag, 0);

    TIME_BLOCK("Variant B: packed (false sharing)") {
        for (long i = 0; i < NUM_THREADS; i++) {
            pthread_create(&threads[i], NULL, variant_b_worker_packed, &packed[i]);
        }
        atomic_store_explicit(&start_flag, 1, memory_order_release);
        for (int i = 0; i < NUM_THREADS; i++) pthread_join(threads[i], NULL);
    }

    long packed_total = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        packed_total += atomic_load(&packed[i].value);
    }
    printf("  Packed total: %ld (expected %ld)\n",
           packed_total, (long)NUM_THREADS * INCREMENTS);
    free(packed);

    // ----- Padded (no false sharing) -----
    padded_counter_t* padded = calloc(NUM_THREADS, sizeof(padded_counter_t));
    for (int i = 0; i < NUM_THREADS; i++) atomic_store(&padded[i].value, 0);
    atomic_store(&start_flag, 0);

    TIME_BLOCK("Variant B: padded (cache-line isolated)") {
        for (long i = 0; i < NUM_THREADS; i++) {
            pthread_create(&threads[i], NULL, variant_b_worker_padded, &padded[i]);
        }
        atomic_store_explicit(&start_flag, 1, memory_order_release);
        for (int i = 0; i < NUM_THREADS; i++) pthread_join(threads[i], NULL);
    }

    long padded_total = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        padded_total += atomic_load(&padded[i].value);
    }
    printf("  Padded total: %ld (expected %ld)\n\n",
           padded_total, (long)NUM_THREADS * INCREMENTS);
    free(padded);
}

// ============================================================
// Variant C — SPSC queue (single producer/single consumer)
// ============================================================

#if ENABLE_VARIANT_C

typedef struct {
    int buffer[QUEUE_SIZE];

    // TODO: Keep head/tail on separate cache lines to avoid ping-pong.
    alignas(64) atomic_size_t head;  // Producer writes
    alignas(64) atomic_size_t tail;  // Consumer writes
} spsc_queue_t;

static inline void spsc_init(spsc_queue_t* q) {
    // TODO: Initialize head/tail to 0 (memory_order_relaxed is fine).
    atomic_store(&q->head, 0);  // FIXME: replace placeholder once implemented
    atomic_store(&q->tail, 0);  // FIXME: replace placeholder once implemented
}

static inline bool spsc_enqueue(spsc_queue_t* q, int value) {
    size_t head = atomic_load_explicit(&q->head, memory_order_relaxed);
    size_t next_head = (head + 1) & MASK;

    // TODO: Load tail with memory_order_acquire to check for full queue.
    size_t tail = 0;  // FIXME: use atomic_load_explicit(&q->tail, memory_order_acquire);

    if (next_head == tail) {
        return false;  // Queue full
    }

    q->buffer[head] = value;

    // TODO: Publish new head with memory_order_release.
    // atomic_store_explicit(&q->head, next_head, memory_order_release);
    (void)next_head;  // Remove once store is added

    return true;
}

static inline bool spsc_dequeue(spsc_queue_t* q, int* value) {
    size_t tail = atomic_load_explicit(&q->tail, memory_order_relaxed);

    // TODO: Load head with memory_order_acquire to observe producer writes.
    size_t head = 0;  // FIXME: use atomic_load_explicit(&q->head, memory_order_acquire);

    if (head == tail) {
        return false;  // Queue empty
    }

    *value = q->buffer[tail];

    // TODO: Store tail with memory_order_release to free the slot.
    // atomic_store_explicit(&q->tail, (tail + 1) & MASK, memory_order_release);
    (void)tail;  // Remove once store is added
    return true;
}

static void* producer(void* arg) {
    spsc_queue_t* q = (spsc_queue_t*)arg;

    while (atomic_load_explicit(&start_flag, memory_order_acquire) == 0) {
        CPU_PAUSE();
    }

    for (int i = 1; i <= NUM_MESSAGES; i++) {
        while (!spsc_enqueue(q, i)) {
            CPU_PAUSE();
        }
    }

    return NULL;
}

static void* consumer(void* arg) {
    spsc_queue_t* q = (spsc_queue_t*)arg;
    int value;
    int received = 0;
    long sum = 0;

    while (atomic_load_explicit(&start_flag, memory_order_acquire) == 0) {
        CPU_PAUSE();
    }

    while (received < NUM_MESSAGES) {
        if (spsc_dequeue(q, &value)) {
            sum += value;
            received++;
        } else {
            CPU_PAUSE();
        }
    }

    return (void*)(intptr_t)sum;
}

static void run_variant_c(void) {
    pthread_t prod, cons;
    spsc_queue_t queue;
    spsc_init(&queue);
    atomic_store(&start_flag, 0);

    long expected = ((long)NUM_MESSAGES * (NUM_MESSAGES + 1)) / 2;

    printf("═══════════════════════════════════════════════════════════\n");
    printf("Variant C: SPSC queue (acquire/release)\n");

    TIME_BLOCK("Variant C: SPSC throughput") {
        pthread_create(&cons, NULL, consumer, &queue);
        pthread_create(&prod, NULL, producer, &queue);

        atomic_store_explicit(&start_flag, 1, memory_order_release);

        void* sum_ptr;
        pthread_join(prod, NULL);
        pthread_join(cons, &sum_ptr);

        long observed = (long)(intptr_t)sum_ptr;
        printf("  Sum: %ld (expected %ld)\n", observed, expected);
        printf("  TODO: Fix TODOs in queue implementation so observed==expected.\n");
    }

    printf("\n");
}

#endif  // ENABLE_VARIANT_C

int main(void) {
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  Exercise 08: Summary Capstone\n");
    printf("═══════════════════════════════════════════════════════════\n\n");

    run_variant_a_mutex();
    run_variant_b();

#if ENABLE_VARIANT_C
    run_variant_c();
#else
    printf("═══════════════════════════════════════════════════════════\n");
    printf("Variant C is disabled (ENABLE_VARIANT_C=0).\n");
    printf("Implement the SPSC queue and set ENABLE_VARIANT_C=1 to run it.\n\n");
#endif

    printf("Next steps:\n");
    printf("  • Fill every TODO above (locks, atomic increments, queue ordering).\n");
    printf("  • Compare timings, inspect assembly (make asm-08), and run perf.\n");
    printf("  • Optional: swap in TAS vs TTAS and document the difference.\n");

    return 0;
}
