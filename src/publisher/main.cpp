// ============================================================================
// PROCESS A: MARKET DATA PUBLISHER
// ============================================================================
// This is the main entry point for the publisher process.
// It will:
//   1. Initialize shared memory and ring buffer
//   2. Generate random market data in a loop
//   3. Push data to shared memory ring buffer (for Process B)
//   4. Later: Send JSON messages over TCP (for Process C)

#include "common/market_data.hpp"
#include "common/shared_memory.hpp"
#include "common/ring_buffer.hpp"
#include "common/fast_clock.hpp"
#include <fmt/chrono.h> // For timestamp formatting
#include <fmt/core.h>   // For fmt::print (fast, type-safe printing)
#include <random>
#include <thread>
#include <chrono>
#include <vector>
#include <string>

int main() {
  fmt::print("===========================================\n");
  fmt::print("   HFT Market Data Publisher (Process A)\n");
  fmt::print("===========================================\n\n");

  try {
    // ========================================================================
    // STEP 1: Initialize Fast Clock for timestamping
    // ========================================================================
    fmt::print("Initializing Fast Clock...\n");
    hft::FastClock fast_clock;
    
    // ========================================================================
    // STEP 2: Initialize Shared Memory and Ring Buffer
    // ========================================================================
    fmt::print("Creating shared memory segment...\n");
    
    // Calculate size needed for ring buffer
    constexpr size_t ring_buffer_size = sizeof(hft::RingBuffer);
    
    // Create shared memory segment
    hft::SharedMemoryManager shm_manager("hft_market_data", ring_buffer_size, true);
    
    if (!shm_manager.is_valid()) {
      fmt::print("ERROR: Failed to create shared memory\n");
      return 1;
    }
    
    fmt::print("Shared memory created successfully (size: {} bytes)\n", ring_buffer_size);
    
    // Get pointer to shared memory and construct ring buffer in-place
    void* shm_addr = shm_manager.get_address();
    hft::RingBuffer* ring_buffer = new(shm_addr) hft::RingBuffer();
    
    fmt::print("Ring buffer initialized in shared memory\n");
    
    // ========================================================================
    // STEP 3: Prepare Market Data Generation
    // ========================================================================
    fmt::print("Setting up market data generation...\n");
    
    // Random number generator for realistic market data
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> price_dist(100.0, 3000.0);
    std::uniform_real_distribution<double> spread_dist(0.01, 1.0);
    
    // Sample instrument names for variety
    std::vector<std::string> instruments = {
      "RELIANCE", "TCS", "INFY", "HDFC", "ICICI", "SBI", "ITC", "HIND_UNILEVER",
      "BHARTI_AIRTEL", "KOTAK_BANK", "AXIS_BANK", "MARUTI", "ASIAN_PAINTS",
      "BAJAJ_FINANCE", "WIPRO", "ONGC", "NTPC", "POWERGRID", "ULTRACEMCO",
      "NESTLEIND", "HCLTECH", "TITAN", "SUNPHARMA", "DRREDDY", "CIPLA",
      "TECHM", "INDUSINDBK", "BAJAJ_AUTO", "HEROMOTOCO", "EICHERMOT",
      "GRASIM", "ADANIPORTS", "JSWSTEEL", "HINDALCO", "TATASTEEL",
      "COALINDIA", "BPCL", "IOC", "DIVISLAB", "BRITANNIA", "DABUR",
      "GODREJCP", "MARICO", "PIDILITIND", "COLPAL", "MCDOWELL_N",
      "AMBUJACEM", "ACC", "SHREECEM", "RAMCOCEM", "INDIACEM"
    };
    
    fmt::print("Prepared {} instrument symbols\n", instruments.size());
    
    // ========================================================================
    // STEP 4: Market Data Generation Loop
    // ========================================================================
    fmt::print("\nStarting market data generation loop...\n");
    fmt::print("Press Ctrl+C to stop\n\n");
    
    size_t message_count = 0;
    size_t overflow_count = 0;
    
    while (true) {
      // Generate random market data
      const std::string& instrument = instruments[gen() % instruments.size()];
      double bid = price_dist(gen);
      double spread = spread_dist(gen);
      double ask = bid + spread;
      int64_t timestamp = fast_clock.now();
      
      // Create market data message
      hft::MarketData market_data(instrument.c_str(), bid, ask, timestamp);
      
      // Try to write to ring buffer
      if (ring_buffer->try_write(market_data)) {
        message_count++;
        
        // Print status every 100 messages
        if (message_count % 100 == 0) {
          fmt::print("Generated {} messages | Buffer usage: {}/{} | Overflows: {}\n",
                    message_count, 
                    ring_buffer->available_for_read(),
                    ring_buffer->capacity(),
                    overflow_count);
        }
      } else {
        overflow_count++;
        // Buffer is full, skip this message
        if (overflow_count % 10 == 1) {
          fmt::print("WARNING: Ring buffer full, dropped message (total drops: {})\n", overflow_count);
        }
      }
      
      // Generate messages at ~1000 Hz (1ms interval)
      std::this_thread::sleep_for(std::chrono::microseconds(1000));
      
      // Stop after generating 1000+ messages for this basic implementation
      if (message_count >= 1000) {
        fmt::print("\nGenerated {} messages successfully!\n", message_count);
        fmt::print("Ring buffer final state: {}/{} messages\n", 
                  ring_buffer->available_for_read(), ring_buffer->capacity());
        break;
      }
    }
    
    fmt::print("\n[Task 6.1 Complete] Basic publisher with market data generation working!\n");
    fmt::print("Next: Add TCP server functionality\n");
    
  } catch (const std::exception& e) {
    fmt::print("ERROR: {}\n", e.what());
    return 1;
  }

  return 0;
}
