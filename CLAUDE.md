# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

This is a systems-level multithreading workshop in C focused on understanding pthread internals, memory architecture, and hardware-level synchronization. The codebase contains 7 exercises (00-06) demonstrating thread mechanics, synchronization primitives, cache coherency, and memory ordering at the hardware level.

## Build System

The project uses a Makefile-based build system with standardized targets:

### Basic Commands
```bash
make all                    # Build all exercises (00-06)
make solutions              # Build all solution files
make <exercise_name>        # Build specific exercise (e.g., make 01_atomics)
make clean                  # Remove all binaries and generated files
```

### Running Exercises
```bash
make run-00                 # Run exercise 00_quick_review
make run-01                 # Run exercise 01_atomics
# Pattern: make run-<number> for exercises 00-06
```

### Analysis Tools
```bash
make asm-<number>           # Generate Intel syntax assembly (e.g., make asm-05)
                           # Output: exercises/<name>/<name>.s
make tsan-<number>          # Build with ThreadSanitizer and run (e.g., make tsan-04)
                           # Detects data races and synchronization issues
make perf-<number>          # Run with perf counters (cache-misses, LLC-loads, etc.)
make objdump-<number>       # Disassemble binary with Intel syntax
```

## Code Structure

### Exercise Layout
- **exercises/NN_name/** - Each exercise directory contains:
  - `NN_name.c` - Main exercise file (student version with TODOs)
  - `solution.c` - Reference solution
  - `README.md` - Exercise-specific instructions

### Shared Components
- **include/benchmark.h** - Timing and benchmarking utilities used across exercises
- **SYSTEMS_GUIDE.md** - Deep technical reference on futex, MESI protocol, memory models, and implementation details
- **API.md** - pthread and C11 atomics API quick reference
- **QUICKSTART.md** - 30-minute getting started guide

### Exercise Progression
1. **00_quick_review** - Daily warmup, core threading concepts (complete demo, not a challenge)
2. **01_atomics** - Memory ordering deep dive (relaxed vs seq_cst vs acquire-release), message passing patterns
3. **02_rwlock** - Reader-writer locks for scalable concurrent reads
4. **03_cache_effects** - False sharing, MESI cache coherency, 64-byte cache line alignment
5. **04_memory_ordering** - x86 TSO vs ARM weak ordering, acquire/release patterns, ThreadSanitizer essential
6. **05_spinlock_internals** - TAS → TTAS → TTAS+PAUSE → Exponential backoff, CPU PAUSE instruction
7. **06_barriers** - Phase synchronization, manual barrier implementation, epoch patterns
8. **07_lockfree_queue** - Lock-free SPSC queue, cache alignment, acquire-release synchronization (NEW!)

## Compilation Flags

The Makefile uses these compilation settings:
- **Standard:** `-Wall -Wextra -pthread -O2` for optimized builds
- **ThreadSanitizer:** `-g -O1 -fsanitize=thread` for race detection (lower optimization for better diagnostics)
- **Assembly:** `-S -fverbose-asm -masm=intel` for readable assembly output

## Key Technical Concepts

### Thread Implementation
- Threads created via clone() syscall (CLONE_VM, CLONE_THREAD flags)
- Each thread gets 8MB stack (via mmap), TLS via %fs register
- Context switches cost ~1-3µs

### Synchronization Primitives
- **Mutex:** Futex-based (fast path: atomic CAS, slow path: syscall)
- **Spinlock:** LOCK XCHG on x86, busy-waits with PAUSE instruction
- **Atomics:** LOCK prefix on x86 (~50-100 cycles contended), LL/SC on ARM
- **Barriers:** Phase synchronization for coordinating thread groups

### Memory Architecture
- Cache line size: 64 bytes on x86_64
- MESI protocol: Modified, Exclusive, Shared, Invalid states
- False sharing: Independent variables on same cache line cause coherency traffic
- Cache latencies: L1 ~4 cycles, L2 ~12 cycles, L3 ~40 cycles, RAM ~200+ cycles

### Memory Ordering
- x86_64 has strong ordering (sequentially consistent for normal loads/stores)
- ARM/PowerPC require explicit barriers
- C11 memory orders: `relaxed` (no sync), `acquire`/`release` (one-way barriers), `seq_cst` (full ordering)

## Development Workflow

### Adding New Exercises
1. Create directory: `exercises/NN_name/`
2. Add `NN_name.c` with TODOs for students
3. Add `solution.c` with working implementation
4. Update `EXERCISES` variable in Makefile
5. Add entry to README.md's exercise list

### Testing Changes
When modifying synchronization code:
1. Build with optimizations: `make <exercise>`
2. Run ThreadSanitizer: `make tsan-<number>` to detect races
3. Check assembly: `make asm-<number>` to verify LOCK instructions and memory barriers
4. Profile with perf: `make perf-<number>` to measure cache effects

### Common Inspection Patterns
- Look for `lock cmpxchg` or `lock xchg` in assembly for atomic operations
- Use `perf stat -e cache-misses,cache-references` to detect false sharing
- ThreadSanitizer output shows exact line numbers of race conditions
- High context-switches in perf indicates mutex contention

## Important Notes

- Exercises 01, 05, 07 contain TODOs for hands-on implementation challenges
- Exercises 00, 03, 04, 06 are complete demonstrations with educational commentary
- Solutions demonstrate correct implementation patterns
- SYSTEMS_GUIDE.md is the authoritative reference for understanding how primitives work under the hood
- Performance characteristics vary by architecture (x86 vs ARM) and CPU generation
- Always use `while` loops (not `if`) with condition variables due to spurious wakeups
- Spinlocks only appropriate for critical sections < 100ns to avoid CPU waste
