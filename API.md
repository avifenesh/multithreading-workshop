# pthread API Reference

Quick reference for POSIX threads. For internals, see SYSTEMS_GUIDE.md.

Note: POSIX (Portable Operating System Interface, IEEE 1003) is a standard API for Unix-like systems. The pthreads library is the POSIX threading API available on Linux and macOS; on Windows, use Win32 equivalents (e.g., SRWLOCK, CONDITION_VARIABLE) or a POSIX layer.

## Thread Management

```c
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void*), void *arg);
int pthread_join(pthread_t thread, void **retval);
void pthread_exit(void *retval);
pthread_t pthread_self(void);
int pthread_detach(pthread_t thread);
```

## Mutex

```c
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_trylock(pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex);
int pthread_mutex_destroy(pthread_mutex_t *mutex);
```

## Condition Variables

```c
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr);
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
                           const struct timespec *abstime);
int pthread_cond_signal(pthread_cond_t *cond);
int pthread_cond_broadcast(pthread_cond_t *cond);
int pthread_cond_destroy(pthread_cond_t *cond);
```

**Always use while loop:**
```c
pthread_mutex_lock(&mutex);
while (!condition) {
    pthread_cond_wait(&cond, &mutex);
}
// condition is true
pthread_mutex_unlock(&mutex);
```

## Read-Write Locks

```c
pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;
int pthread_rwlock_init(pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr);
int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_unlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_destroy(pthread_rwlock_t *rwlock);
```

## Spinlocks

```c
pthread_spinlock_t spin;
int pthread_spin_init(pthread_spinlock_t *lock, int pshared);
int pthread_spin_lock(pthread_spinlock_t *lock);
int pthread_spin_trylock(pthread_spinlock_t *lock);
int pthread_spin_unlock(pthread_spinlock_t *lock);
int pthread_spin_destroy(pthread_spinlock_t *lock);
```

**Use only for very short critical sections (<100ns).**

## Barriers

```c
pthread_barrier_t barrier;
int pthread_barrier_init(pthread_barrier_t *barrier, 
                         const pthread_barrierattr_t *attr,
                         unsigned count);
int pthread_barrier_wait(pthread_barrier_t *barrier);
int pthread_barrier_destroy(pthread_barrier_t *barrier);
```

## C11 Atomics

```c
#include <stdatomic.h>

atomic_int counter = 0;
atomic_store(&counter, 5);
atomic_store_explicit(&counter, 5, memory_order_release);
int value = atomic_load(&counter);
int value = atomic_load_explicit(&counter, memory_order_acquire);

atomic_fetch_add(&counter, 1);
atomic_fetch_sub(&counter, 1);
atomic_fetch_add_explicit(&counter, 1, memory_order_relaxed);

// Compare-and-swap
int expected = 5;
int desired = 10;
atomic_compare_exchange_strong(&counter, &expected, desired);
```

### Compare-and-exchange: weak vs strong, explicit orders

Prototypes (example for `atomic_int`):
```c
bool atomic_compare_exchange_weak(atomic_int *obj, int *expected, int desired);
bool atomic_compare_exchange_strong(atomic_int *obj, int *expected, int desired);

bool atomic_compare_exchange_weak_explicit(
    atomic_int *obj, int *expected, int desired,
    memory_order success_memorder, memory_order failure_memorder);
bool atomic_compare_exchange_strong_explicit(
    atomic_int *obj, int *expected, int desired,
    memory_order success_memorder, memory_order failure_memorder);
```

Key semantics:
- If `*obj == *expected`, store `desired` into `*obj` and return true.
- Otherwise, load the current value into `*expected` and return false.
- `weak` may fail spuriously (return false even if `*obj == *expected`), so use it in a loop. `strong` does not allow spurious failure.
- Failure memory order must not include release semantics (i.e., it can be `memory_order_relaxed` or `memory_order_acquire`, or `memory_order_seq_cst`).

Typical patterns:
```c
// 1) Spinlock acquire (TTAS) using weak CAS with loop
atomic_bool locked = false;
void lock(void) {
    for (;;) {
        if (!atomic_load_explicit(&locked, memory_order_relaxed)) {
            bool expected = false;
            if (atomic_compare_exchange_weak_explicit(
                    &locked, &expected, true,
                    memory_order_acquire,   // on success
                    memory_order_relaxed))  // on failure
                return;
        }
        // pause/yield hint here
    }
}
void unlock(void) {
    atomic_store_explicit(&locked, false, memory_order_release);
}

// 2) Single-shot CAS where spurious failure is undesirable → strong
int expected = old;
if (!atomic_compare_exchange_strong_explicit(
        &obj, &expected, new,
        memory_order_acq_rel, memory_order_acquire)) {
    // expected now has the current value of obj
}
```

Notes:
- Prefer `weak` in loops (especially on ARM/AArch64) because it maps naturally to LL/SC and can be cheaper; on x86, `weak` and `strong` often generate the same instruction.
- For pure “read” CAS (e.g., acquiring a flag), use `acquire` on success and `relaxed` on failure. For RMW that both reads and publishes, use `acq_rel` on success and `acquire` (or `relaxed`) on failure.

### Memory Orders

- `memory_order_relaxed` - No synchronization, only atomicity
- `memory_order_acquire` - Load: prevents later ops from moving before
- `memory_order_release` - Store: prevents earlier ops from moving after
- `memory_order_acq_rel` - Both acquire and release
- `memory_order_seq_cst` - Sequential consistency (default)

## Debugging Tools

### ThreadSanitizer
```bash
gcc -fsanitize=thread -g -O1 program.c -o program
./program
```

### Helgrind (Valgrind)
```bash
valgrind --tool=helgrind ./program
```

### GDB Thread Debugging
```bash
gdb ./program
(gdb) info threads
(gdb) thread 2
(gdb) bt
(gdb) thread apply all bt
```

## Performance Comparison

| Primitive | Uncontended | Contended | CPU While Waiting | Use When |
|-----------|-------------|-----------|-------------------|----------|
| **Mutex** | ~10-20ns | ~1-3µs | 0% (sleeps) | General locking |
| **Spinlock** | ~10ns | Variable | 100% (spins) | <100ns critical sections |
| **RWLock** | ~15-30ns | ~1-5µs | 0% (sleeps) | Many reads, few writes |
| **Atomic** | ~5-50ns | ~10-100ns | 0% (lock-free) | Simple counters/flags |

## Common Pitfalls

1. **Forgetting to unlock** - use RAII or careful error handling
2. **Using `if` instead of `while` with condvar** - spurious wakeups
3. **Deadlock** - inconsistent lock ordering
4. **False sharing** - cache line contention on separate variables
5. **Over-synchronization** - don't lock if you don't need to
6. **Wrong memory order** - don't use relaxed for synchronization
7. **Holding spinlock too long** - wastes CPU, priority inversion
