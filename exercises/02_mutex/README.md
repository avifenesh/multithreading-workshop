# Exercise 02: Mutex Protection

## 🎯 Goal
Learn how to use mutexes to protect shared data.

## 📖 Concepts
- `pthread_mutex_t` - Mutex type
- `pthread_mutex_init()` - Initialize a mutex
- `pthread_mutex_lock()` - Acquire the lock
- `pthread_mutex_unlock()` - Release the lock
- `pthread_mutex_destroy()` - Clean up

## 🔧 Your Task
Multiple threads are incrementing a shared counter, but without protection it won't be accurate.
Add mutex locks to protect the critical section.

## ✅ Expected Output
```
Final counter value: 10000
Success! Counter is correct.
```

## 💡 Hints
- Lock before accessing shared data
- Unlock after you're done
- Keep the critical section small
