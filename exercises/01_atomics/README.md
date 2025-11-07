# Exercise 01: Atomic Operations & Memory Ordering

## 🎯 Goals
1. Understand the performance cost of different memory orders
2. See race conditions manifest in broken code
3. Master acquire-release synchronization patterns
4. Learn when relaxed ordering is safe

## 📖 What You'll Implement

### Part 1: Broken Counter (See the Race!)
Watch non-atomic operations lose increments:
```c
broken_counter++;  // Assembly: mov/inc/mov = 3 instructions!
// Lost increments due to interleaving
```

### Part 2: Sequential Consistency (Correct but Slow)
```c
atomic_fetch_add_explicit(&counter, 1, memory_order_seq_cst);
// Full memory barriers, total order
// x86: LOCK prefix + MFENCE
```

### Part 3: Relaxed Ordering (Fast!)
```c
atomic_fetch_add_explicit(&counter, 1, memory_order_relaxed);
// WHY SAFE: Each increment is independent
// No other memory operations to order relative to
// 1.5-2x faster than seq_cst
```

### Part 4: Message Passing (Where Ordering Matters!)
```c
// Producer
data = 42;
atomic_store_explicit(&ready, 1, memory_order_release);  // Barrier!

// Consumer
while (!atomic_load_explicit(&ready, memory_order_acquire));  // Barrier!
value = data;  // Guaranteed to see 42
```

## 🔧 Your Tasks

1. **Implement seq_cst version** - Complete the TODO in `seqcst_increment()`
2. **Implement relaxed version** - Complete the TODO in `relaxed_increment()`
3. **Fix message passing** - Implement proper acquire-release in producer/consumer

## 💡 Memory Ordering Guide

| Order | Use When | Cost | Guarantees |
|-------|----------|------|------------|
| `relaxed` | Independent ops (counters) | Lowest | Atomicity only |
| `acquire` | Loading shared data | Medium | Prevents reordering forward |
| `release` | Publishing shared data | Medium | Prevents reordering backward |
| `seq_cst` | Need total order | Highest | Everything |

**Rule of thumb:** Use acquire-release for synchronization, relaxed for independent statistics.

## 🔍 Analysis Commands

```bash
# See the race condition
make run-01        # Watch increments get lost!

# Inspect assembly
make asm-01        # Look for 'lock add' instruction

# Detect races with ThreadSanitizer
make tsan-01       # Catches the broken_counter race

# Compare performance
# Run solution to see seq_cst vs relaxed timing
```

## ⚠️ Key Insights

1. **Non-atomic = Race:** `counter++` is 3 instructions, can interleave
2. **seq_cst = Slow:** Full barriers, but simple reasoning
3. **relaxed = Fast:** No synchronization, needs careful analysis
4. **acquire-release = Sweet spot:** Efficient synchronization for message passing

## 📚 What to Learn

- **From assembly:** See `lock add` instruction (LOCK prefix = atomicity)
- **From timing:** Measure 1.5-2x difference between seq_cst and relaxed
- **From TSan:** Automated race detection catches bugs you'd miss
- **From message passing:** Understand happens-before relationships

## 🚀 Next Steps

After completing this exercise:
1. Check your understanding: Can you explain why relaxed is safe for counters?
2. Run `make asm-01` and find the LOCK prefix
3. Run `make tsan-01` and see the race detection
4. Move to Exercise 04 to understand x86 vs ARM memory models
