#!/bin/bash

PASSED=0
FAILED=0

echo "================================"
echo "  Multithreading Workshop Tests"
echo "================================"
echo ""

# Test Exercise 01
echo "[TEST 01] Thread Basics..."
if ./exercises/01_basics/01_basics 2>/dev/null | grep -q "All threads completed"; then
    echo "✓ PASS: Exercise 01"
    ((PASSED++))
else
    echo "✗ FAIL: Exercise 01 - Check that threads are created and joined"
    ((FAILED++))
fi
echo ""

# Test Exercise 02
echo "[TEST 02] Mutex Protection..."
if ./exercises/02_mutex/02_mutex 2>/dev/null | grep -q "Success! Counter is correct"; then
    echo "✓ PASS: Exercise 02"
    ((PASSED++))
else
    echo "✗ FAIL: Exercise 02 - Add mutex protection"
    ((FAILED++))
fi
echo ""

# Test Exercise 03
echo "[TEST 03] Race Condition Fix..."
OUTPUT=$(./exercises/03_race_condition/03_race_condition 2>/dev/null)
if echo "$OUTPUT" | grep -q "Success! Balance is correct"; then
    echo "✓ PASS: Exercise 03"
    ((PASSED++))
else
    echo "✗ FAIL: Exercise 03 - Fix the race condition with proper locking"
    ((FAILED++))
fi
echo ""

# Test Exercise 04
echo "[TEST 04] Producer-Consumer..."
if timeout 5 ./exercises/04_producer_consumer/04_producer_consumer 2>/dev/null | grep -q "All items produced and consumed successfully"; then
    echo "✓ PASS: Exercise 04"
    ((PASSED++))
else
    echo "✗ FAIL: Exercise 04 - Implement condition variables correctly"
    ((FAILED++))
fi
echo ""

# Test Exercise 05
echo "[TEST 05] Deadlock Prevention..."
if timeout 5 ./exercises/05_deadlock/05_deadlock 2>/dev/null | grep -q "All transfers completed successfully"; then
    echo "✓ PASS: Exercise 05"
    ((PASSED++))
else
    echo "✗ FAIL: Exercise 05 - Fix the deadlock (timed out or failed)"
    ((FAILED++))
fi
echo ""

echo "================================"
echo "  Results: $PASSED passed, $FAILED failed"
echo "================================"

if [ $FAILED -eq 0 ]; then
    echo "🎉 All tests passed! Great job!"
    exit 0
else
    echo "⚠️  Some tests failed. Keep working on them!"
    exit 1
fi
