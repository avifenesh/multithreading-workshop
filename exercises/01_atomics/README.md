# Advanced 06: C11 Atomics

## 🎯 Goal
Use C11 `stdatomic.h` for lock-free programming.

## 📖 C11 Atomic Concepts
- **`_Atomic` type qualifier**: Makes operations atomic at hardware level
- **`atomic_int`, `atomic_long`**: Atomic integer types
- **Memory orders**: `memory_order_relaxed`, `memory_order_acquire`, `memory_order_release`, `memory_order_seq_cst`
- **`atomic_load()`, `atomic_store()`**: Explicit memory order control
- **`atomic_fetch_add()`, `atomic_fetch_sub()`**: Atomic read-modify-write
- **`atomic_compare_exchange_strong()`**: CAS operation (compare-and-swap)

## 🔧 Your Task
Implement a lock-free counter using C11 atomics. Compare performance with mutex version.

## 💡 Key Differences
```c
// Mutex version (heavyweight)
pthread_mutex_lock(&mutex);
counter++;
pthread_mutex_unlock(&mutex);

// Atomic version (lock-free, hardware-level)
atomic_fetch_add(&counter, 1);
```

## ⚠️ Memory Ordering
- `memory_order_relaxed`: No synchronization, just atomicity
- `memory_order_acquire/release`: Synchronizes with other threads
- `memory_order_seq_cst`: Sequential consistency (default, strongest)

Trade-off: Weaker ordering = better performance, but harder to reason about.
