# Exercise 05: Deadlock Detection & Prevention

## 🎯 Goal
Understand deadlock conditions and how to prevent them in C.

## 📖 C-Specific Concepts
- **Deadlock conditions**: Mutual exclusion, hold and wait, no preemption, circular wait
- **Lock ordering**: Prevent circular wait by acquiring locks in consistent order
- **`pthread_mutex_trylock()`**: Non-blocking lock attempt
- **Timeout mechanisms**: Using `pthread_mutex_timedlock()` (if available)

## 🔧 Your Task
Two threads transfer money between accounts, but they deadlock!

**The bug:**
- Thread A: locks account1, then account2
- Thread B: locks account2, then account1
- Classic circular wait → DEADLOCK

## ✅ Expected Output
```
Transferring $50 from account 0 to account 1
Transferring $30 from account 1 to account 0
...
All transfers completed successfully!
Final balances: Account 0: $1000, Account 1: $1000
```

## 💡 Hints
- Fix by always locking accounts in same order (e.g., lower ID first)
- Compare account pointers/IDs before locking
- Alternative: use `pthread_mutex_trylock()` with backoff
