/**
 * Exercise 07: Lock-Free Single-Producer Single-Consumer (SPSC) Queue
 *
 * PRACTICAL APPLICATION: This is used everywhere in high-performance systems:
 * - Database query pipelines
 * - Audio/video processing
 * - Network packet handling
 * - Inter-thread message passing
 *
 * CHALLENGE: Implement a lock-free ring buffer queue
 *
 * KEY INSIGHTS:
 * - Only ONE producer, ONE consumer = simpler than MPMC
 * - Use memory ordering to synchronize without locks
 * - Understand cache line bouncing (false sharing)
 *
 * MEMORY ORDERING REQUIREMENTS:
 * - Producer writes data, THEN increments head (release)
 * - Consumer reads head (acquire), THEN reads data
 * - This creates happens-before relationship
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <unistd.h>
#include <sched.h>
#include <time.h>
#include "benchmark.h"

#ifndef ALIGN_HEAD
#define ALIGN_HEAD 64
#endif

#ifndef ALIGN_TAIL
#define ALIGN_TAIL 64
#endif

#if ALIGN_HEAD
#define HEAD_ALIGN_SPEC alignas(ALIGN_HEAD)
#else
#define HEAD_ALIGN_SPEC
#endif

#if ALIGN_TAIL
#define TAIL_ALIGN_SPEC alignas(ALIGN_TAIL)
#else
#define TAIL_ALIGN_SPEC
#endif

#define QUEUE_SIZE 1024  // Must be power of 2
#define MASK (QUEUE_SIZE - 1)
#define NUM_MESSAGES 10000000

// Lock-free SPSC Queue
typedef struct {
    // The ring buffer
    int buffer[QUEUE_SIZE];
    HEAD_ALIGN_SPEC atomic_size_t head;
    TAIL_ALIGN_SPEC atomic_size_t tail;
} spsc_queue_t;

void queue_init(spsc_queue_t *q) {
    atomic_store(&q->head, 0);
    atomic_store(&q->tail, 0);
}

static void print_layout_info(spsc_queue_t *q) {
    uintptr_t queue_addr = (uintptr_t)q;
    uintptr_t head_addr = (uintptr_t)&q->head;
    uintptr_t tail_addr = (uintptr_t)&q->tail;

    printf("Alignment config: ALIGN_HEAD=%d, ALIGN_TAIL=%d\n", ALIGN_HEAD, ALIGN_TAIL);
    printf("sizeof(spsc_queue_t)=%zu, align=%zu\n",
           sizeof(spsc_queue_t), (size_t)_Alignof(spsc_queue_t));
    printf("Offsets: head=%zu, tail=%zu\n",
           offsetof(spsc_queue_t, head), offsetof(spsc_queue_t, tail));
    printf("Addresses (mod 64): queue=%p (%zu), head=%p (%zu), tail=%p (%zu)\n",
           (void *)q, (size_t)(queue_addr & 63),
           (void *)&q->head, (size_t)(head_addr & 63),
           (void *)&q->tail, (size_t)(tail_addr & 63));
}

static inline void adaptive_backoff(unsigned *attempts) {
    if (*attempts < 32) {
        __builtin_ia32_pause();
    } else if (*attempts < 64) {
        sched_yield();
    } else {
        struct timespec ts = {0, 50000}; // 50 microseconds
        nanosleep(&ts, NULL);
    }
    (*attempts)++;
}

// Producer enqueues a value
bool queue_enqueue(spsc_queue_t *q, int value) {
    size_t head = atomic_load_explicit(&q->head, memory_order_relaxed);
    size_t next_head = (head + 1) & MASK;
    size_t tail = atomic_load_explicit(&q->tail, memory_order_acquire);

    if (next_head == tail) {
        return false;  // Queue full
    }

    // Write data to buffer
    q->buffer[head] = value;
    atomic_store_explicit(&q->head, next_head, memory_order_release);
    return true;
}

// Consumer dequeues a value
bool queue_dequeue(spsc_queue_t *q, int *value) {
    size_t tail = atomic_load_explicit(&q->tail, memory_order_relaxed);

    size_t head = atomic_load_explicit(&q->head, memory_order_acquire);
    if (head == tail) {
        return false;  // Queue empty
    }

    // Read data from buffer
    *value = q->buffer[tail];
    size_t next_tail = (tail + 1) & MASK;
    atomic_store_explicit(&q->tail, next_tail, memory_order_release);
    return true;
}

// Test: Producer thread
void *producer(void *arg) {
    spsc_queue_t *q = (spsc_queue_t *)arg;
    unsigned backoff = 0;

    for (int i = 0; i < NUM_MESSAGES; i++) {
        while (!queue_enqueue(q, i)) {
            adaptive_backoff(&backoff);
        }
        backoff = 0;
    }

    return NULL;
}

// Test: Consumer thread
void *consumer(void *arg) {
    spsc_queue_t *q = (spsc_queue_t *)arg;
    int value;
    int received = 0;
    unsigned backoff = 0;

    while (received < NUM_MESSAGES) {
        if (queue_dequeue(q, &value)) {
            if (value != received) {
                printf("ERROR: Expected %d, got %d\n", received, value);
                exit(1);
            }
            received++;
            backoff = 0;
        } else {
            adaptive_backoff(&backoff);
        }
    }

    return NULL;
}

int main() {
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  Exercise 07: Lock-Free SPSC Queue\n");
    printf("  Messages: %d, Queue size: %d\n", NUM_MESSAGES, QUEUE_SIZE);
    printf("═══════════════════════════════════════════════════════════\n\n");

    spsc_queue_t *queue = cache_aligned_alloc(sizeof(spsc_queue_t));
    if (!queue) {
        fprintf(stderr, "Failed to allocate queue\n");
        return 1;
    }
    queue_init(queue);
    print_layout_info(queue);
    printf("perf tip: perf stat -e cache-misses,cache-references "
           "./exercises/07_lockfree_queue/07_lockfree_queue\n");

    pthread_t prod, cons;

    printf("Testing lock-free queue...\n");
    uint64_t start_nanos = get_nanos();
    TIME_BLOCK("SPSC queue throughput") {
        pthread_create(&cons, NULL, consumer, queue);
        pthread_create(&prod, NULL, producer, queue);

        pthread_join(prod, NULL);
        pthread_join(cons, NULL);
    }
    uint64_t end_nanos = get_nanos();

    printf("✓ All messages received in order!\n\n");

    // Performance metrics
    double elapsed_sec = (end_nanos - start_nanos) / 1e9;
    double msg_per_sec = elapsed_sec > 0 ? NUM_MESSAGES / elapsed_sec : 0.0;
    printf("Throughput: %.2f million messages/sec\n", msg_per_sec / 1e6);

    printf("\n═══════════════════════════════════════════════════════════\n");
    printf("  KEY INSIGHTS:\n");
    printf("  • SPSC: Single producer/consumer = no CAS needed!\n");
    printf("  • Cache alignment: 2-3x speedup (prevents false sharing)\n");
    printf("  • acquire-release: Creates happens-before relationship\n");
    printf("  • Ring buffer: Modulo arithmetic for wrap-around\n");
    printf("\n");
    printf("  MEMORY ORDERING BREAKDOWN:\n");
    printf("  Producer:\n");
    printf("    1. Write data to buffer[head]\n");
    printf("    2. Store head (RELEASE) ← Publishes data\n");
    printf("  Consumer:\n");
    printf("    3. Load head (ACQUIRE) ← Sees published data\n");
    printf("    4. Read data from buffer[tail]\n");
    printf("    5. Store tail (RELEASE) ← Frees slot\n");
    printf("  Producer:\n");
    printf("    6. Load tail (ACQUIRE) ← Sees freed slot\n");
    printf("\n");
    printf("  WHY NO CAS?\n");
    printf("  • Only one writer per variable (head/tail)\n");
    printf("  • Producer writes head, consumer only reads it\n");
    printf("  • Consumer writes tail, producer only reads it\n");
    printf("  • This is the beauty of SPSC!\n");
    printf("\n");
    printf("  ANALYSIS:\n");
    printf("  make asm-07     - See plain mov (no lock prefix!)\n");
    printf("  make perf-07    - Measure cache-misses\n");
    printf("  make tsan-07    - Verify no races\n");
    printf("\n");
    printf("  EXPERIMENT:\n");
    printf("  Remove alignas(64) from head/tail and re-run.\n");
    printf("  Observe 2-3x slowdown from false sharing!\n");
    printf("═══════════════════════════════════════════════════════════\n");

    free(queue);
    return 0;
}
