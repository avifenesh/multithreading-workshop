#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <stdalign.h>

// =============================================================================
// Timing Utilities
// =============================================================================

/**
 * Get high-resolution timestamp in nanoseconds
 */
static inline uint64_t get_nanos(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * TIME_BLOCK macro - measures execution time of a code block
 * 
 * Usage:
 *   TIME_BLOCK("my operation") {
 *       // code to measure
 *   }
 */
#define TIME_BLOCK(label) \
    for (uint64_t _start = get_nanos(), _elapsed = 0, _i = 0; \
         _i == 0; \
         _i++, _elapsed = get_nanos() - _start, \
         printf("%s: %.3f ms\n", label, _elapsed / 1e6))

/**
 * Simpler timing macro that stores elapsed time in a variable
 * 
 * Usage:
 *   double elapsed;
 *   TIME_IT(elapsed) {
 *       // code to measure
 *   }
 *   printf("Took: %.3f ms\n", elapsed * 1000);
 */
#define TIME_IT(elapsed_var) \
    for (struct timespec _ts_start, _ts_end; \
         clock_gettime(CLOCK_MONOTONIC, &_ts_start) == 0; ) \
    for (int _done = 0; !_done; \
         _done = 1, \
         clock_gettime(CLOCK_MONOTONIC, &_ts_end), \
         elapsed_var = (_ts_end.tv_sec - _ts_start.tv_sec) + \
                       (_ts_end.tv_nsec - _ts_start.tv_nsec) / 1e9)

// =============================================================================
// Cache-Aligned Allocation
// =============================================================================

#define CACHE_LINE_SIZE 64

/**
 * Allocate cache-aligned memory
 */
static inline void* cache_aligned_alloc(size_t size) {
    void *ptr = NULL;
    if (posix_memalign(&ptr, CACHE_LINE_SIZE, size) != 0) {
        return NULL;
    }
    return ptr;
}

/**
 * Macro to declare cache-aligned variables
 */
#define CACHE_ALIGNED alignas(CACHE_LINE_SIZE)

// =============================================================================
// Performance Counter Wrappers (Linux perf_event_open)
// =============================================================================

#ifdef __linux__
#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <string.h>

typedef struct {
    int fd;
    uint64_t count;
} perf_counter_t;

/**
 * Initialize a perf counter
 * type: PERF_TYPE_HARDWARE, PERF_TYPE_SOFTWARE, etc.
 * config: PERF_COUNT_HW_CACHE_MISSES, PERF_COUNT_HW_INSTRUCTIONS, etc.
 */
static inline int perf_counter_init(perf_counter_t *pc, uint32_t type, uint64_t config) {
    struct perf_event_attr pe;
    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type = type;
    pe.size = sizeof(struct perf_event_attr);
    pe.config = config;
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;
    
    pc->fd = (int)syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0);
    return pc->fd;
}

static inline void perf_counter_start(perf_counter_t *pc) {
    ioctl(pc->fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(pc->fd, PERF_EVENT_IOC_ENABLE, 0);
}

static inline void perf_counter_stop(perf_counter_t *pc) {
    ioctl(pc->fd, PERF_EVENT_IOC_DISABLE, 0);
    read(pc->fd, &pc->count, sizeof(uint64_t));
}

static inline void perf_counter_close(perf_counter_t *pc) {
    close(pc->fd);
}

/**
 * Convenience macro for measuring cache misses
 * 
 * Usage:
 *   MEASURE_CACHE_MISSES(misses) {
 *       // code to measure
 *   }
 *   printf("Cache misses: %lu\n", misses);
 */
#define MEASURE_CACHE_MISSES(miss_var) \
    for (perf_counter_t _pc_##miss_var;;) \
    for (int _init = (perf_counter_init(&_pc_##miss_var, PERF_TYPE_HARDWARE, \
                                        PERF_COUNT_HW_CACHE_MISSES), 1); \
         _init; _init = 0) \
    for (int _done = (perf_counter_start(&_pc_##miss_var), 0); !_done; \
         _done = 1, \
         perf_counter_stop(&_pc_##miss_var), \
         miss_var = _pc_##miss_var.count, \
         perf_counter_close(&_pc_##miss_var))

#endif // __linux__

// =============================================================================
// CPU Fence/Barrier Utilities
// =============================================================================

/**
 * Compiler barrier (prevents compiler reordering)
 */
#define COMPILER_BARRIER() __asm__ __volatile__("" ::: "memory")

/**
 * Full memory barrier (prevents compiler AND CPU reordering)
 */
#define MEMORY_BARRIER() __sync_synchronize()

/**
 * CPU pause instruction (for spin loops)
 */
#if defined(__x86_64__) || defined(__i386__)
    #define CPU_PAUSE() __asm__ __volatile__("pause" ::: "memory")
#elif defined(__aarch64__) || defined(__arm__)
    #define CPU_PAUSE() __asm__ __volatile__("yield" ::: "memory")
#else
    #define CPU_PAUSE() COMPILER_BARRIER()
#endif

// =============================================================================
// Statistics Helpers
// =============================================================================

typedef struct {
    double sum;
    double sum_sq;
    uint64_t count;
    double min;
    double max;
} stats_t;

static inline void stats_init(stats_t *s) {
    s->sum = 0.0;
    s->sum_sq = 0.0;
    s->count = 0;
    s->min = 1e300;
    s->max = -1e300;
}

static inline void stats_add(stats_t *s, double value) {
    s->sum += value;
    s->sum_sq += value * value;
    s->count++;
    if (value < s->min) s->min = value;
    if (value > s->max) s->max = value;
}

static inline double stats_mean(const stats_t *s) {
    return s->count > 0 ? s->sum / s->count : 0.0;
}

static inline double stats_stddev(const stats_t *s) {
    if (s->count < 2) return 0.0;
    double mean = stats_mean(s);
    double variance = (s->sum_sq / s->count) - (mean * mean);
    return variance > 0 ? __builtin_sqrt(variance) : 0.0;
}

static inline void stats_print(const stats_t *s, const char *label) {
    printf("%s: mean=%.3f ms, stddev=%.3f ms, min=%.3f ms, max=%.3f ms (n=%lu)\n",
           label,
           stats_mean(s) * 1000,
           stats_stddev(s) * 1000,
           s->min * 1000,
           s->max * 1000,
           s->count);
}

#endif // BENCHMARK_H
