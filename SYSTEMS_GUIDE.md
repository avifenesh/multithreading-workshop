# Systems-Level Guide to Multithreading in C

This guide goes beyond API usage to explain **how multithreading actually works** at the OS, libc, and hardware level.

---

## Table of Contents
1. [Thread Creation & Scheduling](#thread-creation--scheduling)
2. [Memory Architecture & Cache Coherency](#memory-architecture--cache-coherency)
3. [Synchronization Primitives Implementation](#synchronization-primitives-implementation)
4. [Memory Models & Ordering](#memory-models--ordering)
5. [Performance Analysis](#performance-analysis)

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
   
2. **Thread Local Storage (TLS) Setup**
   - Each thread gets its own `errno`, `pthread_self()` identity
   - On x86_64: stored in `%fs` segment register
   - Compiler accesses via `%fs:offset` addressing
   
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

## References

- **Futex man page:** `man futex`
- **Intel Memory Ordering:** _Intel® 64 and IA-32 Architectures Software Developer's Manual, Vol. 3A, §8.2_
- **C11 Atomics:** ISO/IEC 9899:2011, §7.17
- **Linux clone:** `man clone`
- **MESI Protocol:** _Computer Architecture: A Quantitative Approach_ by Hennessy & Patterson
- **Perfbook:** [kernel.org/pub/linux/kernel/people/paulmck/perfbook/perfbook.html](https://kernel.org/pub/linux/kernel/people/paulmck/perfbook/perfbook.html)

---

## Exercises That Demonstrate These Concepts

- **Cache effects:** `advanced/08_cache_effects` - false sharing, padding
- **Memory ordering:** `advanced/09_memory_ordering` - acquire/release semantics
- **Spinlock internals:** `advanced/10_spinlock_internals` - implement from scratch
- **Barriers:** `advanced/11_barriers` - phase synchronization

