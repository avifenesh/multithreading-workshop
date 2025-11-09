# Exercise 08: Summary Capstone

Pull together the core topics from the workshop with one small program that demonstrates multiple concurrency strategies, compares their performance, and explains the results.

## Goals
- Practice atomics, acquire/release ordering, and cache effects.
- Compare lock-based vs lock-avoiding approaches.
- Observe the impact of false sharing and alignment.

## Variants
- Variant A — Locked counter (baseline)
  - N threads increment a shared counter protected by `pthread_mutex_t` or `pthread_spinlock_t`.
  - Measure throughput with `TIME_BLOCK`.

- Variant B — Per-thread counters + merge
  - Each thread increments its own counter using `memory_order_relaxed`.
  - Merge at the end in a single thread.
  - Do it twice: first without padding (expect false sharing), then with `alignas(64)` padding (expect 2–10× speedup).

- Variant C — Message passing (SPSC)
  - Use a single-producer/single-consumer queue to pass integers from a producer to a consumer that sums them.
  - Use `memory_order_release`/`acquire` on head/tail to synchronize.
  - Validate the final sum.

Optional — Spinlock comparison
- Implement TAS vs TTAS as in `exercises/05_spinlock_internals` and use as the lock for Variant A. Compare results.

## Hints
- Start together: use a `pthread_barrier_t` or an atomic start flag (`release` on writer, `acquire` on readers).
- Measure: use `TIME_BLOCK` from `include/benchmark.h`.
- Inspect assembly: `make asm-08` (use `less exercises/08_summary/08_summary.s`).
- Profile:
  - Linux: `make perf-08` to see cache-misses and context-switches.
  - macOS: use Time Profiler via `xcrun xctrace` (see root README Platform Notes).
  - Windows: prefer WSL2 for parity; otherwise VS Profiler or WPA.

## Acceptance
- All variants compute the same total.
- Removing `alignas(64)` in Variant B causes a measurable slowdown (false sharing).
- Variant B (padded) should outperform Variant A under contention; TTAS should beat TAS if you add that comparison.

## Build & Run
```bash
make 08_summary
make run-08

# Analysis
make asm-08
make perf-08
make tsan-08 || true  # some variants may not be meaningful under TSan
```

## Files
- `08_summary.c` — Starter with minimal working implementations and notes on where to extend.
- `solution.c` — Reference build (simplified). Focus on understanding and measurements.

