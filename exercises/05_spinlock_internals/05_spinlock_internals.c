/**
 * Exercise 5: Spinlock Internals
 * 
 * Implement spinlocks from scratch and understand their internals:
 * - Test-and-set (TAS) spinlock
 * - Test-and-test-and-set (TTAS) spinlock  
 * - Compare with pthread_spinlock_t
 * 
 * Learn why TTAS is better for contention.
 */


#include <stdio.h>
#include <pthread.h>     // POSIX Threads API (Portable Operating System Interface)
#include <stdatomic.h>   // C11 atomics (lock-free ops + memory orders)
#include <stdbool.h>
#include "benchmark.h"

#define NUM_THREADS 4
#define ITERATIONS 100000

#if defined(__x86_64__) || defined(__i386__)
// Spin-wait hint: x86 PAUSE reduces power and improves HT fairness
static inline void cpu_relax() {
    __builtin_ia32_pause();
}
#elif defined(__aarch64__) || defined(__arm__)
#include <unistd.h>
// Spin-wait hint: AArch64 YIELD tells CPU we are busy-waiting
static inline void cpu_relax() {
    asm volatile("yield" ::: "memory");
}
#else
static inline void cpu_relax() {
    volatile int dummy = 0;
    (void)dummy;
}
#endif

// Simple TAS spinlock - always does atomic exchange
typedef struct {
    atomic_flag lock;
} tas_spinlock_t;

void tas_lock(tas_spinlock_t *lock) {
    while (atomic_flag_test_and_set_explicit(&lock->lock, memory_order_acquire)) {
        // Test-and-set: atomic exchange sets flag; busy-wait while held
      }
}

void
tas_unlock (tas_spinlock_t *lock)
{
  atomic_flag_clear_explicit (&lock->lock, memory_order_release);
}

// TTAS spinlock - test locally first
typedef struct
{
  atomic_bool locked;
} ttas_spinlock_t;

void
ttas_lock (ttas_spinlock_t *lock)
{
  while (1)
    {
      // First, check locally (cheap read, can be cached)
      if (!atomic_load_explicit (&lock->locked, memory_order_relaxed))
        {
          // Then try to acquire (expensive atomic exchange)
          bool expected = false;
          // Weak CAS: may spuriously fail; updates 'expected' on failure; safe
          // in loop
          if (atomic_compare_exchange_weak_explicit (
                  &lock->locked, &expected, true,
                  memory_order_acquire, // on success: acquire the lock
                  memory_order_relaxed))
            {        // on failure: no ordering needed
              break; // Got the lock
            }
        }
      cpu_relax ();
    }
}

// TODO: TTAS with PAUSE - implement this version
// This should be identical to ttas_lock but add pause instruction
typedef struct {
    atomic_bool locked;
} ttas_pause_spinlock_t;

void ttas_pause_lock(ttas_pause_spinlock_t *lock) {
    // TTAS with PAUSE instruction
    while (1) {
        if (!atomic_load_explicit(&lock->locked, memory_order_relaxed)) {
            bool expected = false;
            // Weak CAS: may fail spuriously; loop retries
            if (atomic_compare_exchange_weak_explicit(
                    &lock->locked, &expected, true,
                    memory_order_acquire, memory_order_relaxed)) {
                break;
            }
        }
        cpu_relax();  // Platform-independent pause
    }
    //
    // What PAUSE does on x86:
    //   1. Hints CPU that this is a spin-wait loop
    //   2. Reduces power consumption (delays pipeline for ~140 cycles)
    //   3. Avoids memory order mis-speculation penalty
    //   4. Improves hyper-threading (lets other thread use execution units)
}

void ttas_pause_unlock(ttas_pause_spinlock_t *lock) {
    atomic_store_explicit(&lock->locked, false, memory_order_release);
}

// ADVANCED: Exponential backoff spinlock
typedef struct {
    atomic_bool locked;
} backoff_spinlock_t;

#define BACKOFF_MIN 4
#define BACKOFF_MAX 1024

void backoff_lock(backoff_spinlock_t *lock) {
    int backoff = BACKOFF_MIN;
    while (1) {
        if (!atomic_load_explicit(&lock->locked, memory_order_relaxed)) {
            bool expected = false;
            // Weak CAS + backoff: reduce contention on shared cache line
            if (atomic_compare_exchange_weak_explicit(
                    &lock->locked, &expected, true,
                    memory_order_acquire, memory_order_relaxed)) {
                break;
            }
        }

        // Exponential backoff: pause for longer each time
        for (int i = 0; i < backoff; i++) {
            cpu_relax();
        }

        backoff = (backoff * 2 > BACKOFF_MAX) ? BACKOFF_MAX : backoff * 2;
    }
}

void backoff_unlock(backoff_spinlock_t *lock) {
    atomic_store_explicit(&lock->locked, false, memory_order_release);
}

void ttas_unlock(ttas_spinlock_t *lock) {
    atomic_store_explicit(&lock->locked, false, memory_order_release);
}

// Shared counter
long shared_counter = 0;

// Test functions
void *tas_worker(void *arg) {
    tas_spinlock_t *lock = (tas_spinlock_t *)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        tas_lock(lock);
        shared_counter++;
        tas_unlock(lock);
    }
    return NULL;
}

void *ttas_worker(void *arg) {
    ttas_spinlock_t *lock = (ttas_spinlock_t *)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        ttas_lock(lock);
        shared_counter++;
        ttas_unlock(lock);
    }
    return NULL;
}

void *pthread_spin_worker(void *arg) {
    pthread_spinlock_t *lock = (pthread_spinlock_t *)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        pthread_spin_lock(lock);
        shared_counter++;
        pthread_spin_unlock(lock);
    }
    return NULL;
}

void *ttas_pause_worker(void *arg) {
    ttas_pause_spinlock_t *lock = (ttas_pause_spinlock_t *)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        ttas_pause_lock(lock);
        shared_counter++;
        ttas_pause_unlock(lock);
    }
    return NULL;
}

void *backoff_worker(void *arg) {
    backoff_spinlock_t *lock = (backoff_spinlock_t *)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        backoff_lock(lock);
        shared_counter++;
        backoff_unlock(lock);
    }
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];

    printf("═══════════════════════════════════════════════════════════\n");
    printf("  Exercise 05: Spinlock Internals & CPU Instructions\n");
    printf("  Threads: %d, Iterations: %d\n", NUM_THREADS, ITERATIONS);
    printf("═══════════════════════════════════════════════════════════\n\n");

    // // Test 1: TAS spinlock
    // printf("1. TAS (Test-And-Set) Spinlock\n");
    // tas_spinlock_t tas_lock = { ATOMIC_FLAG_INIT };
    // shared_counter = 0;

    // TIME_BLOCK("   TAS spinlock") {
    //     for (long i = 0; i < NUM_THREADS; i++) {
    //         pthread_create(&threads[i], NULL, tas_worker, &tas_lock);
    //     }
    //     for (int i = 0; i < NUM_THREADS; i++) {
    //         pthread_join(threads[i], NULL);
    //     }
    // }
    // printf("   Counter: %ld ✓\n", shared_counter);
    // printf("   How: Busy-waits with atomic test_and_set (LOCK XCHG)\n");
    // printf("   Problem: Every spin does expensive atomic operation\n\n");

    // Test 2: TTAS spinlock
    // printf("2. TTAS (Test-Test-And-Set) Spinlock\n");
    // ttas_spinlock_t ttas_lock = { false };
    // shared_counter = 0;

    // TIME_BLOCK("   TTAS spinlock") {
    //     for (long i = 0; i < NUM_THREADS; i++) {
    //         pthread_create(&threads[i], NULL, ttas_worker, &ttas_lock);
    //     }
    //     for (int i = 0; i < NUM_THREADS; i++) {
    //         pthread_join(threads[i], NULL);
    //     }
    // }
    // printf("   Counter: %ld ✓\n", shared_counter);
    // printf("   How: Read locally (cheap), then test_and_set (expensive)\n");
    // printf("   Benefit: Reduces cache coherency traffic\n\n");

    // Test 3: TODO - TTAS with PAUSE
    // printf("3. TODO: TTAS + PAUSE Instruction\n");
    // printf("   Implement ttas_pause_lock() to see the improvement!\n");

    // ttas_pause_spinlock_t ttas_pause_lock = { false };
    // shared_counter = 0;

    // TIME_BLOCK("   TTAS+PAUSE") {
    //     for (long i = 0; i < NUM_THREADS; i++) {
    //         pthread_create(&threads[i], NULL, ttas_pause_worker, &ttas_pause_lock);
    //     }
    //     for (int i = 0; i < NUM_THREADS; i++) {
    //         pthread_join(threads[i], NULL);
    //     }
    // }
    // printf("   Counter: %ld ✓\n", shared_counter);
    // printf("   How: Adds x86 PAUSE instruction in spin loop\n");
    // printf("   Assembly: Look for 'pause' in: make asm-05\n");
    // printf("   Benefits:\n");
    // printf("     • Reduces power consumption (~140 cycle delay)\n");
    // printf("     • Avoids memory order mis-speculation penalty\n");
    // printf("     • Improves hyper-threading performance\n\n");


    // Test 4: Exponential backoff (already implemented)
    // printf("4. ADVANCED: Exponential Backoff Spinlock\n");
    // backoff_spinlock_t backoff_lock = { false };
    // shared_counter = 0;

    // TIME_BLOCK("   Backoff spinlock") {
    //     for (long i = 0; i < NUM_THREADS; i++) {
    //         pthread_create(&threads[i], NULL, backoff_worker, &backoff_lock);
    //     }
    //     for (int i = 0; i < NUM_THREADS; i++) {
    //         pthread_join(threads[i], NULL);
    //     }
    // }
    // printf("   Counter: %ld ✓\n", shared_counter);
    // printf("   How: Doubles wait time on each failed acquire (4 → 8 → 16 → ... → 1024)\n");
    // printf("   Benefit: Adapts to contention level\n\n");

    // Test 5: pthread_spinlock_t (for comparison)
    printf("5. pthread_spinlock_t (libc implementation)\n");
    pthread_spinlock_t pthread_lock;
    pthread_spin_init(&pthread_lock, PTHREAD_PROCESS_PRIVATE);
    shared_counter = 0;

    TIME_BLOCK("   pthread_spinlock") {
        for (long i = 0; i < NUM_THREADS; i++) {
            pthread_create(&threads[i], NULL, pthread_spin_worker, (void *)&pthread_lock);
        }
        for (int i = 0; i < NUM_THREADS; i++) {
            pthread_join(threads[i], NULL);
        }
    }
    // printf("   Counter: %ld ✓\n", shared_counter);
    // printf("   Note: Likely uses TTAS+PAUSE internally\n\n");

    // pthread_spin_destroy(&pthread_lock);

    // printf("═══════════════════════════════════════════════════════════\n");
    // printf("  KEY INSIGHTS:\n");
    // printf("  • TAS: Every spin = atomic op = cache coherency traffic\n");
    // printf("  • TTAS: Read locally first (cached), atomic only when unlocked\n");
    // printf("  • PAUSE: x86 hint for spin loops (~140 cycle delay)\n");
    // printf("  • Backoff: Adaptive delay reduces contention\n");
    // printf("\n");
    // printf("  ANALYSIS COMMANDS:\n");
    // printf("  make asm-05     - See 'lock cmpxchg' and 'pause' instructions\n");
    // printf("  make perf-05    - Measure cache-misses, LLC-loads\n");
    // printf("  make objdump-05 - Full disassembly with addresses\n");
    // printf("\n");
    // printf("  ASSEMBLY LOOKUPS:\n");
    // printf("  • TAS:     'lock xchg' or 'lock bts' (bit test-and-set)\n");
    // printf("  • TTAS:    'cmp' (test), then 'lock cmpxchg' (CAS)\n");
    // printf("  • PAUSE:   'pause' instruction in loop\n");
    // printf("  • Backoff: Multiple 'pause' instructions in sequence\n");
    // printf("═══════════════════════════════════════════════════════════\n");

    return 0;
}
