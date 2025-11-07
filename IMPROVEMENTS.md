# Multithreading Workshop - Improvements Summary

## Overview
This workshop has been enhanced for **fast-learning senior engineers** with strong backgrounds in high-level languages (Rust/Node.js) who need **systems-level C threading knowledge** for database/OS work.

## Philosophy Changes
- **From:** Basic tutorials with fill-in-the-blanks
- **To:** Hands-on challenges with assembly inspection, performance analysis, and bug hunting
- **Focus:** Understanding what happens at the CPU/memory/OS level, not just API usage

---

## Enhanced Exercises

### ✅ Exercise 01: Atomic Operations & Memory Ordering
**Before:** Trivial - just add `atomic_fetch_add`
**After:** Deep dive into memory ordering

**New Content:**
1. **Broken version** - See race condition manifest, lose increments
2. **seq_cst version** - Full barriers, slow but correct
3. **relaxed version** - Fast path, understand when safe
4. **Message passing** - Demonstrate acquire-release pattern

**Learning Outcomes:**
- See non-atomic operations in assembly (`mov/inc/mov` interleaving)
- Measure seq_cst vs relaxed performance difference (1.5-2x)
- Understand when relaxed is safe (independent operations)
- Master acquire-release for synchronization

**Commands:**
```bash
make run-01        # See races manifest
make asm-01        # See 'lock add' instruction
make tsan-01       # Catch races with ThreadSanitizer
```

---

### ✅ Exercise 04: Memory Ordering & CPU Memory Models
**Before:** Broken version "works" on x86, no explanation why
**After:** Explicit x86 TSO caveat with ARM comparison

**Critical Addition:**
```
⚠️  x86 Memory Model (TSO - Total Store Order)
  ✓ Store-Store: NO reordering
  ✓ Load-Load:   NO reordering
  ✓ Load-Store:  NO reordering
  ✗ Store-Load:  MAY reorder (only weakness!)

ARM/RISC-V: WEAK ordering - reorders aggressively!
```

**New Understanding:**
- x86 TSO masks many bugs (false security)
- ARM would expose them immediately
- Compiler can still reorder on x86
- ThreadSanitizer is essential: `make tsan-04`

**Assembly Differences:**
- x86 seq_cst: `mfence; mov` or `lock mov`
- x86 release: Plain `mov` (TSO sufficient)
- ARM release: `stlr` or `str; dmb ish`

---

### ✅ Exercise 05: Spinlock Internals & CPU Instructions
**Before:** Basic TAS vs TTAS comparison
**After:** Full spinlock optimization hierarchy

**New Variants:**
1. **TAS (Test-And-Set)** - Baseline, atomic every spin
2. **TTAS (Test-Test-And-Set)** - Read locally first
3. **TTAS + PAUSE** - x86 PAUSE instruction (NEW!)
4. **Exponential Backoff** - Adaptive contention handling (NEW!)

**CPU PAUSE Instruction:**
```c
__builtin_ia32_pause();  // On x86
// Assembly: 'pause'
// Effects:
//   - Hints CPU: spin-wait loop
//   - Delays pipeline ~140 cycles
//   - Reduces power consumption
//   - Improves hyper-threading
```

**Performance Hierarchy:**
```
TAS < TTAS < TTAS+PAUSE < Exponential Backoff
(worst)                              (best)
```

**Commands:**
```bash
make run-05        # See performance differences
make asm-05        # Look for 'pause' instruction
make perf-05       # Measure cache-misses
```

---

### 🆕 Exercise 07: Lock-Free SPSC Queue (NEW!)
**Practical Application:** Database pipelines, message passing, audio/video processing

**Challenge:** Implement lock-free ring buffer with correct memory ordering

**Key Concepts:**
1. **Single Producer/Consumer** - No CAS needed!
2. **Cache Line Alignment** - `alignas(64)` prevents false sharing (2-3x speedup)
3. **Acquire-Release** - Creates happens-before without locks

**Memory Ordering Flow:**
```
Producer:
  1. Write data[head]
  2. Store head (RELEASE) ← Publishes data

Consumer:
  3. Load head (ACQUIRE)  ← Sees data
  4. Read data[tail]
  5. Store tail (RELEASE) ← Frees slot

Producer:
  6. Load tail (ACQUIRE)  ← Sees freed slot
```

**Why No CAS?**
- Producer writes head, consumer only reads it
- Consumer writes tail, producer only reads it
- Separation of duties = no contention!

**False Sharing Prevention:**
```c
alignas(64) atomic_size_t head;  // Own cache line
alignas(64) atomic_size_t tail;  // Own cache line
// Without alignas: 2-3x slower!
```

**Commands:**
```bash
make run-07     # 10M messages, measure throughput
make asm-07     # See plain mov (no LOCK prefix!)
make tsan-07    # Verify no races
```

**Experiment:** Remove `alignas(64)` and see 2-3x slowdown from false sharing.

---

## Architecture Understanding

### x86-64 Strong Memory Model (TSO)
```
✓ Allowed:     Denied:
  Load-Load    Store-Load reordering
  Load-Store
  Store-Store

Implication: Many bugs hidden on x86, exposed on ARM!
```

### Assembly Inspection Patterns

**Atomics:**
```asm
# seq_cst:
mfence                 # Full barrier
mov    %eax, (%rdx)    # Store

# OR

lock mov %eax, (%rdx)  # Atomic store with barrier

# relaxed on x86:
mov    %eax, (%rdx)    # Plain store (TSO sufficient)
```

**Spinlocks:**
```asm
# TAS:
lock xchg %al, (%rdx)  # Atomic exchange every spin

# TTAS:
.L_spin:
    mov    (%rdx), %al # Read (cached)
    test   %al, %al
    jne    .L_spin
    lock cmpxchg ...   # CAS only when looks free

# TTAS+PAUSE:
.L_spin:
    pause             # ← The magic instruction
    mov    (%rdx), %al
    ...
```

---

## Analysis Tools Reference

### ThreadSanitizer (Essential!)
```bash
make tsan-01    # Detect race in Exercise 01
make tsan-04    # Catch broken memory ordering
make tsan-07    # Verify lock-free queue correctness
```

**Why Essential:**
- x86 TSO hides bugs
- Compiler can reorder
- TSan finds them reliably

### Assembly Inspection
```bash
make asm-01     # See 'lock add' for atomics
make asm-04     # See 'mfence' for seq_cst
make asm-05     # See 'pause' instruction
make asm-07     # See plain moves (no lock!) in SPSC
```

**What to Look For:**
- `lock` prefix: Atomic operations
- `mfence`/`lfence`/`sfence`: Memory barriers
- `pause`: Spin-wait hint
- Plain `mov`: No synchronization

### Performance Analysis
```bash
make perf-03    # Cache effects, false sharing
make perf-05    # Spinlock cache coherency traffic
make perf-07    # SPSC queue throughput
```

**Key Metrics:**
- `cache-misses`: False sharing indicator
- `LLC-loads`: Last-level cache access
- `context-switches`: Mutex contention

---

## Recommended Learning Path

### For Your Background (Rust/Node.js → C Systems Programming):

**Quick Review (30 min):**
1. Skim Exercise 00 - just a reference
2. Skip Exercises 02 (too basic)

**Deep Dive (3-4 hours):**
1. **Exercise 01** - Memory ordering fundamentals
   - Complete all TODOs
   - Run `make asm-01` and find the `lock add`
   - Run `make tsan-01` to see race detection

2. **Exercise 04** - x86 vs ARM memory models
   - Understand why broken version works on x86
   - Compare TSO table with ARM weak ordering

3. **Exercise 05** - CPU instructions & spinlocks
   - Implement TTAS+PAUSE
   - Run `make asm-05` and find `pause` instruction
   - Compare TAS/TTAS/TTAS+PAUSE/Backoff performance

4. **Exercise 07** - Lock-free SPSC queue
   - This is DIRECTLY applicable to your DB work
   - Understand acquire-release synchronization
   - Experiment with removing `alignas(64)`

**Exercises 03 & 06:** Solid but optional for your goals

**Read SYSTEMS_GUIDE.md:** After hands-on work, read for deep theory

---

## Key Takeaways for Database/OS Work

### 1. Memory Ordering is Subtle
- x86 TSO hides bugs that ARM exposes
- Always use ThreadSanitizer
- Acquire-release is the sweet spot

### 2. Cache Line Awareness
- 64-byte alignment prevents false sharing
- 2-3x performance impact
- Critical for lock-free structures

### 3. Lock-Free Patterns
- SPSC: No CAS needed, just acquire-release
- MPMC: Need CAS and ABA solutions
- Huge performance wins when done right

### 4. CPU Instructions Matter
- PAUSE: Essential for spinlocks
- LOCK prefix: ~50-100 cycles
- Plain atomics on x86: Often just MOV

### 5. Assembly Literacy
- Verify compiler output
- Check for LOCK prefix
- Find memory barriers
- Essential for performance work

---

## What's Still Missing (For Even Deeper Learning)

### Not Covered Yet:
1. **ABA Problem** - Lock-free stack hazard
2. **Memory Reclamation** - Hazard pointers, epoch-based RCU
3. **MPMC Queues** - Multi-producer multi-consumer
4. **Seqlocks** - Read-optimized locking
5. **Work-Stealing Queues** - Thread pool pattern

These are advanced topics for a follow-up or when you encounter them in your database work.

---

## Quick Command Reference

```bash
# Build & Run
make all           # Build all exercises
make run-01        # Run specific exercise
make solutions     # Build solution files

# Analysis
make asm-01        # Generate assembly
make tsan-04       # ThreadSanitizer
make perf-05       # Performance counters
make objdump-03    # Full disassembly

# What to Look For
make asm-01 | grep "lock"     # Find atomic operations
make asm-05 | grep "pause"    # Find spin hints
make perf-03 2>&1 | grep cache-misses  # False sharing
```

---

## Success Criteria

You'll know you've mastered this material when you can:

1. ✅ Explain why `memory_order_relaxed` is safe for counters
2. ✅ Identify the bug in a lock-free algorithm from assembly
3. ✅ Calculate expected false sharing from struct layout
4. ✅ Choose correct memory ordering for message passing
5. ✅ Understand why your code works on x86 but fails on ARM
6. ✅ Implement SPSC queue from scratch with correct ordering
7. ✅ Find PAUSE instruction in disassembly
8. ✅ Use ThreadSanitizer to catch subtle races

---

## Next Steps

1. **Complete the exercises** in order: 01 → 04 → 05 → 07
2. **Read SYSTEMS_GUIDE.md** for deeper theory
3. **Apply to your database work** - look for opportunities to use SPSC queues
4. **Experiment** - break things, use TSan, inspect assembly
5. **Move fast** - you have the background, trust your intuition and verify with tools

**Remember:** The goal isn't to memorize APIs, it's to understand what happens at the CPU/cache/memory level. Every time you write threading code, think about the assembly and the cache.

Good luck! 🚀
