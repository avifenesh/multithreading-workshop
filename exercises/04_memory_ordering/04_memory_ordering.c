/**
 * Exercise 4: Memory Ordering and CPU Memory Models
 *
 * ⚠️ CRITICAL x86 CAVEAT ⚠️
 * The "broken" version often WORKS on x86 due to strong memory model (TSO).
 * Use ThreadSanitizer to catch the bug: make tsan-04
 *
 * x86-64 Memory Model (TSO - Total Store Ordering):
 *   ✓ Loads are NOT reordered with loads
 *   ✓ Stores are NOT reordered with stores
 *   ✓ Stores are NOT reordered with earlier loads
 *   ✗ Loads MAY be reordered with earlier stores (the only hole!)
 *
 * ARM/PowerPC/RISC-V have WEAK ordering - reorders aggressively!
 *
 * LEARNING GOALS:
 * - Understand x86 TSO vs ARM weak ordering
 * - See compiler vs CPU reordering
 * - Master acquire-release synchronization
 * - Learn why volatile ≠ atomic
 */

#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>
#include <assert.h>

// Shared variables
atomic_int data = 0;
atomic_int flag = 0;

// Version 1: BROKEN - no synchronization
int data_broken = 0;
int flag_broken = 0;

void *producer_broken(void *arg) {
    (void)arg;
    data_broken = 42;  // Can be reordered!
    flag_broken = 1;   // Can be reordered!
    return NULL;
}

void *consumer_broken(void *arg) {
    (void)arg;
    while (flag_broken == 0) {
        // Busy wait - might never see flag=1 due to caching
    }
    // Might see data=0 even after flag=1 due to reordering
    printf("Broken version - data=%d (may be 0 or 42)\n", data_broken);
    return NULL;
}

// Version 2: seq_cst - strongest ordering
void *producer_seqcst(void *arg) {
    (void)arg;
    atomic_store_explicit(&data, 42, memory_order_seq_cst);
    atomic_store_explicit(&flag, 1, memory_order_seq_cst);
    return NULL;
}

void *consumer_seqcst(void *arg) {
    (void)arg;
    while (atomic_load_explicit(&flag, memory_order_seq_cst) == 0) {
        // Busy wait with seq_cst ordering
    }
    int value = atomic_load_explicit(&data, memory_order_seq_cst);
    printf("Seq_cst version - data=%d (always 42)\n", value);
    assert(value == 42);
    return NULL;
}

// Version 3: acquire-release - efficient synchronization
void *producer_acqrel(void *arg) {
    (void)arg;
    atomic_store_explicit(&data, 42, memory_order_relaxed);
    atomic_store_explicit(&flag, 1, memory_order_release); // Release barrier
    return NULL;
}

void *consumer_acqrel(void *arg) {
    (void)arg;
    while (atomic_load_explicit(&flag, memory_order_acquire) == 0) {
        // Acquire barrier on successful read
    }
    int value = atomic_load_explicit(&data, memory_order_relaxed);
    printf("Acquire-release version - data=%d (always 42)\n", value);
    assert(value == 42);
    return NULL;
}

void test_version(const char *name, void *(*producer)(void*), void *(*consumer)(void*)) {
    printf("\n=== %s ===\n", name);
    
    // Reset state
    data_broken = 0;
    flag_broken = 0;
    atomic_store(&data, 0);
    atomic_store(&flag, 0);
    
    pthread_t prod, cons;
    pthread_create(&cons, NULL, consumer, NULL);
    pthread_create(&prod, NULL, producer, NULL);
    
    pthread_join(prod, NULL);
    pthread_join(cons, NULL);
}

int main() {
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  Exercise 04: Memory Ordering & CPU Memory Models\n");
    printf("═══════════════════════════════════════════════════════════\n\n");

    printf("⚠️  IMPORTANT: x86 Memory Model Caveat\n");
    printf("╔═══════════════════════════════════════════════════════╗\n");
    printf("║  The 'broken' version will likely WORK on x86!       ║\n");
    printf("║  x86 has TSO (Total Store Order) - strong model      ║\n");
    printf("║  ARM/RISC-V would fail reliably                      ║\n");
    printf("║                                                        ║\n");
    printf("║  To catch the bug: make tsan-04                      ║\n");
    printf("╚═══════════════════════════════════════════════════════╝\n\n");

    printf("x86-64 Ordering Rules (TSO):\n");
    printf("  ✓ Store-Store: NO reordering (data=42, flag=1 stays ordered)\n");
    printf("  ✓ Load-Load:   NO reordering\n");
    printf("  ✓ Load-Store:  NO reordering\n");
    printf("  ✗ Store-Load:  MAY reorder (the weak point)\n\n");

    // WARNING: Broken version may appear to work due to timing
    printf("Testing BROKEN version (no atomics):\n");
    printf("  On x86: Usually works (luck + TSO)\n");
    printf("  On ARM: Would fail spectacularly\n");
    printf("  Compiler: Can still reorder if optimized!\n\n");
    for (int i = 0; i < 3; i++) {
        test_version("  Broken", producer_broken, consumer_broken);
    }

    printf("\nTesting seq_cst version:\n");
    test_version("  Seq_cst", producer_seqcst, consumer_seqcst);

    printf("\nTesting acquire-release version:\n");
    test_version("  Acquire-release", producer_acqrel, consumer_acqrel);

    printf("\n═══════════════════════════════════════════════════════════\n");
    printf("  KEY INSIGHTS:\n");
    printf("  • x86 TSO masks many bugs (false sense of security)\n");
    printf("  • ARM/RISC-V weak ordering exposes them immediately\n");
    printf("  • Compiler can reorder even on x86 (use -O2)\n");
    printf("  • seq_cst: Full fence, expensive (MFENCE on x86)\n");
    printf("  • acquire-release: Efficient one-way barriers\n");
    printf("\n");
    printf("  ANALYSIS:\n");
    printf("  make asm-04     - See fence instructions\n");
    printf("  make tsan-04    - Catch the race (ESSENTIAL!)\n");
    printf("  make objdump-04 - Full disassembly\n");
    printf("\n");
    printf("  ASSEMBLY ON x86:\n");
    printf("  • Broken:         Plain 'mov' instructions\n");
    printf("  • seq_cst store:  'mfence; mov' or 'lock mov'\n");
    printf("  • release store:  Plain 'mov' (x86 TSO is enough)\n");
    printf("  • acquire load:   Plain 'mov' (x86 TSO is enough)\n");
    printf("\n");
    printf("  ASSEMBLY ON ARM:\n");
    printf("  • release: 'stlr' (store-release) or 'str; dmb ish'\n");
    printf("  • acquire: 'ldar' (load-acquire) or 'dmb ish; ldr'\n");
    printf("═══════════════════════════════════════════════════════════\n");

    return 0;
}
