# C Multithreading Workshop - Project Summary

## Overview
Interactive C threading workshop focused on low-level pthread API, C11 atomics, memory models, and POSIX threading primitives. Designed for experienced developers who want hands-on practice with C-specific threading details.

## Structure

```
multithreading-workshop/
├── README.md                    # Main overview
├── QUICKSTART.md                # Getting started guide
├── ADVANCED.md                  # Advanced topics + API reference
├── Makefile                     # Build system
├── tests/
│   └── run_tests.sh            # Automated test runner
├── exercises/                   # Core exercises (01-05)
│   ├── 01_basics/              # pthread_create, pthread_join
│   ├── 02_mutex/               # pthread_mutex_t basics
│   ├── 03_race_condition/      # Fix actual race condition bug
│   ├── 04_producer_consumer/   # Condition variables (pthread_cond_t)
│   └── 05_deadlock/            # Deadlock prevention via lock ordering
└── advanced/                    # Advanced topics (06-09)
    ├── 06_atomics/             # C11 stdatomic.h, memory orders
    ├── 07_rwlock/              # pthread_rwlock_t
    ├── 08_barriers/            # (Coming soon)
    └── 09_spinlock/            # (Coming soon)
```

## Key Features

### 1. Broken Code to Fix
Each exercise has intentionally broken/incomplete code with TODO comments marking what to implement.

### 2. Progressive Difficulty
- Start with basic thread creation
- Build up to condition variables
- End with deadlock scenarios
- Advanced exercises cover lock-free programming

### 3. C-Specific Focus
- POSIX pthread API deep dive
- C11 atomics and memory ordering
- Memory models (relaxed, acquire/release, seq_cst)
- Platform-specific synchronization primitives

### 4. Automated Testing
```bash
make test  # Runs all exercise tests
```

### 5. Solutions Provided
Each exercise includes a `solution.c` for reference.

## Exercise Topics

| Exercise | Topic | Key Concepts |
|----------|-------|--------------|
| 01 | Thread Basics | `pthread_create`, `pthread_join`, thread functions |
| 02 | Mutex | `pthread_mutex_t`, lock/unlock, critical sections |
| 03 | Race Conditions | Identifying and fixing non-atomic operations |
| 04 | Producer-Consumer | `pthread_cond_t`, condition variables, spurious wakeups |
| 05 | Deadlock | Circular wait, lock ordering, deadlock prevention |
| 06 | Atomics | `stdatomic.h`, memory ordering, lock-free counters |
| 07 | RWLock | `pthread_rwlock_t`, concurrent readers |
| 08 | Barriers | Thread synchronization points |
| 09 | Spinlocks | User-space spinning, cache coherence |

## C Threading Primitives Covered

- **Thread management**: `pthread_create`, `pthread_join`, `pthread_exit`
- **Mutexes**: `pthread_mutex_t`, init, lock, unlock, destroy
- **Condition variables**: `pthread_cond_t`, wait, signal, broadcast
- **RWLocks**: `pthread_rwlock_t`, rdlock, wrlock
- **Atomics**: C11 `atomic_int`, fetch_add, compare_exchange, memory orders
- **Barriers**: `pthread_barrier_t` (planned)
- **Spinlocks**: `pthread_spinlock_t` (planned)

## Debugging Tools Referenced

- **ThreadSanitizer**: `-fsanitize=thread` for race detection
- **Helgrind**: Valgrind tool for threading bugs
- **GDB**: Multi-threaded debugging
- Compiler warnings: `-Wall -Wextra`

## Learning Objectives

1. **Master pthread API** - Understand return values, error handling, attributes
2. **Understand memory models** - C11 memory ordering implications
3. **Recognize patterns** - Producer-consumer, reader-writer, barriers
4. **Debug threading bugs** - Race conditions, deadlocks, data races
5. **Performance trade-offs** - Mutex vs atomic vs spinlock vs rwlock
6. **Low-level details** - Thread-local storage, context switching, cache coherence

## Quick Commands

```bash
# Build all exercises
make all

# Run specific exercise
make run-01

# Run all tests
make test

# Build solutions
make solutions

# Clean
make clean

# Help
make help
```

## Target Audience

Experienced developers who:
- Know multithreading concepts
- Want C-specific implementation details
- Need hands-on practice with pthread API
- Want to understand atomics and memory models
- Care about the "bits and bytes" of C threading

Not for: Threading beginners who need conceptual introduction.
