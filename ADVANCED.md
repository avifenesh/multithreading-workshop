# Advanced C Threading Exercises

Once you've completed the basic exercises (01-05), dive into these advanced topics specific to C threading.

## Advanced Exercises

### 06. C11 Atomics (`advanced/06_atomics`)
Lock-free programming with `stdatomic.h`. Learn about memory ordering, atomic operations, and compare-and-swap.

**Compile:** `gcc -std=c11 -pthread advanced/06_atomics/06_atomics.c -o advanced/06_atomics/06_atomics`

### 07. Read-Write Locks (`advanced/07_rwlock`)
`pthread_rwlock_t` for reader-writer scenarios. Multiple concurrent readers, exclusive writers.

**Compile:** `gcc -pthread advanced/07_rwlock/07_rwlock.c -o advanced/07_rwlock/07_rwlock`

### 08. Barriers (Coming Soon)
`pthread_barrier_t` for synchronizing multiple threads at a point.

### 09. Spinlocks (Coming Soon)
`pthread_spinlock_t` for very short critical sections.

## Quick Comparison

| Primitive | Use Case | Overhead | Contention Behavior |
|-----------|----------|----------|---------------------|
| **Mutex** | General locking | Medium | Sleeps (context switch) |
| **Spinlock** | Very short critical sections | Low | Busy-waits (burns CPU) |
| **RWLock** | Many reads, few writes | Medium | Allows concurrent readers |
| **Atomic** | Simple counters/flags | Very low | Lock-free (hardware) |
| **Condvar** | Wait for condition | Medium | Sleeps until signaled |

## Memory Models

### C11 Memory Orders (from weakest to strongest)
1. **`memory_order_relaxed`**: No synchronization, only atomicity
2. **`memory_order_acquire/release`**: Acquire-release semantics
3. **`memory_order_acq_rel`**: Both acquire and release
4. **`memory_order_seq_cst`**: Sequential consistency (default)

**Rule of thumb:** Start with `seq_cst`, optimize to weaker orders only if profiling shows it matters.

## pthread API Reference

### Thread Management
```c
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void*), void *arg);
int pthread_join(pthread_t thread, void **retval);
void pthread_exit(void *retval);
pthread_t pthread_self(void);
```

### Mutex
```c
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_trylock(pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex);
int pthread_mutex_destroy(pthread_mutex_t *mutex);
```

### Condition Variables
```c
int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr);
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
int pthread_cond_signal(pthread_cond_t *cond);
int pthread_cond_broadcast(pthread_cond_t *cond);
int pthread_cond_destroy(pthread_cond_t *cond);
```

### RWLock
```c
int pthread_rwlock_init(pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr);
int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_unlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_destroy(pthread_rwlock_t *rwlock);
```

### C11 Atomics
```c
#include <stdatomic.h>

atomic_int counter = 0;
atomic_store(&counter, 5);
int value = atomic_load(&counter);
atomic_fetch_add(&counter, 1);
atomic_fetch_sub(&counter, 1);

// Compare-and-swap
int expected = 5;
int desired = 10;
atomic_compare_exchange_strong(&counter, &expected, desired);
```

## Debugging Tips

### Compile with ThreadSanitizer
```bash
gcc -fsanitize=thread -g -O1 program.c -o program
./program
```

### Valgrind with Helgrind (race detector)
```bash
valgrind --tool=helgrind ./program
```

### GDB Thread Debugging
```bash
gdb ./program
(gdb) info threads
(gdb) thread 2
(gdb) bt
```

## Common Pitfalls

1. **Forgetting to unlock**: Always use RAII patterns or error handling
2. **Wrong order with condvars**: Always check condition in `while` loop
3. **Deadlock**: Lock ordering matters!
4. **False sharing**: Cache line contention on separate variables
5. **Over-synchronization**: Don't lock if you don't need to
