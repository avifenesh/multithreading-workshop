# Systems-Level Multithreading in C

**Deep dive into pthread internals, memory architecture, and hardware-level synchronization.**

## What You'll Learn

- **Thread mechanics:** clone() syscall, stack allocation, TLS, futex, scheduling
- **Synchronization internals:** Futex-based mutexes, spinlock implementations, barriers
- **Memory architecture:** Cache coherency (MESI), false sharing, cache line effects
- **Memory ordering:** Acquire/release semantics, compiler/CPU reordering, barriers
- **Performance analysis:** perf, ThreadSanitizer, assembly inspection
- **Hardware atomics:** x86 LOCK prefix, ARM LL/SC, CAS operations

## Prerequisites

```bash
sudo apt-get install build-essential linux-tools-generic valgrind
```

**Assumes:** Strong C, familiarity with threading concepts (you know what a mutex is, now learn how it works).

## Exercises

1. **01_atomics** - Memory ordering deep dive: seq_cst vs relaxed vs acquire-release, message passing patterns
2. **02_rwlock** - Reader-writer locks, scalable concurrent reads
3. **03_cache_effects** - False sharing, cache coherency (MESI), 64-byte cache lines (2-10x impact!)
4. **04_memory_ordering** - x86 TSO vs ARM weak ordering, acquire/release patterns, ThreadSanitizer essential
5. **05_spinlock_internals** - TAS → TTAS → TTAS+PAUSE → Exponential backoff, CPU PAUSE instruction
6. **06_barriers** - Phase synchronization, manual implementation, epoch pattern
7. **07_lockfree_queue** - Lock-free SPSC queue, cache alignment, acquire-release synchronization
8. **08_summary** - Summary capstone: compare mutex vs per-thread padded counters and an acquire/release SPSC path

## Quick Start

```bash
make all          # Build all exercises
make run-01       # Run exercise 01
```

## Analysis Tools

```bash
make asm-05                    # View assembly (see LOCK prefix)
make tsan-04                   # ThreadSanitizer race detection
make perf-03                   # Cache misses, coherency traffic
make objdump-05                # Disassemble binary
```

## Study Approach

**For experienced developers (Rust/Node.js background):**
1. Skim Exercise 00 (reference), skip Exercise 02 (you know RwLock)
2. **Core path:** 01 → 03 → 04 → 05 → 07 (4-5 hours)
3. Use analysis tools liberally: `make asm-XX`, `make tsan-XX`, `make perf-XX`
4. Read **SYSTEMS_GUIDE.md** for deeper theory
5. See **IMPROVEMENTS.md** for detailed enhancement guide

**Traditional path:**
1. Read **SYSTEMS_GUIDE.md** (theory: futex, cache coherency, memory models)
2. Work through exercises 01-07 with assembly/perf inspection
3. Reference **API.md** for pthread details

## Key Concepts

See also: SYSTEMS_GUIDE.md → Glossary for acronym and instruction definitions (TSO, LL/SC, LSE, PAUSE, DMB, POSIX/IEEE, etc.).

### Thread Lifecycle
- `clone()` syscall flags (CLONE_VM, CLONE_THREAD)
- Stack allocation via `mmap`, guard pages
- TLS and `%fs` register (x86)
- Context switch cost (~1-3µs)

### Synchronization
- **Mutex:** Futex fast path (atomic CAS) + slow path (syscall)
- **Spinlock:** Test-and-set, TTAS, exponential backoff, PAUSE
- **Atomics:** `LOCK` prefix (x86), LL/SC (ARM)
- **Condvars:** Spurious wakeups, predicate loops, futex queues

### Memory Architecture
- **Cache:** L1/L2/L3 latency (4/12/40 cycles), 64-byte lines
- **MESI:** Modified, Exclusive, Shared, Invalid states
- **False sharing:** Independent vars on same cache line

### Memory Ordering
- **Compiler reordering:** Optimizer rearranges code
- **CPU reordering:** Store buffers, load speculation
- **Barriers:** Compiler (`asm volatile`), hardware (`MFENCE`, `DMB`)
- **C11 orders:** relaxed, acquire, release, seq_cst

## Example Workflows

**Cache effects:**
```bash
./exercises/03_cache_effects/03_cache_effects
perf stat -e cache-misses,LLC-loads ./exercises/03_cache_effects/03_cache_effects
make asm-03
```

**Spinlock internals:**
```bash
./exercises/05_spinlock_internals/05_spinlock_internals
make asm-05  # See LOCK XCHG, PAUSE instructions
```

**Memory ordering:**
```bash
make tsan-04  # Detect broken synchronization
```

## Platform Notes

### macOS — Equivalent, Same, Different
- Equivalent
  - Build: `xcode-select --install` then `brew install llvm binutils`; run `make all`. Use Apple Clang by default (`gcc` is Clang on macOS).
  - TSan: `TSAN_CC=clang make tsan-04` (ThreadSanitizer supported on Apple Clang).
  - Disassembly: `llvm-objdump -d --x86-asm-syntax=intel -S <bin> | less`.
  - Debugging: `lldb <bin>` (instead of `gdb`).
- Same
  - All exercises build on Intel and Apple Silicon; pthreads + C11 atomics work unchanged.
  - Assembly generation via `clang -S -O2 -fverbose-asm` works; if `-masm=intel` is rejected, omit it.
- Different
  - `perf-*` targets are Linux-only. Use sampling instead: `xcrun xctrace record --template 'Time Profiler' --launch ./<bin> --output trace.trace` or quick `sample <pid> 5 -mayDie`.
  - `objdump-%` uses GNU objdump; prefer `llvm-objdump` on macOS (see Equivalent above).
  - Internals differ: no `futex`/`clone` on macOS; mutexes/condvars are built on Mach primitives (e.g., `ulock`), but concepts map 1:1.

### Windows — Equivalent, Same, Different
- Equivalent
  - Recommended: WSL2 + Ubuntu. Install deps (`sudo apt-get install build-essential linux-tools-generic valgrind`) and use `make`/`perf` as-is.
  - Native (MSYS2/MinGW): Install MSYS2, then in the UCRT64 shell: `pacman -S --needed base-devel mingw-w64-ucrt-x86_64-toolchain`. Build with `make all` (links `libwinpthread` via `-pthread`).
  - Disassembly: `llvm-objdump -d -S <bin>` or `objdump -d -S <bin>` from MSYS2 binutils.
- Same
  - Exercises compile with GCC/Clang in MSYS2 using pthreads and C11 atomics.
  - Spin/wait semantics and memory-ordering exercises apply unchanged conceptually.
- Different
  - `perf-*` not available natively; use Windows Performance Recorder/Analyzer (WPR/WPA) or Visual Studio Profiler. Prefer WSL2 for parity.
  - ThreadSanitizer is limited on native Windows toolchains; use Clang+WSL2 for TSan.
  - Under-the-hood uses Win32 primitives (e.g., SRWLOCK) rather than Linux `futex`.

#### Windows internals: SRWLOCK vs Linux futex
- Linux `pthread_mutex_t`/`pthread_cond_t` rely on `futex()` — user mode fast path, `FUTEX_WAIT/FUTEX_WAKE` slow path, optional priority inheritance (`FUTEX_LOCK_PI`).
- Windows `SRWLOCK`/`CRITICAL_SECTION` spin in user mode, then park on kernel waits (push locks or keyed events). Windows exposes the futex-like primitive as `WaitOnAddress`/`WakeByAddress*`, but high-level locks call it internally.
- Priority inheritance & robustness: POSIX has `PTHREAD_PRIO_INHERIT` and `PTHREAD_MUTEX_ROBUST`; Windows relies on priority boosting heuristics and `WAIT_ABANDONED` (for kernel `Mutex` objects only).
- Cross-process: POSIX allows `PTHREAD_PROCESS_SHARED` in shared memory; Windows requires named kernel objects (Mutex/Semaphore/Event) because `SRWLOCK`/`CRITICAL_SECTION` are process-local.

### Capstone — Summary Exercise
- See `exercises/08_summary` for a hands-on summary that combines atomics, memory ordering, cache effects, and synchronization. Build with `make 08_summary` and run with `make run-08`.

## References

- **SYSTEMS_GUIDE.md** - Core theory (futex, MESI, memory models)
- **API.md** - pthread API reference
- `man futex`, `man pthreads`
- [Intel SDM](https://software.intel.com/sdm) - Memory ordering, LOCK prefix
- [Drepper: What Every Programmer Should Know About Memory](https://people.freebsd.org/~lstewart/articles/cpumemory.pdf)
- [McKenney: Linux Kernel Perfbook](https://kernel.org/pub/linux/kernel/people/paulmck/perfbook/perfbook.html)
