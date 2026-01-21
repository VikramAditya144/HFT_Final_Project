#pragma once

// ============================================================================
// LOCK-FREE RING BUFFER IMPLEMENTATION
// ============================================================================
// This header implements a Single Producer Single Consumer (SPSC) lock-free
// ring buffer optimized for high-frequency trading applications.

#include <atomic>
#include <cstddef>
#include "market_data.hpp"

namespace hft {

// ============================================================================
// CONSTANTS
// ============================================================================

// Buffer size must be a power of 2 for efficient modulo operations
// Using 1024 elements provides good balance between memory usage and capacity
constexpr size_t RING_BUFFER_SIZE = 1024;

// Compile-time check that buffer size is a power of 2
static_assert((RING_BUFFER_SIZE & (RING_BUFFER_SIZE - 1)) == 0, 
              "RING_BUFFER_SIZE must be a power of 2");

// ============================================================================
// RingBuffer Class
// ============================================================================
//
// DESIGN PRINCIPLES:
//   - Single Producer Single Consumer (SPSC) for maximum performance
//   - Lock-free using atomic operations with acquire-release semantics
//   - 64-byte alignment to prevent false sharing between cache lines
//   - Power-of-2 buffer size for efficient modulo operations using bitwise AND
//
// MEMORY LAYOUT:
//   - write_idx and read_idx are on separate cache lines (64-byte aligned)
//   - This prevents false sharing when producer updates write_idx and
//     consumer updates read_idx simultaneously
//
// MEMORY ORDERING:
//   - Producer uses memory_order_release on write_idx to ensure all data
//     writes are visible before the index update
//   - Consumer uses memory_order_acquire on write_idx to see producer's writes
//   - Consumer uses memory_order_release on read_idx for consistency
//
// ALGORITHM:
//   - Empty condition: read_idx == write_idx
//   - Full condition: (write_idx + 1) % BUFFER_SIZE == read_idx
//   - Available space: (read_idx - write_idx - 1) % BUFFER_SIZE
//

class alignas(64) RingBuffer {
private:
    // Producer's write index (aligned to separate cache line)
    alignas(64) std::atomic<size_t> write_idx{0};
    
    // Consumer's read index (aligned to separate cache line)
    alignas(64) std::atomic<size_t> read_idx{0};
    
    // The actual data buffer
    // Each MarketData is 64-byte aligned for optimal cache performance
    MarketData buffer[RING_BUFFER_SIZE];

public:
    // ========================================================================
    // CONSTRUCTOR
    // ========================================================================
    RingBuffer() = default;

    // ========================================================================
    // COPY/MOVE SEMANTICS
    // ========================================================================
    // RingBuffer is not copyable or movable due to atomic members
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;
    RingBuffer(RingBuffer&&) = delete;
    RingBuffer& operator=(RingBuffer&&) = delete;

    // ========================================================================
    // PRODUCER INTERFACE (Single Producer)
    // ========================================================================
    
    // Try to write a market data item to the buffer
    // Returns true on success, false if buffer is full
    [[nodiscard]] bool try_write(const MarketData& data) noexcept {
        const size_t current_write = write_idx.load(std::memory_order_relaxed);
        const size_t next_write = (current_write + 1) & (RING_BUFFER_SIZE - 1);
        
        // Check if buffer is full
        // We need to leave one slot empty to distinguish between full and empty
        const size_t current_read = read_idx.load(std::memory_order_acquire);
        if (next_write == current_read) {
            return false; // Buffer is full
        }
        
        // Write the data to the buffer
        buffer[current_write] = data;
        
        // Update write index with release semantics
        // This ensures the data write is visible before the index update
        write_idx.store(next_write, std::memory_order_release);
        
        return true;
    }
    
    // Check if the buffer is full (from producer's perspective)
    [[nodiscard]] bool is_full() const noexcept {
        const size_t current_write = write_idx.load(std::memory_order_relaxed);
        const size_t next_write = (current_write + 1) & (RING_BUFFER_SIZE - 1);
        const size_t current_read = read_idx.load(std::memory_order_acquire);
        return next_write == current_read;
    }
    
    // Get the number of available slots for writing
    [[nodiscard]] size_t available_for_write() const noexcept {
        const size_t current_write = write_idx.load(std::memory_order_relaxed);
        const size_t current_read = read_idx.load(std::memory_order_acquire);
        
        // Calculate available space, accounting for the one slot we keep empty
        return (current_read - current_write - 1) & (RING_BUFFER_SIZE - 1);
    }

    // ========================================================================
    // CONSUMER INTERFACE (Single Consumer)
    // ========================================================================
    
    // Try to read a market data item from the buffer
    // Returns true on success, false if buffer is empty
    [[nodiscard]] bool try_read(MarketData& data) noexcept {
        const size_t current_read = read_idx.load(std::memory_order_relaxed);
        const size_t current_write = write_idx.load(std::memory_order_acquire);
        
        // Check if buffer is empty
        if (current_read == current_write) {
            return false; // Buffer is empty
        }
        
        // Read the data from the buffer
        data = buffer[current_read];
        
        // Update read index with release semantics
        const size_t next_read = (current_read + 1) & (RING_BUFFER_SIZE - 1);
        read_idx.store(next_read, std::memory_order_release);
        
        return true;
    }
    
    // Check if the buffer is empty (from consumer's perspective)
    [[nodiscard]] bool is_empty() const noexcept {
        const size_t current_read = read_idx.load(std::memory_order_relaxed);
        const size_t current_write = write_idx.load(std::memory_order_acquire);
        return current_read == current_write;
    }
    
    // Get the number of items available for reading
    [[nodiscard]] size_t available_for_read() const noexcept {
        const size_t current_read = read_idx.load(std::memory_order_relaxed);
        const size_t current_write = write_idx.load(std::memory_order_acquire);
        return (current_write - current_read) & (RING_BUFFER_SIZE - 1);
    }

    // ========================================================================
    // UTILITY FUNCTIONS
    // ========================================================================
    
    // Get the buffer capacity (total number of slots minus one for full/empty distinction)
    [[nodiscard]] static constexpr size_t capacity() noexcept {
        return RING_BUFFER_SIZE - 1;
    }
    
    // Get the raw buffer size
    [[nodiscard]] static constexpr size_t buffer_size() noexcept {
        return RING_BUFFER_SIZE;
    }
    
    // Get current write index (for debugging/monitoring)
    [[nodiscard]] size_t get_write_index() const noexcept {
        return write_idx.load(std::memory_order_relaxed);
    }
    
    // Get current read index (for debugging/monitoring)
    [[nodiscard]] size_t get_read_index() const noexcept {
        return read_idx.load(std::memory_order_relaxed);
    }
};

// ============================================================================
// COMPILE-TIME CHECKS
// ============================================================================

// Verify alignment requirements
static_assert(alignof(RingBuffer) == 64, 
              "RingBuffer should be aligned to 64-byte boundaries");

// Verify buffer size is reasonable
static_assert(RING_BUFFER_SIZE >= 64, 
              "Buffer size should be at least 64 for reasonable capacity");

static_assert(RING_BUFFER_SIZE <= 65536, 
              "Buffer size should not exceed 64K to avoid excessive memory usage");

} // namespace hft