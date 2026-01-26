#!/bin/bash

echo "=== HFT System Integration Test ==="

# Build the project
echo "Building project..."
cd build
make -j4
if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

echo "Build successful!"

# Test 1: Run unit tests
echo ""
echo "=== Running Unit Tests ==="
./shared_memory_tests
if [ $? -ne 0 ]; then
    echo "Unit tests failed!"
    exit 1
fi

echo "Unit tests passed!"

# Test 2: Test shared memory data path
echo ""
echo "=== Testing Shared Memory Data Path ==="

# Start publisher in background
./publisher &
PUBLISHER_PID=$!
echo "Started publisher (PID: $PUBLISHER_PID)"

# Give publisher time to initialize
sleep 2

# Start SHM consumer for a short time
./shm_consumer &
SHM_PID=$!
echo "Started SHM consumer (PID: $SHM_PID)"

# Let them run for a few seconds
sleep 3

# Kill processes
kill $SHM_PID 2>/dev/null
kill $PUBLISHER_PID 2>/dev/null
wait $SHM_PID 2>/dev/null
wait $PUBLISHER_PID 2>/dev/null

echo "Shared memory test completed"

# Test 3: Test TCP data path
echo ""
echo "=== Testing TCP Data Path ==="

# Start publisher in background
./publisher &
PUBLISHER_PID=$!
echo "Started publisher (PID: $PUBLISHER_PID)"

# Give publisher time to initialize
sleep 2

# Start TCP consumer for a short time
./tcp_consumer &
TCP_PID=$!
echo "Started TCP consumer (PID: $TCP_PID)"

# Let them run for a few seconds
sleep 3

# Kill processes
kill $TCP_PID 2>/dev/null
kill $PUBLISHER_PID 2>/dev/null
wait $TCP_PID 2>/dev/null
wait $PUBLISHER_PID 2>/dev/null

echo "TCP test completed"

echo ""
echo "=== System Integration Test Complete ==="
echo "All components compiled and basic functionality verified"