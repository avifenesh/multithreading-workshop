# Exercise 04: Producer-Consumer with Condition Variables

## 🎯 Goal
Master `pthread_cond_t` - condition variables for thread signaling.

## 📖 C-Specific Concepts
- **Condition Variables**: `pthread_cond_t` - wait for a condition without busy-waiting
- **`pthread_cond_wait()`**: Atomically releases mutex and waits. Re-acquires mutex before returning.
- **`pthread_cond_signal()`**: Wakes one waiting thread
- **`pthread_cond_broadcast()`**: Wakes all waiting threads
- **Spurious wakeups**: Always use a while loop, not if, when checking conditions

## 🔧 Your Task
Implement a bounded buffer (circular queue) shared between producer and consumer threads.

**Key C threading details:**
- `pthread_cond_wait()` is NOT just "wait and unlock" - it's atomic
- Must always hold the mutex when calling condition variable functions
- Predicate checks must be in while loops (spurious wakeups are real)

## ✅ Expected Output
```
Producer 0 produced: 1
Consumer 0 consumed: 1
...
All items produced and consumed successfully!
```

## 💡 Hints
- Use two condition variables: `not_full` and `not_empty`
- Producer waits on `not_full`, signals `not_empty`
- Consumer waits on `not_empty`, signals `not_full`
