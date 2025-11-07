CC = gcc
CFLAGS = -Wall -Wextra -pthread -I./include
CFLAGS_OPT = $(CFLAGS) -O2
CFLAGS_TSAN = $(CFLAGS) -g -O1 -fsanitize=thread
LDFLAGS = -pthread

EXERCISES = 00_quick_review 01_atomics 02_rwlock 03_cache_effects 04_memory_ordering 05_spinlock_internals 06_barriers 07_lockfree_queue

BINARIES = $(foreach ex,$(EXERCISES),exercises/$(ex)/$(ex))
SOLUTIONS = $(foreach ex,$(EXERCISES),exercises/$(ex)/solution)

.PHONY: all clean help $(EXERCISES) asm tsan perf objdump

all: $(BINARIES)

solutions: $(SOLUTIONS)

# Build exercises
exercises/%/% : exercises/%/%.c
	$(CC) $(CFLAGS_OPT) -o $@ $< $(LDFLAGS)

# Build solutions
exercises/%/solution : exercises/%/solution.c
	$(CC) $(CFLAGS_OPT) -o $@ $< $(LDFLAGS)

# Individual exercise targets
00_quick_review: exercises/00_quick_review/00_quick_review
01_atomics: exercises/01_atomics/01_atomics
02_rwlock: exercises/02_rwlock/02_rwlock
03_cache_effects: exercises/03_cache_effects/03_cache_effects
04_memory_ordering: exercises/04_memory_ordering/04_memory_ordering
05_spinlock_internals: exercises/05_spinlock_internals/05_spinlock_internals
06_barriers: exercises/06_barriers/06_barriers

# Run individual exercises
run-00: exercises/00_quick_review/00_quick_review
	@./exercises/00_quick_review/00_quick_review

run-01: exercises/01_atomics/01_atomics
	@./exercises/01_atomics/01_atomics

run-02: exercises/02_rwlock/02_rwlock
	@./exercises/02_rwlock/02_rwlock

run-03: exercises/03_cache_effects/03_cache_effects
	@./exercises/03_cache_effects/03_cache_effects

run-04: exercises/04_memory_ordering/04_memory_ordering
	@./exercises/04_memory_ordering/04_memory_ordering

run-05: exercises/05_spinlock_internals/05_spinlock_internals
	@./exercises/05_spinlock_internals/05_spinlock_internals

run-06: exercises/06_barriers/06_barriers
	@./exercises/06_barriers/06_barriers

run-07: exercises/07_lockfree_queue/07_lockfree_queue
	@./exercises/07_lockfree_queue/07_lockfree_queue

# Assembly output
asm-%: exercises/%/%.c
	$(CC) $(CFLAGS_OPT) -S -fverbose-asm -masm=intel -o exercises/$*/$*.s $<
	@echo "Assembly: exercises/$*/$*.s"

# ThreadSanitizer
tsan-%: exercises/%/%.c
	$(CC) $(CFLAGS_TSAN) -o exercises/$*/$*_tsan $< $(LDFLAGS)
	@./exercises/$*/$*_tsan || true

# Perf analysis
perf-%: exercises/%/%
	perf stat -e cache-references,cache-misses,LLC-load-misses,context-switches ./$<

# Disassembly
objdump-%: exercises/%/%
	objdump -d -M intel -S $< | less

clean:
	rm -f $(BINARIES) $(SOLUTIONS)
	rm -f exercises/*/*.s exercises/*/*_tsan
	@echo "Cleaned all binaries"

help:
	@echo "Systems-Level Multithreading Workshop"
	@echo ""
	@echo "Build:"
	@echo "  make all          - Build all exercises"
	@echo "  make solutions    - Build all solutions"
	@echo "  make 01_atomics   - Build specific exercise"
	@echo ""
	@echo "Run:"
	@echo "  make run-01       - Run exercise 01"
	@echo "  make run-03       - Run exercise 03"
	@echo ""
	@echo "Analysis:"
	@echo "  make asm-05       - Generate assembly (see LOCK prefix)"
	@echo "  make tsan-04      - ThreadSanitizer race detection"
	@echo "  make perf-03      - Performance counters"
	@echo "  make objdump-05   - Disassemble binary"
	@echo ""
	@echo "Examples:"
	@echo "  make asm-05 && less exercises/05_spinlock_internals/05_spinlock_internals.s"
	@echo "  perf stat -e cache-misses ./exercises/03_cache_effects/03_cache_effects"
