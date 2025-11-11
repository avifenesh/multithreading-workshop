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
#include "benchmark.h"
#include <pthread.h>
#include <stdalign.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define NUM_THREADS 8
#define INCREMENTS 2000000

#define QUEUE_SIZE 1024
#define MASK (QUEUE_SIZE - 1)
#define NUM_MESSAGES 1000000

static atomic_int start_flag = 0;

// ============================================================
// Variant A — Shared counter with synchronization
// ============================================================

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static long shared_counter = 0;
typedef struct
{
  atomic_bool locked;
} ttas_spinlock_t;

static void
ttas_spinlock_init (ttas_spinlock_t *lock)
{
  atomic_store_explicit (&lock->locked, false, memory_order_relaxed);
}

static void
ttas_lock (ttas_spinlock_t *lock)
{
  while (1)
    {
      if (!atomic_load_explicit (&lock->locked, memory_order_relaxed))
        {
          bool expected = false;
          if (atomic_compare_exchange_weak_explicit (
                  &lock->locked, &expected, true, memory_order_acquire,
                  memory_order_relaxed))
            break;
        }
      CPU_PAUSE ();
    }
}

static void
ttas_unlock (ttas_spinlock_t *lock)
{
  atomic_store_explicit (&lock->locked, false, memory_order_release);
}

static void *
variant_a_ttas_worker (void *arg)
{
  ttas_spinlock_t *lock = (ttas_spinlock_t *)arg;
  ttas_lock (lock);
  for (int i = 0; i < INCREMENTS; i++)
    {
      shared_counter++;
    }
  ttas_unlock (lock);
  return NULL;
}

static void
run_variant_a_ttas (void)
{
  pthread_t threads[NUM_THREADS];
  ttas_spinlock_t lock;
  ttas_spinlock_init (&lock);
  printf ("═══════════════════════════════════════════════════════════\n");
  printf ("Variant A (TTAS): Shared counter with TTAS spinlock\n");
  TIME_BLOCK ("Variant A (TTAS): synchronized counter")
  {
    shared_counter = 0;
    for (int i = 0; i < NUM_THREADS; i++)
      {
        pthread_create (&threads[i], NULL, variant_a_ttas_worker, &lock);
      }
    for (int i = 0; i < NUM_THREADS; i++)
      {
        pthread_join (threads[i], NULL);
      }
  }
  long expected = (long)NUM_THREADS * INCREMENTS;
  printf ("  Result: %ld (expected %ld) %s\n", shared_counter, expected,
          shared_counter == expected ? "✓" : "✗ INCORRECT");
  printf ("\n");
}

static void *
variant_a_worker (void *arg)
{
  (void)arg;
  while (atomic_load_explicit (&start_flag, memory_order_acquire) == 0)
    {
      CPU_PAUSE ();
    }

  for (int i = 0; i < INCREMENTS; i++)
    {
      pthread_mutex_lock (&mutex);
      shared_counter++;
      pthread_mutex_unlock (&mutex);
    }

  return NULL;
}

static void
run_variant_a (void)
{
  pthread_t threads[NUM_THREADS];

  pthread_mutex_init (&mutex, NULL);

  shared_counter = 0;
  atomic_store_explicit (&start_flag, 0, memory_order_relaxed);

  printf ("═══════════════════════════════════════════════════════════\n");
  printf ("Variant A: Shared counter with synchronization\n");

  TIME_BLOCK ("Variant A: synchronized counter")
  {
    for (int i = 0; i < NUM_THREADS; i++)
      {
        pthread_create (&threads[i], NULL, variant_a_worker, NULL);
      }
    atomic_store_explicit (&start_flag, 1, memory_order_release);

    for (int i = 0; i < NUM_THREADS; i++)
      {
        pthread_join (threads[i], NULL);
      }
  }

  long expected = (long)NUM_THREADS * INCREMENTS;
  printf ("  Result: %ld (expected %ld) %s\n", shared_counter, expected,
          shared_counter == expected ? "✓" : "✗ INCORRECT");

  pthread_mutex_destroy (&mutex);
  atomic_store_explicit (&start_flag, 0, memory_order_relaxed);

  printf ("\n");
}

// ============================================================
// Variant B — Per-thread counters (packed vs padded)
// ============================================================

typedef struct
{
  atomic_long counter;
} packed_counter_t;

typedef struct
{
  alignas (64) atomic_long counter;
} padded_counter_t;

static void* variant_b_worker_packed(void* arg) {
  packed_counter_t *counter = (packed_counter_t *)arg;

  while (atomic_load_explicit (&start_flag, memory_order_acquire) == 0)
    {
      CPU_PAUSE ();
    }

  for (int i = 0; i < INCREMENTS; i++)
    {
      atomic_fetch_add_explicit (&counter->counter, 1, memory_order_relaxed);
    }

    return NULL;
}

static void* variant_b_worker_padded(void* arg) {
  padded_counter_t *counter = (padded_counter_t *)arg;

  while (atomic_load_explicit (&start_flag, memory_order_acquire) == 0)
    {
      CPU_PAUSE ();
    }

  for (int i = 0; i < INCREMENTS; i++)
    {
      atomic_fetch_add_explicit (&counter->counter, 1, memory_order_relaxed);
    }

    return NULL;
}

static void run_variant_b(void) {
  pthread_t threads[NUM_THREADS];

  printf ("═══════════════════════════════════════════════════════════\n");
  printf ("Variant B: Per-thread counters (false sharing comparison)\n");

  printf ("  Size of packed_counter_t: %zu bytes\n",
          sizeof (packed_counter_t));
  printf ("  Size of padded_counter_t: %zu bytes\n",
          sizeof (padded_counter_t));

  atomic_store (&start_flag, 0);

  // ----- Packed (false sharing) -----
  packed_counter_t *packed_counters
      = malloc (NUM_THREADS * sizeof (packed_counter_t));
  for (int i = 0; i < NUM_THREADS; i++)
    {
      atomic_store (&packed_counters[i].counter, 0);
    }

    TIME_BLOCK("Variant B: packed (false sharing)") {
      for (int i = 0; i < NUM_THREADS; i++)
        {
          pthread_create (&threads[i], NULL, variant_b_worker_packed,
                          &packed_counters[i]);
        }
      atomic_store (&start_flag, 1);
      for (int i = 0; i < NUM_THREADS; i++)
        {
          pthread_join (threads[i], NULL);
        }
    }

    long packed_total = 0;
    for (int i = 0; i < NUM_THREADS; i++)
      {
        packed_total += atomic_load (&packed_counters[i].counter);
      }

    printf("  Packed total: %ld (expected %ld) %s\n",
           packed_total, (long)NUM_THREADS * INCREMENTS,
           packed_total == (long)NUM_THREADS * INCREMENTS ? "✓" : "✗");

    free (packed_counters);
    printf ("\n");

    // ----- Padded (no false sharing) -----
    padded_counter_t *padded_counters
        = malloc (NUM_THREADS * sizeof (padded_counter_t));
    for (int i = 0; i < NUM_THREADS; i++)
      {
        atomic_store (&padded_counters[i].counter, 0);
      }

    atomic_store(&start_flag, 0);

    TIME_BLOCK("Variant B: padded (cache-line isolated)") {
      for (int i = 0; i < NUM_THREADS; i++)
        {
          pthread_create (&threads[i], NULL, variant_b_worker_padded,
                          &padded_counters[i]);
        }
      atomic_store (&start_flag, 1);
      for (int i = 0; i < NUM_THREADS; i++)
        {
          pthread_join (threads[i], NULL);
        }
    }
    long padded_total = 0;

    for (int i = 0; i < NUM_THREADS; i++)
      {
        padded_total += atomic_load (&padded_counters[i].counter);
      }

    printf("  Padded total: %ld (expected %ld) %s\n",
           padded_total, (long)NUM_THREADS * INCREMENTS,
           padded_total == (long)NUM_THREADS * INCREMENTS ? "✓" : "✗");

    free (padded_counters);
    atomic_store (&start_flag, 0);
    printf("\n");
}

// ============================================================
// Variant C — SPSC lock-free queue
// ============================================================

typedef struct
{
  int buffer[QUEUE_SIZE];
  alignas (64) atomic_size_t head;
  alignas (64) atomic_size_t tail;
} spsc_queue_t;

static int
spsc_init (spsc_queue_t *q)
{
  atomic_store (&q->head, 0);
  atomic_store (&q->tail, 0);
  return 0;
}

static bool
spsc_enqueue (spsc_queue_t *q, int value)
{
  size_t head = atomic_load_explicit (&q->head, memory_order_relaxed);
  size_t next_head = (head + 1) & MASK;
  size_t tail = atomic_load_explicit (&q->tail, memory_order_acquire);
  if (next_head == tail)
    {
      return false;
    }
  q->buffer[head] = value;
  atomic_store_explicit (&q->head, next_head, memory_order_release);
  return true;
}

static bool
spsc_dequeue (spsc_queue_t *q, int *value)
{
  int tail = atomic_load_explicit (&q->tail, memory_order_relaxed);
  int head = atomic_load_explicit (&q->head, memory_order_acquire);
  if (head == tail)
    return false;
  *value = q->buffer[tail];
  int next_tail = (tail + 1) & MASK;
  atomic_store_explicit (&q->tail, next_tail, memory_order_release);
  return true;
}

static void *
producer (void *arg)
{
  spsc_queue_t *q = (spsc_queue_t *)arg;
  while (atomic_load_explicit (&start_flag, memory_order_acquire) == 0)
    {
      CPU_PAUSE ();
    }
  for (int i = 0; i < NUM_MESSAGES; i++)
    {
      while (!spsc_enqueue (q, i))
        {
          CPU_PAUSE ();
        }
    }

  return NULL;
}

static void* consumer(void* arg) {
  spsc_queue_t *q = (spsc_queue_t *)arg;

  while (atomic_load_explicit (&start_flag, memory_order_acquire) == 0)
    {
      CPU_PAUSE ();
    }

  long sum = 0;

  for (int i = 0; i < NUM_MESSAGES; i++)
    {
      int value;
      while (!spsc_dequeue (q, &value))
        {
          CPU_PAUSE ();
        }
      sum += value;
    }

  return (void *)(intptr_t)sum;
}

static void run_variant_c(void) {
  pthread_t prod, cons;

  spsc_queue_t queue;
  spsc_init (&queue);

  atomic_store (&start_flag, 0);

  long expected = ((long)NUM_MESSAGES * (NUM_MESSAGES - 1)) / 2;

  void *consumer_result;

  printf ("═══════════════════════════════════════════════════════════\n");
  printf ("Variant C: SPSC lock-free queue\n");

  TIME_BLOCK ("Variant C: message passing")
  {
    pthread_create (&cons, NULL, consumer, &queue);
    pthread_create (&prod, NULL, producer, &queue);

    atomic_store (&start_flag, 1);

    pthread_join (prod, NULL);
    pthread_join (cons, &consumer_result);
  }
  long observed = (long)(intptr_t)consumer_result;

  printf ("  Sum: %ld (expected %ld) %s\n", observed, expected,
          observed == expected ? "✓" : "✗");
  atomic_store (&start_flag, 0);

  printf ("\n");
}

int
main (void)
{
  run_variant_a ();
  run_variant_a_ttas ();
  run_variant_b ();
  run_variant_c ();
  return 0;
}
