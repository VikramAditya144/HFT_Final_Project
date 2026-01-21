#pragma once

// ============================================================================
// FAST CLOCK IMPLEMENTATION
// ============================================================================
// This header implements a high-performance timestamp mechanism that avoids
// syscall overhead in the hot path by using a background thread to periodically
// update a cached timestamp.

#include <atomic>
#include <chrono>
#include <thread>

namespace hft {

// ============================================================================
// FastClock Class
// ============================================================================
//
// DESIGN RATIONALE:
//   - In HFT systems, calling gettimeofday() or clock_gettime() in the hot path
//     is too expensive due to syscall overhead
//   - This class uses a background thread to periodically update a cached
//     timestamp every 200ms
//   - The hot path simply reads an atomic variable (no syscalls!)
//   - Trade-off: Slightly less precision for much better performance
//
// MEMORY ORDERING:
//   - Uses memory_order_relaxed for timestamp reads/writes since we don't need
//     synchronization with other memory operations
//   - The timestamp is just a single value, no complex ordering needed
//
// THREAD SAFETY:
//   - All operations are thread-safe through atomic operations
//   - Multiple threads can safely call now() concurrently
//   - Only one background thread updates the cached time
//

class FastClock {
private:
    // Cached timestamp in nanoseconds since epoch
    // Using relaxed memory ordering since we only care about the timestamp value
    std::atomic<int64_t> cached_time_ns{0};
    
    // Background thread that updates the cached timestamp
    std::thread update_thread;
    
    // Flag to control the background thread lifecycle
    std::atomic<bool> running{false};
    
    // Background thread function that updates cached_time_ns every 200ms
    void update_loop() {
        while (running.load(std::memory_order_relaxed)) {
            // Get current time using high_resolution_clock
            auto now_time = std::chrono::high_resolution_clock::now();
            
            // Convert to nanoseconds since epoch
            auto ns_since_epoch = now_time.time_since_epoch();
            int64_t ns_count = std::chrono::duration_cast<std::chrono::nanoseconds>(ns_since_epoch).count();
            
            // Update the cached timestamp
            cached_time_ns.store(ns_count, std::memory_order_relaxed);
            
            // Sleep for 200ms before next update
            // This balances accuracy with CPU overhead
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }

public:
    // ========================================================================
    // CONSTRUCTOR
    // ========================================================================
    FastClock() {
        // Initialize with current time
        auto now_time = std::chrono::high_resolution_clock::now();
        auto ns_since_epoch = now_time.time_since_epoch();
        int64_t ns_count = std::chrono::duration_cast<std::chrono::nanoseconds>(ns_since_epoch).count();
        cached_time_ns.store(ns_count, std::memory_order_relaxed);
        
        // Start the background update thread
        running.store(true, std::memory_order_relaxed);
        update_thread = std::thread(&FastClock::update_loop, this);
    }

    // ========================================================================
    // DESTRUCTOR
    // ========================================================================
    ~FastClock() {
        // Signal the background thread to stop
        running.store(false, std::memory_order_relaxed);
        
        // Wait for the background thread to finish
        if (update_thread.joinable()) {
            update_thread.join();
        }
    }

    // ========================================================================
    // COPY/MOVE SEMANTICS
    // ========================================================================
    // FastClock is not copyable or movable due to the background thread
    FastClock(const FastClock&) = delete;
    FastClock& operator=(const FastClock&) = delete;
    FastClock(FastClock&&) = delete;
    FastClock& operator=(FastClock&&) = delete;

    // ========================================================================
    // PUBLIC INTERFACE
    // ========================================================================
    
    // Get current timestamp in nanoseconds since epoch
    // This is the HOT PATH function - no syscalls, just atomic read!
    [[nodiscard]] int64_t now() const noexcept {
        return cached_time_ns.load(std::memory_order_relaxed);
    }
    
    // Check if the background thread is running
    [[nodiscard]] bool is_running() const noexcept {
        return running.load(std::memory_order_relaxed);
    }
    
    // Get the update frequency in milliseconds (always 200ms)
    [[nodiscard]] static constexpr int get_update_frequency_ms() noexcept {
        return 200;
    }
};

} // namespace hft