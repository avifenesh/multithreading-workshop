# Exercise 01: Thread Basics

## 🎯 Goal
Learn how to create and join threads using pthread.

## 📖 Concepts
- `pthread_create()` - Creates a new thread
- `pthread_join()` - Waits for a thread to finish
- Thread function signature: `void* function(void* arg)`

## 🔧 Your Task
The code is incomplete. Fill in the TODOs to:
1. Create 5 threads that each print their ID
2. Wait for all threads to complete before exiting

## ✅ Expected Output
```
Thread 0 is running
Thread 1 is running
Thread 2 is running
Thread 3 is running
Thread 4 is running
All threads completed!
```

## 💡 Hints
- `pthread_create(&thread, NULL, function, arg)`
- `pthread_join(thread, NULL)`
- Cast thread ID to `(void*)(long)` when passing as argument
