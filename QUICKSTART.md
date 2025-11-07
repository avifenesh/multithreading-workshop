# Quick Start

**30-minute path to hands-on systems understanding.**

## 1. Read Theory (15 min)

```bash
less SYSTEMS_GUIDE.md
```

Focus on:
- **Thread Creation & Scheduling** - clone(), futex, TLS
- **Memory Architecture** - MESI, cache lines, false sharing
- **Memory Models** - Acquire/release, happens-before

## 2. Build & Run (5 min)

```bash
make all
./exercises/03_cache_effects/03_cache_effects
```

Observe: False sharing causes 2-10× slowdown.

## 3. Inspect (10 min)

### See Cache Misses
```bash
perf stat -e cache-misses,cache-references ./exercises/03_cache_effects/03_cache_effects
```

### View Assembly
```bash
make asm-05
less exercises/05_spinlock_internals/05_spinlock_internals.s
# Look for: lock xchg, pause
```

### Detect Races
```bash
make tsan-04
# ThreadSanitizer shows broken synchronization
```

## Key Commands

```bash
# Build
make all                    # All exercises
make 03_cache_effects       # Single exercise

# Run
make run-03                 # Execute exercise 03

# Analysis
make asm-05                 # Assembly (LOCK prefix, PAUSE)
make tsan-04                # Race detection
make perf-03                # Cache counters
make objdump-05             # Disassembly

# Profile
perf record -g ./exercises/05_spinlock_internals/05_spinlock_internals
perf report
```

## Experiments

**1. Padding impact**
Edit `exercises/03_cache_effects/03_cache_effects.c`:
```c
// Remove alignas(64) from padded version
struct padded_counter {
    atomic_int value;  // No alignment
};
```
Rebuild, run. Performance matches false-sharing version.

**2. Memory order**
Edit `exercises/04_memory_ordering/04_memory_ordering.c`:
```c
// Change to relaxed (wrong!)
atomic_store_explicit(&ready, 1, memory_order_relaxed);
```
Run with `make tsan-04` - catches the bug.

**3. Spinlock variants**
```bash
./exercises/05_spinlock_internals/05_spinlock_internals
```
Compare TAS vs TTAS performance. View assembly to see why.

## What to Look For

**In assembly:**
- `lock xchg` / `lock cmpxchg` - Atomic operations
- `pause` - Spin loop hint (x86)
- `mfence` - Memory barrier (seq_cst)

**In perf:**
- High cache-misses - False sharing or excessive atomics
- High context-switches - Contention, mutex blocking

**In ThreadSanitizer:**
- Data race reports - Missing synchronization
- Lock order violations - Potential deadlock

## Next Steps

Work through exercises 01-06 in order:
1. Atomics - Memory orders
2. RWLock - Concurrent readers
3. Cache effects - False sharing
4. Memory ordering - Acquire/release
5. Spinlock - TAS vs TTAS
6. Barriers - Phase sync

Read `SYSTEMS_GUIDE.md` sections as needed for deeper understanding.
