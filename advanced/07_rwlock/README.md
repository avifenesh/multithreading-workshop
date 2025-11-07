# Advanced 07: Read-Write Locks

## 🎯 Goal
Use `pthread_rwlock_t` for reader-writer scenarios.

## 📖 RWLock Concepts
- **`pthread_rwlock_t`**: Allows multiple readers OR one writer
- **`pthread_rwlock_rdlock()`**: Acquire read lock (shared)
- **`pthread_rwlock_wrlock()`**: Acquire write lock (exclusive)
- **`pthread_rwlock_unlock()`**: Release lock

## 🔧 Scenario
Database with many reads, few writes. Regular mutex serializes all access (slow). RWLock allows concurrent reads (fast).

## 💡 Performance Characteristics
- Multiple readers can hold lock simultaneously
- Writers get exclusive access
- Better than mutex when reads >> writes
- Overhead slightly higher than mutex for single-threaded case

## ⚠️ Watch For
- Writer starvation: If readers constantly hold lock, writers may wait forever
- Some implementations prefer writers to prevent starvation
