/**
 * Exercise 08: Summary Capstone
 *
 * Combines: mutex/spinlock, per-thread counters with/without padding,
 * and a lock-free SPSC message-passing path using acquire/release.
 *
 * Build: make 08_summary
 * Run:   make run-08
 */

#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdalign.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include "benchmark.h"

#define NUM_THREADS 8
#define INCREMENTS 2000000

// Start synchronization (atomic flag approach)
static atomic_int start_flag = 0;

// =============================
// Variant A: Locked counter
// =============================

static long shared_counter = 0;

static void* variant_a_worker_mutex(void* arg) {
    pthread_mutex_t* m = (pthread_mutex_t*)arg;
    while (atomic_load_explicit(&start_flag, memory_order_acquire) == 0) {
        CPU_PAUSE();
    }
    for (int i = 0; i < INCREMENTS; i++) {
        pthread_mutex_lock(m);
        shared_counter++;
        pthread_mutex_unlock(m);
    }
    return NULL;
}

static void run_variant_a_mutex(void) {
    pthread_t th[NUM_THREADS];
    pthread_mutex_t m;
    pthread_mutex_init(&m, NULL);
    shared_counter = 0;
    atomic_store_explicit(&start_flag, 0, memory_order_relaxed);

    for (long i = 0; i < NUM_THREADS; i++) {
        pthread_create(&th[i], NULL, variant_a_worker_mutex, &m);
    }

    TIME_BLOCK("Variant A: mutex-protected counter") {
        atomic_store_explicit(&start_flag, 1, memory_order_release);
        for (int i = 0; i < NUM_THREADS; i++) pthread_join(th[i], NULL);
    }

    long expected = (long)NUM_THREADS * INCREMENTS;
    printf("  Result: %ld (expected %ld)\n\n", shared_counter, expected);
    pthread_mutex_destroy(&m);
}

// TODO: Optional — Implement TAS/TTAS and compare against pthread_mutex

// =============================================
// Variant B: Per-thread counters + merge (packed)
// =============================================

typedef struct {
    atomic_long value; // false sharing when in an array
} packed_counter_t;

typedef struct {
    alignas(64) atomic_long value; // padded to its own cache line
} padded_counter_t;

static void* variant_b_worker_packed(void* arg) {
    packed_counter_t* counters = (packed_counter_t*)arg;
    long tid = 0;
    pthread_t self = pthread_self();
    // Map thread to index by scanning the array address range once.
    // Simpler: pass tid via thread arg separately; here we keep it minimal.
    // For clarity, we pass the pointer to the slot instead.
    atomic_fetch_add_explicit(&((packed_counter_t*)arg)->value, 0, memory_order_relaxed);
    // Real work loop
    while (atomic_load_explicit(&start_flag, memory_order_acquire) == 0) CPU_PAUSE();
    for (int i = 0; i < INCREMENTS; i++) {
        atomic_fetch_add_explicit(&((packed_counter_t*)arg)->value, 1, memory_order_relaxed);
    }
    (void)counters; (void)tid; (void)self;
    return NULL;
}

static void* variant_b_worker_padded(void* arg) {
    while (atomic_load_explicit(&start_flag, memory_order_acquire) == 0) CPU_PAUSE();
    for (int i = 0; i < INCREMENTS; i++) {
        atomic_fetch_add_explicit(&((padded_counter_t*)arg)->value, 1, memory_order_relaxed);
    }
    return NULL;
}

static void run_variant_b(void) {
    // Packed (false sharing)
    pthread_t th[NUM_THREADS];
    packed_counter_t* packed = calloc(NUM_THREADS, sizeof(packed_counter_t));
    for (int i = 0; i < NUM_THREADS; i++) atomic_store(&packed[i].value, 0);
    atomic_store(&start_flag, 0);
    for (long i = 0; i < NUM_THREADS; i++) {
        // Pass pointer to each slot as the arg
        pthread_create(&th[i], NULL, variant_b_worker_packed, &packed[i]);
    }
    TIME_BLOCK("Variant B: per-thread (packed, false sharing)") {
        atomic_store_explicit(&start_flag, 1, memory_order_release);
        for (int i = 0; i < NUM_THREADS; i++) pthread_join(th[i], NULL);
    }
    long packed_sum = 0;
    for (int i = 0; i < NUM_THREADS; i++) packed_sum += atomic_load(&packed[i].value);
    printf("  Result: %ld (expected %ld)\n", packed_sum, (long)NUM_THREADS * INCREMENTS);
    free(packed);

    // Padded (no false sharing)
    padded_counter_t* padded = calloc(NUM_THREADS, sizeof(padded_counter_t));
    for (int i = 0; i < NUM_THREADS; i++) atomic_store(&padded[i].value, 0);
    atomic_store(&start_flag, 0);
    for (long i = 0; i < NUM_THREADS; i++) pthread_create(&th[i], NULL, variant_b_worker_padded, &padded[i]);
    TIME_BLOCK("Variant B: per-thread (padded, no false sharing)") {
        atomic_store_explicit(&start_flag, 1, memory_order_release);
        for (int i = 0; i < NUM_THREADS; i++) pthread_join(th[i], NULL);
    }
    long padded_sum = 0;
    for (int i = 0; i < NUM_THREADS; i++) padded_sum += atomic_load(&padded[i].value);
    printf("  Result: %ld (expected %ld)\n\n", padded_sum, (long)NUM_THREADS * INCREMENTS);
    free(padded);
}

// ===========================================
// Variant C: SPSC queue (message passing)
// ===========================================

#define QUEUE_SIZE 1024
#define MASK (QUEUE_SIZE - 1)
#define NUM_MESSAGES 1000000

typedef struct {
    int buffer[QUEUE_SIZE];
    alignas(64) atomic_size_t head; // producer writes
    alignas(64) atomic_size_t tail; // consumer writes
} spsc_queue_t;

static inline void spsc_init(spsc_queue_t* q) {
    atomic_store(&q->head, 0);
    atomic_store(&q->tail, 0);
}

static inline bool spsc_enqueue(spsc_queue_t* q, int v) {
    size_t head = atomic_load_explicit(&q->head, memory_order_relaxed);
    size_t next = (head + 1) & MASK;
    size_t tail = atomic_load_explicit(&q->tail, memory_order_acquire);
    if (next == tail) return false; // full
    q->buffer[head] = v;
    atomic_store_explicit(&q->head, next, memory_order_release);
    return true;
}

static inline bool spsc_dequeue(spsc_queue_t* q, int* out) {
    size_t tail = atomic_load_explicit(&q->tail, memory_order_relaxed);
    size_t head = atomic_load_explicit(&q->head, memory_order_acquire);
    if (head == tail) return false; // empty
    *out = q->buffer[tail];
    atomic_store_explicit(&q->tail, (tail + 1) & MASK, memory_order_release);
    return true;
}

typedef struct { spsc_queue_t* q; } prod_arg_t;
typedef struct { spsc_queue_t* q; long sum; } cons_arg_t;

static void* producer(void* arg) {
    spsc_queue_t* q = ((prod_arg_t*)arg)->q;
    while (atomic_load_explicit(&start_flag, memory_order_acquire) == 0) CPU_PAUSE();
    for (int i = 1; i <= NUM_MESSAGES; i++) {
        while (!spsc_enqueue(q, i)) CPU_PAUSE();
    }
    return NULL;
}

static void* consumer(void* arg) {
    cons_arg_t* ca = (cons_arg_t*)arg;
    spsc_queue_t* q = ca->q;
    long local = 0;
    while (atomic_load_explicit(&start_flag, memory_order_acquire) == 0) CPU_PAUSE();
    int got = 0, v;
    while (got < NUM_MESSAGES) {
        if (spsc_dequeue(q, &v)) { local += v; got++; }
        else CPU_PAUSE();
    }
    ca->sum = local;
    return NULL;
}

static void run_variant_c(void) {
    spsc_queue_t q; spsc_init(&q);
    pthread_t pt, ct;
    prod_arg_t pa = { .q = &q };
    cons_arg_t ca = { .q = &q, .sum = 0 };
    atomic_store(&start_flag, 0);
    pthread_create(&ct, NULL, consumer, &ca);
    pthread_create(&pt, NULL, producer, &pa);
    long expected = ((long)NUM_MESSAGES * (NUM_MESSAGES + 1)) / 2; // sum(1..N)
    TIME_BLOCK("Variant C: SPSC queue (acq/rel)") {
        atomic_store_explicit(&start_flag, 1, memory_order_release);
        pthread_join(pt, NULL);
        pthread_join(ct, NULL);
    }
    printf("  Sum: %ld (expected %ld)\n\n", ca.sum, expected);
}

int main(void) {
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  Exercise 08: Summary Capstone\n");
    printf("  Threads: %d, Increments: %d, Messages: %d\n", NUM_THREADS, INCREMENTS, NUM_MESSAGES);
    printf("═══════════════════════════════════════════════════════════\n\n");

    run_variant_a_mutex();
    run_variant_b();
    run_variant_c();

    printf("Notes:\n");
    printf("- Replace mutex with pthread_spinlock_t or TAS/TTAS to compare.\n");
    printf("- Remove alignas(64) in Variant B to observe false sharing.\n");
    printf("- Inspect assembly: make asm-08 (no lock prefix in SPSC path).\n");

    return 0;
}

