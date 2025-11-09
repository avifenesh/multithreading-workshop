# Exercise 08: Summary Capstone

A true hands-on challenge that tests your ability to recall and apply concepts from exercises 01-07. You must implement three concurrent counting strategies **from scratch** - no code is provided, only TODO hints.

## Overview

This capstone exercise synthesizes everything you've learned:
- **Exercise 01**: Atomic operations and memory ordering
- **Exercise 03**: Cache effects and false sharing
- **Exercise 05**: Spinlock implementations
- **Exercise 07**: Lock-free SPSC queue patterns

## The Challenge

Implement three variants that all compute the same total (NUM_THREADS × INCREMENTS):

### Variant A: Synchronized Shared Counter
- **Concept**: Multiple threads increment a single shared counter
- **Your Task**:
  - Choose a synchronization primitive (mutex, spinlock, or custom TAS/TTAS)
  - Implement thread workers that wait for start signal and safely increment
  - Measure performance as a baseline
- **Learning Goals**:
  - Contention cost
  - Synchronization overhead
  - Lock implementation tradeoffs

### Variant B: Per-Thread Counters
- **Concept**: Each thread has its own counter, merge at the end
- **Your Task**:
  - Define `packed_counter_t` (no padding → false sharing)
  - Define `padded_counter_t` (alignas(64) → cache line isolation)
  - Implement both versions and observe the performance delta
- **Learning Goals**:
  - Cache coherency traffic
  - False sharing impact (expect 2-10x slowdown for packed)
  - When to use relaxed atomics

### Variant C: Lock-Free SPSC Queue
- **Concept**: Producer sends 1..NUM_MESSAGES, consumer sums them
- **Your Task**:
  - Implement queue structure with cache-aligned head/tail
  - Implement enqueue with acquire/release semantics
  - Implement dequeue with acquire/release semantics
  - Producer/consumer threads that use the queue
- **Learning Goals**:
  - Acquire/release ordering for message passing
  - Lock-free synchronization
  - Cache line bouncing between producer and consumer

## Success Criteria

✅ **Correctness**:
- All variants compute correct totals
- No data races (verify with `make tsan-08`)

✅ **Performance**:
- Variant B padded is 2-10x faster than packed
- Variant B (either version) is faster than Variant A
- Variant C demonstrates efficient message passing

✅ **Understanding**:
- You can explain why each performs differently
- You understand when to use each pattern
- You can identify the memory ordering requirements

## Build & Run

```bash
# Build and run
make 08_summary
make run-08

# Verify correctness (should see ✓ for all variants)
./exercises/08_summary/08_summary

# Check for data races
make tsan-08

# Measure cache effects
make perf-08

# Inspect assembly
make asm-08
less exercises/08_summary/08_summary.s
```

## Implementation Tips

### Variant A Tips
- Start with pthread_mutex_t for simplicity
- Optional: Try pthread_spinlock_t and compare
- Advanced: Implement your own TAS or TTAS spinlock from exercise 05
- Remember: acquire lock → increment → release lock

### Variant B Tips
- Packed: `typedef struct { atomic_long value; } packed_counter_t;`
- Padded: `typedef struct { alignas(64) atomic_long value; } padded_counter_t;`
- Use `memory_order_relaxed` for increments (no cross-thread sync needed)
- Use `calloc()` to allocate arrays, `free()` to cleanup
- Each thread gets pointer to its own counter: `&counters[thread_id]`

### Variant C Tips
- Queue structure:
  ```c
  typedef struct {
      int buffer[QUEUE_SIZE];
      alignas(64) atomic_size_t head;  // Producer cache line
      alignas(64) atomic_size_t tail;  // Consumer cache line
  } spsc_queue_t;
  ```
- Enqueue: relaxed load head, acquire load tail, release store head
- Dequeue: relaxed load tail, acquire load head, release store tail
- Use `(index + 1) & MASK` for ring buffer wrap-around
- Return sum via `return (void*)(intptr_t)sum;`

## Analysis Questions

After implementing all variants, answer these:

1. **Why is padded faster than packed?**
   - Hint: What happens when two threads write to the same cache line?

2. **Why does false sharing occur in packed?**
   - Hint: How big is a cache line? How big is atomic_long?

3. **When would you choose Variant A over B?**
   - Hint: Consider memory usage and code complexity

4. **Why use acquire/release in Variant C?**
   - Hint: What guarantees do you need for message passing?

5. **What's the cost of a mutex vs relaxed atomic?**
   - Hint: Look at TIME_BLOCK output and context-switches in perf

## Common Mistakes

❌ **Using seq_cst everywhere**: Variant B only needs relaxed atomics
❌ **Forgetting cache alignment**: Both variants benefit from alignas(64)
❌ **Wrong memory order in queue**: Must use acquire/release, not relaxed
❌ **Not waiting for start_flag**: Threads might start before others are created
❌ **Forgetting to free memory**: Check with valgrind

## Advanced Challenges

Once you have working implementations:

1. **Spinlock Showdown**: Implement TAS, TTAS, and TTAS+PAUSE for Variant A. Compare.
2. **Scalability Test**: Increase NUM_THREADS to 16, 32, 64. What breaks first?
3. **Multi-Producer Queue**: Can you extend Variant C to support multiple producers?
4. **Hybrid Approach**: Combine batching with per-thread counters (flush every N ops)
5. **Different Architectures**: How do results differ on ARM vs x86?

## Reference Solution

Check `solution.c` for a reference implementation, but try to complete it yourself first! The learning happens in the struggle to recall and apply concepts.

## Key Takeaways

After completing this exercise, you should deeply understand:
- **When to use locks** vs **lock-free** vs **thread-local** approaches
- **How cache coherency affects performance** (false sharing)
- **Why memory ordering matters** (acquire/release for message passing)
- **How to analyze performance** (perf, assembly, timing)
- **Real-world tradeoffs** (simplicity vs performance vs correctness)

This is the culmination of everything you've learned. Make it count! 🚀
