// ============================================================================
// PROCESS B: SHARED MEMORY CONSUMER
// ============================================================================
// This process reads market data from shared memory (ring buffer).
// Shared memory is MUCH faster than TCP because:
//   - No kernel overhead (no syscalls for each message)
//   - No data copying (both processes see the same memory)
//   - No network stack overhead
//
// This consumer attaches to the existing shared memory segment created by
// the publisher and polls the ring buffer for new messages using spin-wait.

#include "common/market_data.hpp"
#include "common/shared_memory.hpp"
#include "common/ring_buffer.hpp"
#include "common/fast_clock.hpp"
#include <fmt/chrono.h>
#include <fmt/core.h>
#include <chrono>
#include <thread>

int main() {
  fmt::print("===========================================\n");
  fmt::print("   HFT Shared Memory Consumer (Process B)\n");
  fmt::print("===========================================\n\n");

  try {
    // ========================================================================
    // STEP 1: Initialize Fast Clock for latency measurement
    // ========================================================================
    fmt::print("Initializing Fast Clock for latency measurement...\n");
    hft::FastClock fast_clock;
    
    // ========================================================================
    // STEP 2: Attach to existing shared memory segment
    // ========================================================================
    fmt::print("Attaching to shared memory segment 'hft_market_data'...\n");
    
    // Calculate size needed for ring buffer (same as publisher)
    constexpr size_t ring_buffer_size = sizeof(hft::RingBuffer);
    
    // Attach to existing shared memory segment in read-only mode
    hft::SharedMemoryManager shm_manager("hft_market_data", ring_buffer_size, false);
    
    if (!shm_manager.is_valid()) {
      fmt::print("ERROR: Failed to attach to shared memory segment.\n");
      fmt::print("Make sure the publisher (Process A) is running first.\n");
      return 1;
    }
    
    fmt::print("Successfully attached to shared memory (size: {} bytes)\n", ring_buffer_size);
    
    // Get pointer to shared memory and cast to ring buffer
    void* shm_addr = shm_manager.get_address();
    hft::RingBuffer* ring_buffer = static_cast<hft::RingBuffer*>(shm_addr);
    
    fmt::print("Ring buffer attached successfully\n");
    
    // ========================================================================
    // STEP 3: Basic ring buffer polling loop
    // ========================================================================
    fmt::print("\nStarting ring buffer polling loop...\n");
    fmt::print("Waiting for market data from publisher...\n");
    fmt::print("Press Ctrl+C to stop\n\n");
    
    size_t message_count = 0;
    size_t empty_polls = 0;
    int64_t total_latency_ns = 0;
    int64_t min_latency_ns = INT64_MAX;
    int64_t max_latency_ns = 0;
    
    // Polling loop with spin-wait
    while (true) {
      hft::MarketData market_data;
      
      // Try to read from ring buffer
      if (ring_buffer->try_read(market_data)) {
        // Calculate latency
        int64_t receive_time = fast_clock.now();
        int64_t latency_ns = receive_time - market_data.timestamp_ns;
        
        // Update latency statistics
        total_latency_ns += latency_ns;
        min_latency_ns = std::min(min_latency_ns, latency_ns);
        max_latency_ns = std::max(max_latency_ns, latency_ns);
        
        message_count++;
        
        // Log received message with latency
        if (message_count % 100 == 1 || message_count <= 10) {
          fmt::print("Received: {} | Bid: {:.2f} | Ask: {:.2f} | Latency: {:.3f}μs\n",
                    market_data.instrument,
                    market_data.bid,
                    market_data.ask,
                    latency_ns / 1000.0); // Convert to microseconds
        }
        
        // Print statistics every 100 messages
        if (message_count % 100 == 0) {
          double avg_latency_us = (total_latency_ns / message_count) / 1000.0;
          double min_latency_us = min_latency_ns / 1000.0;
          double max_latency_us = max_latency_ns / 1000.0;
          
          fmt::print("\n--- Statistics after {} messages ---\n", message_count);
          fmt::print("Average latency: {:.3f}μs\n", avg_latency_us);
          fmt::print("Min latency: {:.3f}μs\n", min_latency_us);
          fmt::print("Max latency: {:.3f}μs\n", max_latency_us);
          fmt::print("Empty polls: {}\n", empty_polls);
          fmt::print("Buffer available: {}/{}\n", 
                    ring_buffer->available_for_read(), ring_buffer->capacity());
          fmt::print("----------------------------------------\n\n");
        }
        
        // Reset empty poll counter when we successfully read
        empty_polls = 0;
        
      } else {
        // Buffer is empty, handle underflow condition
        empty_polls++;
        
        // Avoid busy-waiting by adding a small delay when buffer is consistently empty
        if (empty_polls > 1000) {
          // After many empty polls, add a tiny sleep to reduce CPU usage
          std::this_thread::sleep_for(std::chrono::microseconds(1));
          
          // Reset counter to avoid overflow
          if (empty_polls > 10000) {
            empty_polls = 1000;
          }
        }
        
        // Print status occasionally when waiting
        if (empty_polls % 100000 == 0) {
          fmt::print("Waiting for data... (empty polls: {})\n", empty_polls);
        }
      }
      
      // Exit condition for testing - stop after processing some messages
      if (message_count >= 1000) {
        fmt::print("\nProcessed {} messages successfully!\n", message_count);
        
        // Final statistics
        if (message_count > 0) {
          double avg_latency_us = (total_latency_ns / message_count) / 1000.0;
          double min_latency_us = min_latency_ns / 1000.0;
          double max_latency_us = max_latency_ns / 1000.0;
          
          fmt::print("\n=== Final Latency Statistics ===\n");
          fmt::print("Messages processed: {}\n", message_count);
          fmt::print("Average latency: {:.3f}μs\n", avg_latency_us);
          fmt::print("Min latency: {:.3f}μs\n", min_latency_us);
          fmt::print("Max latency: {:.3f}μs\n", max_latency_us);
          fmt::print("Total empty polls: {}\n", empty_polls);
          fmt::print("================================\n");
        }
        break;
      }
    }
    
    fmt::print("\n[Task 9.1 Complete] Shared memory consumer with polling working!\n");
    fmt::print("Next: Add property tests for SHM consumer polling\n");
    
  } catch (const std::exception& e) {
    fmt::print("ERROR: {}\n", e.what());
    return 1;
  }

  return 0;
}
