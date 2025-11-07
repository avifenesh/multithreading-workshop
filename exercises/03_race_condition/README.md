# Exercise 03: Fix the Race Condition

## 🎯 Goal
Identify and fix a classic race condition bug.

## 📖 Concepts
- Race conditions occur when multiple threads access shared data without synchronization
- The outcome depends on thread scheduling (non-deterministic)
- Even simple operations like `balance += amount` are not atomic

## 🔧 Your Task
A bank account simulation has a critical bug! Multiple threads are depositing and withdrawing money, but the final balance is wrong.

**The bug:** The balance update involves:
1. Read current balance
2. Calculate new balance
3. Write new balance

These steps are NOT atomic, causing lost updates.

## ✅ Expected Output
```
Starting balance: 1000
Final balance: 1000
Success! Balance is correct.
```

## 💡 Hints
- Look for the critical section where `balance` is modified
- You'll need to add a mutex
- Protect ALL accesses to the shared variable
