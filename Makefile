CC = gcc
TSAN_CC ?= clang
CFLAGS = -Wall -Wextra -pthread -I./include
CFLAGS_OPT = $(CFLAGS) -O2
CFLAGS_TSAN = $(CFLAGS) -g -O1 -fsanitize=thread
LDFLAGS = -pthread

EXERCISES = 00_quick_review 01_atomics 02_rwlock 03_cache_effects 04_memory_ordering 05_spinlock_internals 06_barriers 07_lockfree_queue 08_summary

# Helpers to resolve either numeric prefixes (e.g., 01) or full exercise names.
find_exercise = $(strip \
	$(if $(filter $(1),$(EXERCISES)),$(1),\
	$(firstword $(filter $(1)_%,$(EXERCISES)))))
ensure_exercise = $(if $(call find_exercise,$1),,$(error Unknown exercise '$1'))
exercise_dir = $(call find_exercise,$1)
exercise_src = exercises/$(call exercise_dir,$1)/$(call exercise_dir,$1).c
exercise_bin = exercises/$(call exercise_dir,$1)/$(call exercise_dir,$1)
exercise_asm = exercises/$(call exercise_dir,$1)/$(call exercise_dir,$1).s
exercise_tsan = exercises/$(call exercise_dir,$1)/$(call exercise_dir,$1)_tsan

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
07_lockfree_queue: exercises/07_lockfree_queue/07_lockfree_queue
08_summary: exercises/08_summary/08_summary

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

run-08: exercises/08_summary/08_summary
	@./exercises/08_summary/08_summary

# Assembly output
asm-%:
	$(call ensure_exercise,$*)
	$(CC) $(CFLAGS_OPT) -S -fverbose-asm -masm=intel -o $(call exercise_asm,$*) $(call exercise_src,$*)
	@echo "Assembly: $(call exercise_asm,$*)"

# ThreadSanitizer
tsan-%:
	$(call ensure_exercise,$*)
	$(TSAN_CC) $(CFLAGS_TSAN) -o $(call exercise_tsan,$*) $(call exercise_src,$*) $(LDFLAGS)
	@$(call exercise_tsan,$*) || true

# Perf analysis
perf-%:
	$(call ensure_exercise,$*)
	$(MAKE) $(call exercise_bin,$*)
	perf stat -e cache-references,cache-misses,L1-dcache-load-misses,context-switches,instructions,cycles ./$(call exercise_bin,$*)

# Disassembly
objdump-%:
	$(call ensure_exercise,$*)
	$(MAKE) $(call exercise_bin,$*)
	objdump -d -M intel -S $(call exercise_bin,$*) | less

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
