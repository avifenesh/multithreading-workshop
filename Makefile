CC = gcc
CFLAGS = -Wall -Wextra -pthread -I./include
LDFLAGS = -pthread

EXERCISES = 01_basics 02_mutex 03_race_condition 04_producer_consumer 05_deadlock

BINARIES = $(foreach ex,$(EXERCISES),exercises/$(ex)/$(ex))
SOLUTIONS = $(foreach ex,$(EXERCISES),exercises/$(ex)/solution)

.PHONY: all clean test help $(EXERCISES)

all: $(BINARIES)

solutions: $(SOLUTIONS)

# Build each exercise
exercises/%/% : exercises/%/%.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# Build solutions (from solution.c)
exercises/%/solution : exercises/%/solution.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# Individual exercise targets
01_basics: exercises/01_basics/01_basics

02_mutex: exercises/02_mutex/02_mutex

03_race_condition: exercises/03_race_condition/03_race_condition

04_producer_consumer: exercises/04_producer_consumer/04_producer_consumer

05_deadlock: exercises/05_deadlock/05_deadlock

# Run individual exercises
run-01: exercises/01_basics/01_basics
	@echo "\n=== Running Exercise 01: Basics ==="
	@./exercises/01_basics/01_basics

run-02: exercises/02_mutex/02_mutex
	@echo "\n=== Running Exercise 02: Mutex ==="
	@./exercises/02_mutex/02_mutex

run-03: exercises/03_race_condition/03_race_condition
	@echo "\n=== Running Exercise 03: Race Condition ==="
	@./exercises/03_race_condition/03_race_condition

run-04: exercises/04_producer_consumer/04_producer_consumer
	@echo "\n=== Running Exercise 04: Producer-Consumer ==="
	@./exercises/04_producer_consumer/04_producer_consumer

run-05: exercises/05_deadlock/05_deadlock
	@echo "\n=== Running Exercise 05: Deadlock ==="
	@timeout 3 ./exercises/05_deadlock/05_deadlock || echo "(timed out - expected if deadlock exists)"

# Test all exercises
test: all
	@echo "Running all tests..."
	@bash tests/run_tests.sh

clean:
	rm -f $(BINARIES) $(SOLUTIONS)
	@echo "Cleaned all binaries"

help:
	@echo "Multithreading Workshop Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  make all       - Build all exercises"
	@echo "  make test      - Run all tests"
	@echo "  make run-01    - Run exercise 01"
	@echo "  make run-02    - Run exercise 02"
	@echo "  make clean     - Remove all binaries"
	@echo "  make solutions - Build all solution files"
