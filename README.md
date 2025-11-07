# Multithreading Workshop in C 🧵

Welcome to the interactive multithreading workshop! Learn pthread concepts through hands-on exercises.

## 🎯 Learning Path

Each exercise builds on the previous one:

1. **01_basics** - Create and join threads
2. **02_mutex** - Protect shared data with mutexes
3. **03_race_condition** - Fix a broken counter (classic race condition)
4. **04_producer_consumer** - Implement the producer-consumer pattern
5. **05_deadlock** - Identify and fix a deadlock scenario

## 🚀 Getting Started

### Prerequisites
```bash
sudo apt-get install build-essential
```

### Build Everything
```bash
make all
```

### Run Tests
```bash
make test
```

### Run Individual Exercise
```bash
make run-01  # or run-02, run-03, etc.
```

### Clean Build
```bash
make clean
```

## 📚 How to Learn

1. Read the `README.md` in each exercise folder
2. Look at the broken code in `<exercise>.c`
3. Follow the TODO comments to fix the code
4. Run `make test` to verify your solution
5. Check `solution.c` if you get stuck

## 🎓 Tips

- Start with exercise 01 and work sequentially
- Read man pages: `man pthread_create`, `man pthread_mutex_lock`
- Use `-lpthread` when compiling manually
- Watch for race conditions - they're sneaky!

Happy threading! 🎉

## 🚀 Advanced Topics

Once you complete exercises 01-05, check out [ADVANCED.md](ADVANCED.md) for:
- **C11 Atomics** - Lock-free programming with `stdatomic.h`
- **Read-Write Locks** - `pthread_rwlock_t` for concurrent readers
- **Memory models** and ordering
- **Complete pthread API reference**
- **Debugging with ThreadSanitizer and Helgrind**

## 📚 Additional Resources

- `man pthreads` - POSIX threads overview
- `man pthread_create` - Individual function man pages
- [POSIX Threads Programming](https://hpc-tutorials.llnl.gov/posix/) - LLNL Tutorial
- C11 Standard (ISO/IEC 9899:2011) - Section 7.17 for atomics
