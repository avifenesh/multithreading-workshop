# Quick Start Guide

## Step 1: Build Everything
```bash
cd multithreading-workshop
make all
```

## Step 2: Start with Exercise 01
```bash
cd exercises/01_basics
cat README.md              # Read the task
cat 01_basics.c            # Look at the broken code
vim 01_basics.c            # Fix the TODOs
```

## Step 3: Compile & Test
```bash
# From workshop root
make 01_basics             # Compile just this exercise
make run-01                # Run it
```

## Step 4: Verify Your Solution
```bash
# Run all tests
make test
```

## Step 5: Check Solution (If Stuck)
```bash
cat exercises/01_basics/solution.c
```

## Workflow Summary

For each exercise:
1. **Read** `exercises/XX_name/README.md`
2. **Edit** `exercises/XX_name/XX_name.c`
3. **Build** `make XX_name` or `make all`
4. **Run** `make run-XX` or `./exercises/XX_name/XX_name`
5. **Test** `make test`
6. **Compare** with `solution.c` if needed

## Debugging Tips

### Compile with all warnings
```bash
gcc -Wall -Wextra -Werror -pthread program.c -o program
```

### Use ThreadSanitizer (detects data races)
```bash
gcc -fsanitize=thread -g -O1 program.c -o program -pthread
./program
```

### Use Valgrind Helgrind
```bash
valgrind --tool=helgrind ./program
```

### Enable debug symbols
```bash
gcc -g -pthread program.c -o program
gdb ./program
```

## Common Mistakes to Avoid

1. **Not joining threads** → Resource leak
2. **Not initializing mutexes** → Undefined behavior
3. **Using `if` instead of `while` with condvars** → Spurious wakeups
4. **Forgetting to unlock** → Deadlock
5. **Wrong lock order** → Deadlock

## Progress Through Exercises

- ✅ Exercise 01: Learn thread creation
- ✅ Exercise 02: Add mutex protection  
- ✅ Exercise 03: Fix race conditions
- ✅ Exercise 04: Master condition variables
- ✅ Exercise 05: Prevent deadlocks
- 🚀 Advanced: Atomics, RWLocks, and more in `ADVANCED.md`

Have fun! 🧵
