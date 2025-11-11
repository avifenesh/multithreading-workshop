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
 * - CAS = Compare-And-Swap (aka compare-exchange) — not needed in SPSC
 *
 * MEMORY ORDERING REQUIREMENTS:
 * - Producer writes data, THEN increments head (release)
 * - Consumer reads head (acquire), THEN reads data
 * - This creates happens-before relationship
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>     // POSIX Threads API
#include <stdatomic.h>
#include <stdbool.h>
#include <unistd.h>
#include "benchmark.h"

#define QUEUE_SIZE 1024  // Must be power of 2
#define MASK (QUEUE_SIZE - 1)
#define NUM_MESSAGES 10000000

// Lock-free SPSC Queue
typedef struct {
    // The ring buffer
    int buffer[QUEUE_SIZE];

    // TODO: Add head and tail indices with proper cache alignment
    // These should be atomic_size_t with 64-byte cache line alignment
    //
    // CRITICAL: Use alignas(64) to prevent false sharing!
    // If head and tail share a cache line, every enqueue/dequeue causes
    // cache line ping-pong between cores = 2-3x slower
    //
    // Format:
    //   alignas(64) atomic_size_t head;  // Producer writes
    //   alignas(64) atomic_size_t tail;  // Consumer writes

    alignas(64) atomic_size_t head;  // FIXME: Remove this comment when done
    alignas(64) atomic_size_t tail;  // FIXME: Remove this comment when done
} spsc_queue_t;

void queue_init(spsc_queue_t *q) {
    // TODO: Initialize head and tail to 0
    atomic_store(&q->head, 0);  // FIXME: Remove this comment when done
    atomic_store(&q->tail, 0);  // FIXME: Remove this comment when done
}

// Producer enqueues a value
bool queue_enqueue(spsc_queue_t *q, int value) {
    size_t head = atomic_load_explicit(&q->head, memory_order_relaxed);
    size_t next_head = (head + 1) & MASK;

    // TODO: Load tail with acquire ordering
    // Why acquire? Pairs with consumer's release store to tail
    size_t tail = 0;  // FIXME: Use atomic_load_explicit with memory_order_acquire

    if (next_head == tail) {
        return false;  // Queue full
    }

    // Write data to buffer
    q->buffer[head] = value;

    // TODO: Update head with release ordering
    // Why release? Publishes the data write to consumer
    // FIXME: Use atomic_store_explicit with memory_order_release
    (void)next_head;  // Suppress warning
    return true;
}

// Consumer dequeues a value
bool queue_dequeue(spsc_queue_t *q, int *value) {
    size_t tail = atomic_load_explicit(&q->tail, memory_order_relaxed);

    // TODO: Load head with acquire ordering
    // Why acquire? Sees producer's data write
    size_t head = 0;  // FIXME: Use atomic_load_explicit with memory_order_acquire

    if (head == tail) {
        return false;  // Queue empty
    }

    // Read data from buffer
    *value = q->buffer[tail];

    // TODO: Update tail with release ordering
    // Why release? Frees the slot for producer
    // FIXME: Use atomic_store_explicit with memory_order_release
    (void)tail;  // Suppress warning
    return true;
}

// Test: Producer thread
void *producer(void *arg) {
    spsc_queue_t *q = (spsc_queue_t *)arg;

    for (int i = 0; i < NUM_MESSAGES; i++) {
        while (!queue_enqueue(q, i)) {
            // Queue full, spin (use architecture hint to be polite on CPU)
            CPU_PAUSE();  // x86: PAUSE, ARM: YIELD (see benchmark.h)
        }
    }

    return NULL;
}

// Test: Consumer thread
void *consumer(void *arg) {
    spsc_queue_t *q = (spsc_queue_t *)arg;
    int value;
    int received = 0;

    while (received < NUM_MESSAGES) {
        if (queue_dequeue(q, &value)) {
            if (value != received) {
                printf("ERROR: Expected %d, got %d\n", received, value);
                exit(1);
            }
            received++;
        } else {
            // Queue empty, spin (use architecture hint)
            CPU_PAUSE();
        }
    }

    return NULL;
}

int main() {
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  Exercise 07: Lock-Free SPSC Queue\n");
    printf("  Messages: %d, Queue size: %d\n", NUM_MESSAGES, QUEUE_SIZE);
    printf("═══════════════════════════════════════════════════════════\n\n");

    spsc_queue_t *queue = malloc(sizeof(spsc_queue_t));
    queue_init(queue);

    pthread_t prod, cons;

    printf("Testing lock-free queue...\n");
    TIME_BLOCK("SPSC queue throughput") {
        pthread_create(&cons, NULL, consumer, queue);
        pthread_create(&prod, NULL, producer, queue);

        pthread_join(prod, NULL);
        pthread_join(cons, NULL);
    }

    printf("✓ All messages received in order!\n\n");

    // Performance metrics
    double msg_per_sec = NUM_MESSAGES / (get_nanos() / 1e9);
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
