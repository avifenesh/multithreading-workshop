# Systems-Level Guide to Multithreading in C

This guide goes beyond API usage to explain **how multithreading actually works** at the OS, libc, and hardware level.

---

## Table of Contents
1. [Thread Creation & Scheduling](#thread-creation--scheduling)
2. [Memory Architecture & Cache Coherency](#memory-architecture--cache-coherency)
3. [Synchronization Primitives Implementation](#synchronization-primitives-implementation)
4. [Memory Models & Ordering](#memory-models--ordering)
5. [Performance Analysis](#performance-analysis)
6. [API Internals & Cross-Platform Equivalents](#api-internals--cross-platform-equivalents)
7. [Glossary: Acronyms & Instructions](#glossary-acronyms--instructions)

---

## Thread Creation & Scheduling

### What Happens When You Call `pthread_create()`?

```c
pthread_create(&tid, NULL, func, arg);
```

**Under the hood (glibc on Linux):**

1. **Stack Allocation**
   - Default: 8MB per thread (check with `ulimit -s`)
   - Allocated via `mmap(PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_STACK)`
   - Guard page at bottom to catch overflow (unmapped page triggers SIGSEGV)
   
2. **Thread-Local Storage (TLS) setup**
   - Each thread gets its own state like `errno` and an identity used by `pthread_self()`.
   - x86_64 Linux: the TLS base (thread pointer) lives in the `%fs` segment register; the compiler generates `%fs:offset` loads/stores to access thread-local data.
   - AArch64: the TLS base lives in the `tpidr_el0` system register; TLS variables are addressed relative to this thread pointer.
   - See “TLS, errno, and pthread_self” below and the Glossary for definitions of `%fs`, `tpidr_el0`, TLS, and TCB/TEB.
   
3. **Clone Syscall** (Linux-specific)
   ```c
   clone(CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | 
         CLONE_THREAD | CLONE_SYSVSEM | CLONE_SETTLS | CLONE_PARENT_SETTID | 
         CLONE_CHILD_CLEARTID, stack_pointer, &tid, tls, &tid);
   ```
   - `CLONE_VM`: Share virtual memory (address space)
   - `CLONE_THREAD`: Share thread group (PID)
   - Creates kernel task_struct, adds to scheduler runqueue

4. **Kernel Scheduling**
   - Linux uses CFS (Completely Fair Scheduler)
   - Each thread is a schedulable entity (task_struct)
   - Timeslice typically 4-100ms depending on load
   - Context switch cost: ~1-3 µs (save/restore registers, TLB flush)

**View threads in action:**
```bash
# See all threads of a process
ps -T -p <pid>

# Check stack location
cat /proc/<pid>/maps | grep stack

# See context switches
perf stat -e context-switches ./program
```

### TLS, errno, and pthread_self

What is TLS?
- Thread-Local Storage (TLS) is per-thread data. It backs `errno`, `pthread_self()` bookkeeping, and user-declared `__thread`/`thread_local` variables.
- The C macro `errno` expands to a function or TLS lookup returning the current thread’s error code; each thread sees its own value.

Thread identity in pthreads:
- `pthread_self()` returns a `pthread_t` (opaque handle). Do not assume it is the OS TID; compare with `pthread_equal(a, b)`.
- Linux: the OS thread ID (TID) is accessible via `gettid()` (glibc 2.30+) or `syscall(SYS_gettid)`. `pthread_t` is typically a pointer to a thread control block (implementation-defined).
- macOS: use `pthread_mach_thread_np(pthread_self())` to get the Mach thread port (numeric ID).
- Windows: use `GetCurrentThreadId()` for a numeric thread ID; the OS also provides a thread handle via `GetCurrentThread()`.

How CPUs access TLS under the hood:
- x86_64 Linux: `%fs` holds the thread pointer (set via `arch_prctl(ARCH_SET_FS)` by the runtime). TLS variables compile to memory references like `%fs:0xNNN`. The TCB (Thread Control Block) lives at a fixed offset from this base.
- AArch64: `tpidr_el0` holds the thread pointer. Loads/stores to TLS become PC-relative sequences that ultimately use this register.
- Windows: the thread pointer refers to the TEB (Thread Environment Block). On x86 it’s accessed via `%fs`, on x64 via `%gs`; higher-level APIs are `TlsAlloc/TlsGetValue` and `__declspec(thread)`.

Resources:
- `man pthread_self`, `man pthread_equal`, `man 2 gettid`, `man arch_prctl`
- glibc TLS ABI overview; AArch64 `tpidr_el0` in Arm ARM; Windows TLS (`TlsAlloc`) and TEB background

---

## Memory Architecture & Cache Coherency

### CPU Cache Hierarchy

```
Core 0          Core 1          Core 2          Core 3
L1d (32KB)      L1d (32KB)      L1d (32KB)      L1d (32KB)
L1i (32KB)      L1i (32KB)      L1i (32KB)      L1i (32KB)
   ↓               ↓               ↓               ↓
L2 (256KB)      L2 (256KB)      L2 (256KB)      L2 (256KB)
   └───────────────┴───────────────┴───────────────┘
                        ↓
                   L3 (shared, 8MB)
                        ↓
                   Main Memory (DDR4, ~60ns latency)
```

**Key facts:**
- L1 hit: ~4 cycles (1ns @ 4GHz)
- L2 hit: ~12 cycles (3ns)
- L3 hit: ~40 cycles (10ns)
- RAM access: ~200+ cycles (60ns)
- **Cache line size: 64 bytes** (on x86_64)

### MESI Protocol (Cache Coherency)

When multiple cores cache the same memory:

| State | Meaning | Can Read? | Can Write? |
|-------|---------|-----------|------------|
| **M**odified | Only I have it, dirty | ✓ | ✓ |
| **E**xclusive | Only I have it, clean | ✓ | ✓ (becomes M) |
| **S**hared | Others may have it | ✓ | ✗ (must invalidate) |
| **I**nvalid | Don't have it | ✗ | ✗ |

**Example: Two threads increment a counter**

```c
int counter = 0;  // Starts in Shared state

// Thread on Core 0
counter++;  // 1. Read (cache miss), 2. Invalidate Core 1's cache, 3. Write (M state)

// Thread on Core 1  
counter++;  // 1. Read (cache miss! Core 0 must flush), 2. Invalidate Core 0, 3. Write
```

Each modification causes cache line ping-pong (expensive!).

**Inspect with `perf`:**
```bash
perf stat -e cache-references,cache-misses,LLC-loads,LLC-load-misses ./program
```

### False Sharing

**Problem:** Two independent variables on same cache line

```c
struct {
    int thread0_counter;  // Byte 0-3
    int thread1_counter;  // Byte 4-7  ← Same 64-byte cache line!
} shared;
```

Thread 0 writes `thread0_counter` → **entire 64-byte line** invalidated on Thread 1's cache.

**Solution: Padding**
```c
struct {
    alignas(64) int thread0_counter;  // Force to own cache line
    alignas(64) int thread1_counter;  // Force to own cache line
};
```

**Demonstrate:**
- See `advanced/08_cache_effects` exercise

---

## Synchronization Primitives Implementation

### Mutex: Futex-Based Implementation

**Fast path (uncontended):**
```c
pthread_mutex_lock(&mutex) {
    if (atomic_cmpxchg(&mutex->lock, 0, 1) == 0)  // Try lock
        return 0;  // Success! (~10ns)
    
    // Slow path...
    futex_wait(&mutex->lock, 1);  // Syscall, sleep (~1µs)
}
```

**What's a futex?**
- **F**ast **u**serspace mu**tex**
- Syscall: `futex(FUTEX_WAIT, addr, expected_val)` - sleep if `*addr == expected_val`
- Syscall: `futex(FUTEX_WAKE, addr, num_threads)` - wake waiters
- Kernel maintains wait queues (hash table indexed by address)

**Why not pure spinlock?**
- Spinlock wastes CPU: `while (locked) { /* burn cycles */ }`
- Mutex sleeps: kernel deschedules thread until `pthread_mutex_unlock()` wakes it

**Assembly inspection:**
```bash
objdump -d -M intel program | grep -A 20 pthread_mutex_lock
# Look for: LOCK CMPXCHG instruction (atomic compare-and-swap)
```

### Spinlock: When to Use

**Implementation:**
```c
typedef struct {
    atomic_flag locked = ATOMIC_FLAG_INIT;
} spinlock_t;

void spin_lock(spinlock_t *lock) {
    while (atomic_flag_test_and_set(&lock->locked))  // LOCK XCHG instruction
        __builtin_ia32_pause();  // PAUSE instruction (reduces power, improves HT)
}
```

**Use when:**
- Critical section < 100ns
- Holder won't be preempted (real-time priority, or disable preemption)
- Lower latency than futex syscall

**Don't use when:**
- Long critical section (wastes CPU)
- Low-priority thread holds lock (priority inversion)

### Atomics: How `atomic_fetch_add` Works

**C code:**
```c
atomic_fetch_add(&counter, 1);
```

**x86_64 assembly:**
```asm
lock incl (%rax)    # LOCK prefix makes instruction atomic
```

**What LOCK does:**
1. Asserts LOCK# signal on bus
2. Exclusive cache line ownership (MESI protocol)
3. Prevents other cores from accessing that cache line
4. ~50-100 cycles for contended atomic

**ARM64 uses load-linked/store-conditional (LL/SC):**
```asm
.retry:
    ldaxr w1, [x0]      # Load-acquire exclusive
    add   w1, w1, #1
    stlxr w2, w1, [x0]  # Store-release exclusive
    cbnz  w2, .retry    # Retry if someone else modified
```

### Condition Variables: The Tricky Part

**Why this pattern is required:**
```c
pthread_mutex_lock(&mutex);
while (!condition) {  // MUST be while, not if!
    pthread_cond_wait(&cond, &mutex);  // Atomically unlock & sleep
}
// condition is true, mutex is locked
pthread_mutex_unlock(&mutex);
```

**What `pthread_cond_wait` does:**
1. Atomically unlock `mutex` and add thread to `cond` waitqueue
2. Sleep (futex syscall)
3. When woken (via `signal` or `broadcast`), reacquire `mutex` before returning
4. **Spurious wakeups possible** (kernel may wake you without signal)

**Implementation detail:**
- Uses two futexes internally (sequence numbers for ABA problem)
- Broadcast wakes all waiters (thundering herd) → they serialize on mutex reacquisition

---

## Memory Models & Ordering

### The Problem: Compiler & CPU Reordering

**Code:**
```c
int data = 0;
int ready = 0;

// Thread 1
data = 42;
ready = 1;

// Thread 2
while (!ready) { }
printf("%d\n", data);  // Could print 0!
```

**Why?**
1. **Compiler reordering:** Optimizer may swap `data = 42` and `ready = 1`
2. **CPU reordering:** Store buffer may commit writes out-of-order
3. **Cache coherency delays:** `data` write may not be visible when `ready` is

### Memory Barriers

**Compiler barrier (prevents compiler reordering):**
```c
asm volatile("" ::: "memory");  // GCC/Clang
```

**Hardware barrier (prevents CPU reordering):**
```c
__sync_synchronize();  // Full memory barrier (MFENCE on x86, DMB on ARM)
```

**x86_64 is sequentially consistent for loads/stores** (but not for non-temporal stores, or across LOCK instructions).

**ARM/PowerPC are weakly ordered** (require explicit barriers).

### C11 Memory Orders

```c
typedef enum {
    memory_order_relaxed,  // No synchronization, just atomicity
    memory_order_consume,  // Load-consume (rarely used correctly)
    memory_order_acquire,  // Prevents later ops from moving before
    memory_order_release,  // Prevents earlier ops from moving after
    memory_order_acq_rel,  // Both acquire and release
    memory_order_seq_cst   // Sequential consistency (default)
} memory_order;
```

**Example: Producer-Consumer Flag**
```c
atomic_int ready = 0;
int data;

// Producer
data = 42;
atomic_store_explicit(&ready, 1, memory_order_release);  
// ↑ Ensures data=42 happens-before ready=1

// Consumer
while (!atomic_load_explicit(&ready, memory_order_acquire)) { }
// ↑ Ensures ready==1 happens-before reading data
printf("%d\n", data);  // Guaranteed to see 42
```

**Release-Acquire Guarantee:**
- All stores before `release` are visible to thread that does `acquire`
- No fence needed on x86 (natural ordering), but needed on ARM

**Relaxed (no synchronization):**
```c
atomic_fetch_add_explicit(&stats_counter, 1, memory_order_relaxed);
// Fine for statistics where exact order doesn't matter
```

**Assembly comparison (x86_64):**
```bash
gcc -S -O2 program.c
# seq_cst: LOCK CMPXCHG or MFENCE
# acquire/release: plain MOV (x86 has strong ordering)
# relaxed: plain MOV
```

---

## Performance Analysis

### Tools

**1. `perf` (Linux performance counters)**
```bash
# See cache misses
perf stat -e cache-misses,cache-references ./program

# Profile where time is spent
perf record -g ./program
perf report

# See context switches
perf stat -e context-switches,cpu-migrations ./program
```

**2. ThreadSanitizer (data race detector)**
```bash
gcc -fsanitize=thread -g -O1 program.c -o program
./program
# Reports races with stack traces
```

**3. Valgrind Helgrind**
```bash
valgrind --tool=helgrind ./program
# Slower but catches some races TSan misses
```

**4. `gdb` thread debugging**
```bash
gdb ./program
(gdb) info threads
(gdb) thread apply all bt  # Backtrace all threads
(gdb) set scheduler-locking on  # Only current thread runs
```

### Measuring Overhead

**Context switch cost:**
```bash
perf stat -e context-switches,cycles ./thread_create_benchmark
# cycles / context_switches = cycles per switch
```

**Lock overhead:**
```c
// Measure:
clock_gettime(CLOCK_MONOTONIC, &start);
for (int i = 0; i < 1000000; i++) {
    pthread_mutex_lock(&mutex);
    pthread_mutex_unlock(&mutex);
}
clock_gettime(CLOCK_MONOTONIC, &end);
// Uncontended: ~10-20ns per lock/unlock
// Contended: ~1-3µs (futex syscall)
```

**Atomic overhead:**
```c
atomic_fetch_add(&counter, 1);  // ~10-50ns depending on contention
```

### Optimization Strategies

1. **Reduce sharing:** Thread-local accumulation, then merge
2. **Batch operations:** Amortize synchronization cost
3. **Lock-free algorithms:** Atomics + careful ordering
4. **Reader-writer locks:** If reads >> writes
5. **Padding:** Avoid false sharing
6. **Spinlocks for µs-scale critical sections**
7. **Profile first:** Don't optimize without data

---

## API Internals & Cross-Platform Equivalents

This section maps each item in API.md to what happens under the hood, and outlines equivalents on ARM, Windows, and macOS. Use it alongside the deeper explanations above.

### Thread Management

APIs: `pthread_create`, `pthread_join`, `pthread_exit`, `pthread_self`, `pthread_detach`

Under the hood (Linux/glibc):
- `pthread_create` allocates a stack (`mmap` + guard page), sets up TLS, then uses `clone(2)` with flags like `CLONE_VM|CLONE_THREAD|CLONE_SETTLS` to create a kernel task sharing the process address space. The new thread starts at `start_routine(arg)` and calls `pthread_exit` on return.
- `pthread_join` waits for target thread termination; implemented via futex-like wait on an internal state and reaping the TCB.
- `pthread_detach` marks the thread as unjoinable; resources are released automatically on exit.
- `pthread_self` returns the thread ID from TLS.

ARM differences:
- Same POSIX semantics. TLS base is held in a dedicated register (AArch64: `tpidr_el0`). ABI and TLS access sequences differ from x86, but behavior is identical at the C level.

Windows equivalents:
- Create: `CreateThread` (or `_beginthreadex` to integrate with the C runtime).
- Join: `WaitForSingleObject(handle, INFINITE)` then `CloseHandle(handle)`.
- Exit: `ExitThread`, or return from thread start function.
- Self: `GetCurrentThreadId` (ID) or `GetCurrentThread` (pseudo-handle).
- Detach: no direct equivalent; close the thread handle to drop your reference.

macOS equivalents:
- Uses POSIX `pthread_*` APIs (supported). Under the hood, pthreads are backed by Mach threads. `pthread_mach_thread_np` exposes the Mach thread port when needed.

Resources:
- man pages: `man pthread_create`, `man clone`, `man pthread_join`; clone(2): https://man7.org/linux/man-pages/man2/clone.2.html
- Windows: CreateThread: https://learn.microsoft.com/windows/win32/api/processthreadsapi/nf-processthreadsapi-createthread, _beginthreadex: https://learn.microsoft.com/cpp/c-runtime-library/reference/beginthread-beginthreadex, WaitForSingleObject: https://learn.microsoft.com/windows/win32/api/synchapi/nf-synchapi-waitforsingleobject
- Apple: Threading Programming Guide: https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/Multithreading/Introduction/Introduction.html, pthread overview: https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/pthread.3.html

### Mutex

APIs: `pthread_mutex_init/lock/trylock/unlock/destroy`, `PTHREAD_MUTEX_INITIALIZER`

Under the hood (Linux/glibc):
- Fast path uses an atomic compare-and-exchange on a small state field in userspace.
- Contended path sleeps with `futex(FUTEX_WAIT, ...)` and wakes with `futex(FUTEX_WAKE, ...)`. This avoids spinning and hands scheduling to the kernel.

x86 vs ARM:
- x86: atomic RMW use `LOCK`-prefixed instructions and benefit from the `PAUSE` hint in spin loops.
- AArch64: atomic RMW compiled either to LL/SC (`LDAXR`/`STLXR`) or LSE atomics (e.g., `LDADD`) when supported; use `yield` or `wfe/sev` hints in spin loops.

Windows equivalents:
- Prefer `CRITICAL_SECTION` for a mutex-like primitive (user-mode fast path, kernel wait on contention). APIs: `InitializeCriticalSection`, `EnterCriticalSection`, `LeaveCriticalSection`, `DeleteCriticalSection`.
- Heavier kernel mutex: `CreateMutex`/`WaitForSingleObject`/`ReleaseMutex` (cross-process capable but slower).

macOS equivalents:
- POSIX `pthread_mutex_*` available. For non-recursive fast mutual exclusion, Apple recommends `os_unfair_lock` (sleeps the waiter; replaces deprecated `OSSpinLock`).

Resources:
- man pages: `man pthread_mutex_lock`, futex(2): https://man7.org/linux/man-pages/man2/futex.2.html
- Windows: Critical Section: https://learn.microsoft.com/windows/win32/sync/critical-section-objects
- Apple: os_unfair_lock: https://developer.apple.com/documentation/os/1646466-os_unfair_lock

### Condition Variables

APIs: `pthread_cond_init/wait/timedwait/signal/broadcast/destroy`, `PTHREAD_COND_INITIALIZER`

Under the hood (Linux/glibc):
- `pthread_cond_wait` atomically releases the associated mutex and blocks the thread on an internal sequence counter using futex; wakeups re-acquire the mutex before returning. Spurious wakeups can occur; always use a while loop predicate.

ARM notes:
- Same POSIX semantics. The atomic sequence counters and futex-like sleeps/wakes are independent of ISA.

Windows equivalents:
- `CONDITION_VARIABLE` with `SleepConditionVariableCS` or `SleepConditionVariableSRW`, paired with `CRITICAL_SECTION` or `SRWLOCK` respectively. Spurious wakeups are also possible—use a loop.

macOS equivalents:
- POSIX `pthread_cond_*` available. Under the hood uses Darwin kernel turnstiles and `ulock` mechanisms to block/wake threads efficiently.

Resources:
- man pages: `man pthread_cond_wait`: https://man7.org/linux/man-pages/man3/pthread_cond_wait.3p.html
- Windows: Condition Variables: https://learn.microsoft.com/windows/win32/sync/condition-variables, WaitOnAddress (futex-like): https://learn.microsoft.com/windows/win32/sync/waitonaddress
- Apple: pthread condvars: https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/pthread_cond_wait.3.html

### Read-Write Locks

APIs: `pthread_rwlock_init/rdlock/wrlock/unlock/destroy`, `PTHREAD_RWLOCK_INITIALIZER`

Under the hood (Linux/glibc):
- Combines fast-path atomics to track reader counts with futex-based blocking for writers/readers when contention arises. Writers typically wait until reader count drops to zero.

ARM notes:
- Same semantics; compiled atomics map to LL/SC or LSE on AArch64.

Windows equivalents:
- `SRWLOCK` with `AcquireSRWLockShared/Exclusive`, `ReleaseSRWLockShared/Exclusive`. Lightweight and entirely in user mode when uncontended.

macOS equivalents:
- POSIX `pthread_rwlock_*` available.

Resources:
- man pages: `man pthread_rwlock_rdlock`: https://man7.org/linux/man-pages/man3/pthread_rwlock_rdlock.3p.html
- Windows: SRWLOCK: https://learn.microsoft.com/windows/win32/sync/slim-reader-writer--srw--locks

### Spinlocks

APIs: `pthread_spin_init/lock/trylock/unlock/destroy`

Under the hood:
- Typically implemented with an `atomic_flag` or exchange on a small state. In tight loops, use CPU hint instructions to reduce power and memory-ordering penalties.

x86 vs ARM hints:
- x86: `PAUSE` in spin loops (e.g., `__builtin_ia32_pause()`), reduces contention on Hyper-Threading.
- AArch64: `YIELD` or `WFE/SEV` (e.g., `__builtin_arm_yield()` or inline `yield; wfe;`).

Windows/macOS guidance:
- Windows: prefer `CRITICAL_SECTION` or `SRWLOCK` over ad-hoc spinlocks in user mode; if spinning, use `YieldProcessor()` in the loop.
- macOS: `OSSpinLock` is deprecated due to priority inversion; prefer `os_unfair_lock` or `pthread_mutex_t`.

Resources:
- Intel: Optimization reference manual (spin-wait hints): https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
- ARM: Barrier and WFE/YIELD usage: https://developer.arm.com/documentation/102336/0100/Software-optimizations/Contention-and-synchronization
- Apple: OSSpinLock deprecation and alternatives: https://developer.apple.com/library/archive/releasenotes/General/APIDiffsMacOSX10_12/Objective-C/Foundation.html (see os_unfair_lock)

### Barriers

APIs: `pthread_barrier_init/wait/destroy`

Under the hood (Linux/glibc):
- Internally tracks arrival count; last arriving thread wakes the group using a condvar/futex pair and flips a generation counter to avoid ABA races.

Windows equivalents:
- `SYNCHRONIZATION_BARRIER`: `InitializeSynchronizationBarrier`, `EnterSynchronizationBarrier`, `DeleteSynchronizationBarrier`.

macOS equivalents:
- POSIX `pthread_barrier_*` may not be available on older macOS; portable alternatives: implement with `pthread_mutex_t` + `pthread_cond_t`, or use Grand Central Dispatch (`dispatch_group_notify`, `dispatch_barrier_async`).

Resources:
- man pages: `man pthread_barrier_wait`: https://man7.org/linux/man-pages/man3/pthread_barrier_wait.3.html
- Windows: Synchronization Barriers: https://learn.microsoft.com/windows/win32/sync/synchronization-barriers
- Apple: GCD (Dispatch) docs: https://developer.apple.com/documentation/dispatch

### C11 Atomics

APIs: `atomic_store(_explicit)`, `atomic_load(_explicit)`, `atomic_fetch_add/sub(_explicit)`, `atomic_compare_exchange_*`, memory orders.

Under the hood:
- Compilers lower atomic RMW to hardware primitives: x86 uses `LOCK`-prefixed instructions; AArch64 uses LL/SC (Load-Linked/Store-Conditional via `LDAXR`/`STLXR`) or LSE (Large System Extensions) atomics (e.g., `LDADD`, `CAS`) when available. Acquire/Release map to appropriate barriers (`MFENCE` on x86 when needed; `DMB ish` on ARM).

Weak vs strong CAS:
- `atomic_compare_exchange_weak[_explicit]` may fail spuriously; use it inside a retry loop. On AArch64 it maps naturally to a single LL/SC attempt; on x86, weak/strong often compile the same.
- `atomic_compare_exchange_strong[_explicit]` does not allow spurious failure; compilers may loop internally to emulate strength on LL/SC architectures.
- Failure memory order must not include release semantics; common pairings are `acquire` (success) + `relaxed` (failure) for acquiring a flag, or `acq_rel` (success) + `acquire` (failure) for read-modify-write updates.

Windows equivalents:
- `Interlocked*` APIs: `InterlockedIncrement/Decrement`, `InterlockedExchange`, `InterlockedCompareExchange`, plus `MemoryBarrier` and acquire/release variants (e.g., `InterlockedCompareExchangeAcquire`). C11 `<stdatomic.h>` is also available with MSVC/Clang.

macOS equivalents:
- Fully supports C11 atomics via Clang. Legacy `OSAtomic*` APIs are deprecated in favor of C11 `<stdatomic.h>`.

Resources:
- ISO C11 Atomics (overview): https://en.cppreference.com/w/c/atomic
- Intel SDM, Vol. 3: https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
- ARM Architecture Reference Manual (Armv8-A): https://developer.arm.com/documentation/ddi0487/latest

### Debugging & Tools (from API.md)

ThreadSanitizer:
- Linux/macOS: use Clang/GCC `-fsanitize=thread`. Great for data race detection.
- Windows: TSan support in clang-cl is evolving; alternatives include Visual Studio Concurrency tools and Intel Inspector.

Helgrind (Valgrind):
- Linux: `valgrind --tool=helgrind`. macOS support is limited on recent versions; prefer TSan or Instruments.

GDB thread debugging:
- Linux: `gdb` with `info threads`, `thread apply all bt`.
- macOS: prefer `lldb` with analogous commands.
- Windows: Visual Studio debugger and WinDbg; for sampling/profiling, use Windows Performance Recorder/Analyzer (ETW).

Performance tooling parallels:
- Linux `perf` ≈ Windows ETW/WPR/WPA ≈ macOS Instruments/DTrace. Each can show context switches, CPU usage, and cache behavior (to varying depths).

Resources:
- TSan: https://clang.llvm.org/docs/ThreadSanitizer.html
- Valgrind Helgrind: https://valgrind.org/docs/manual/hg-manual.html
- GDB: https://sourceware.org/gdb/current/onlinedocs/gdb/, LLDB: https://llvm.org/documentation/
- Windows: ETW: https://learn.microsoft.com/windows/win32/etw/event-tracing-portal, WPR: https://learn.microsoft.com/windows-hardware/test/wpt/windows-performance-recorder, WPA: https://learn.microsoft.com/windows-hardware/test/wpt/windows-performance-analyzer, Visual Studio Diagnostics: https://learn.microsoft.com/visualstudio/profiling/profiling-feature-tour
- macOS: Instruments: https://developer.apple.com/documentation/xcode/instruments, DTrace: https://illumos.org/books/dtrace/ (background), `sample` tool: https://keith.github.io/xcode-man-pages/sample.1.html

---

## Glossary: Acronyms & Instructions

Common terms and instructions expanded once with short meanings.

- LL/SC (Load-Linked / Store-Conditional): A two-instruction atomic update scheme used on architectures like ARM. LL reads a value and “links” the cache line; SC writes back only if no conflicting write occurred since the link. SC returns success/failure, so code retries in a loop. Maps well to `atomic_compare_exchange_weak` in loops.
- CAS (Compare-And-Swap or Compare-And-Exchange): Atomically compare memory with an expected value and, if equal, swap in a new value. In C11 this is `atomic_compare_exchange_*` (compare-and-exchange) and returns success/failure while updating `expected` on failure.
- LSE (Large System Extensions, AArch64): Armv8.1-A extension adding single-instruction atomics like `CAS`, `LDADD`, `SWP`, which avoid LL/SC retry loops in hardware. Improves scalability on large systems.
- TAS / TTAS (Test-And-Set / Test-Test-And-Set): Simple spinlock strategies. TAS always does an atomic exchange; TTAS first reads (cheap), then exchanges only if observed unlocked, reducing coherency traffic under contention.
- TLS (Thread-Local Storage): Per-thread storage for data like `errno` and `pthread_self` identity. Accessed via `%fs` on x86_64 and `tpidr_el0` on AArch64.
- errno: A per-thread integer error code set by many libc/syscalls on failure; implemented as a TLS variable so each thread has its own `errno`.
- pthread_t: Opaque POSIX thread handle returned by `pthread_self()`. Not necessarily a numeric OS thread ID; compare using `pthread_equal`.
- TID (Thread ID): The kernel’s numeric thread identifier (Linux: `gettid()`, Windows: `GetCurrentThreadId`, macOS: Mach thread port via `pthread_mach_thread_np`).
- TCB (Thread Control Block): Runtime structure holding per-thread metadata and pointers (including the TLS base) used by libc.
- TEB (Thread Environment Block): Windows per-thread data structure pointed to by `%fs` (x86) or `%gs` (x64); source of `GetLastError` and TLS slots.
- Thread pointer (TP): Architecture-specific register/base that points to the current thread’s TLS/TCB (x86_64: `%fs` base; AArch64: `tpidr_el0`).
- Segment register (x86): Special registers (`%fs`, `%gs`) whose base can be set to point at per-thread data; used to implement TLS addressing in user mode.
- CFS (Completely Fair Scheduler): Linux’s default scheduler that tries to allocate CPU time fairly among runnable tasks.
- LLC (Last-Level Cache): The largest cache shared among cores on a CPU package (often L3). Tools often report “LLC-load-misses”.
- TSO (Total Store Order): x86 memory model property that largely preserves program order for loads/stores except store→load reordering, making some races “appear to work”.
- SRWLOCK (Slim Reader/Writer Lock): Windows lightweight reader–writer lock with user-mode fast path.
- WaitOnAddress: Windows futex-like primitive for blocking on a memory location change (`WakeByAddress*` wakes waiters).
- ulock (Darwin user locks): macOS/iOS kernel support behind pthreads for blocking on memory locations (turnstiles/ulock).
- ABA problem: A compare-and-exchange can succeed even if a value changed A→B→A in between checks; mitigated with version counters or hazard pointers.
- Thundering herd: Many waiters woken at once (e.g., broadcast) then contend for a resource, causing serialization.
- HT (Hyper-Threading): Intel’s simultaneous multithreading (SMT) implementation where two logical threads share one physical core’s execution resources; PAUSE helps fairness between siblings.
- TLB (Translation Lookaside Buffer): CPU cache of virtual→physical address translations; context switches may flush or invalidate portions, impacting performance.
- POSIX (Portable Operating System Interface): IEEE family of standards that defines a common OS API on Unix-like systems (files, processes, signals, pthreads, etc.). Linux and macOS implement most of POSIX; Windows is not POSIX but offers analogous APIs.
- IEEE (Institute of Electrical and Electronics Engineers): Standards body behind POSIX (IEEE 1003) and many computing/communications standards.
- ETW (Event Tracing for Windows): High-performance event logging/profiling framework in Windows.
- WPR/WPA: Windows Performance Recorder/Analyzer, tools that capture/analyze ETW traces.
- Mach: Microkernel used by Darwin (macOS/iOS). Pthreads are backed by Mach threads; many primitives build on Mach IPC/turnstiles.
- GCD (Grand Central Dispatch): Apple concurrency API (queues, groups, barriers) layered over Mach kernel primitives.
- TSan (ThreadSanitizer): Data race detector for C/C++/Go (Clang/GCC runtime).
- LLDB (Low Level Debugger): LLVM debugger, default on macOS; analogous to GDB.

x86/x86_64 instructions and prefixes:
- LOCK prefix: Forces an instruction to execute atomically with exclusive ownership of the cache line. Also acts as a full barrier for the locked operation.
- CMPXCHG: Compare register with memory; if equal, store a new value, else load memory into the register. With `LOCK`, it provides atomic CAS.
- XCHG: Exchange register and memory. With a memory operand it is atomic; `atomic_flag_test_and_set` can compile to `lock xchg`.
- PAUSE: Spin-wait hint that reduces power and pipeline penalties and improves Hyper-Threading fairness inside tight loops.
- MFENCE/LFENCE/SFENCE: Memory fence instructions (full, load, store) controlling reordering on x86 if explicitly needed.

AArch64 (ARMv8) instructions and barriers:
- LDAXR/STLXR: Load-Acquire Exclusive / Store-Release Exclusive. The LL/SC pair used for atomic RMW with acquire/release semantics. `STLXR` returns 0 on success, non-zero on failure (retry).
- CAS (LSE): Single-instruction compare-and-swap introduced by LSE; can include acquire/release semantics via suffixes (e.g., `CASAL`).
- LDADD (LSE): Atomic fetch-add on a memory location; other variants include `SWP` (swap), `STADD` (store-add) etc.
- DMB ish: Data Memory Barrier (inner shareable domain). Ensures ordering of memory operations across cores when needed.
- YIELD: Hint that the thread is in a busy-wait; gives other threads a chance to run.
- WFE/SEV: Wait For Event / Send Event. Low-power event-based waiting and wakeup hints, often used to augment spin-wait loops.

Further reading:
- Intel SDM (architecture and instruction set): https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
- Arm ARM (Armv8-A, barriers, LL/SC, LSE): https://developer.arm.com/documentation/ddi0487/latest

## References

- **Futex man page:** `man futex`
- **Intel Memory Ordering:** _Intel® 64 and IA-32 Architectures Software Developer's Manual, Vol. 3A, §8.2_
- **C11 Atomics:** ISO/IEC 9899:2011, §7.17
- **Linux clone:** `man clone`
- **MESI Protocol:** _Computer Architecture: A Quantitative Approach_ by Hennessy & Patterson
- **Perfbook:** [kernel.org/pub/linux/kernel/people/paulmck/perfbook/perfbook.html](https://kernel.org/pub/linux/kernel/people/paulmck/perfbook/perfbook.html)

---

## Exercises That Demonstrate These Concepts

- **Quick review:** `exercises/00_quick_review` - daily warmup, core concepts
- **Atomics:** `exercises/01_atomics` - lock-free programming, performance comparison
- **RW locks:** `exercises/02_rwlock` - concurrent reads, exclusive writes
- **Cache effects:** `exercises/03_cache_effects` - false sharing, padding
- **Memory ordering:** `exercises/04_memory_ordering` - acquire/release semantics
- **Spinlock internals:** `exercises/05_spinlock_internals` - implement from scratch
- **Barriers:** `exercises/06_barriers` - phase synchronization
