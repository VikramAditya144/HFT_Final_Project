// ============================================================================
// PROCESS C: TCP CONSUMER
// ============================================================================
// This process connects to the publisher over TCP and receives market data.
// TCP is slower than shared memory, but:
//   - Works across different machines
//   - Handles network errors gracefully
//   - Is the standard way exchanges deliver data

#include "common/market_data.hpp"
#include <fmt/core.h>
#include <fmt/chrono.h>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <chrono>

int main() {
  fmt::print("===========================================\n");
  fmt::print("   HFT TCP Consumer (Process C)\n");
  fmt::print("===========================================\n\n");

  try {
    // ========================================================================
    // STEP 1: Initialize Boost.Asio and connect to publisher
    // ========================================================================
    fmt::print("Connecting to publisher at 127.0.0.1:9000...\n");
    
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::socket socket(io_context);
    boost::asio::ip::tcp::resolver resolver(io_context);
    
    // Resolve the endpoint
    auto endpoints = resolver.resolve("127.0.0.1", "9000");
    
    // Connect to the publisher
    boost::asio::connect(socket, endpoints);
    
    fmt::print("Successfully connected to publisher!\n\n");
    
    // ========================================================================
    // STEP 2: Message receiving and parsing loop
    // ========================================================================
    fmt::print("Starting message receiving and parsing loop...\n");
    fmt::print("Press Ctrl+C to stop\n\n");
    
    size_t message_count = 0;
    size_t parse_errors = 0;
    boost::asio::streambuf buffer;
    
    while (true) {
      try {
        // Read until newline (message boundary)
        // This handles partial reads automatically - boost::asio::read_until
        // will keep reading until it finds the delimiter
        boost::asio::read_until(socket, buffer, '\n');
        
        // Extract the line from buffer
        std::istream stream(&buffer);
        std::string json_line;
        std::getline(stream, json_line);
        
        if (!json_line.empty()) {
          message_count++;
          
          // Record receive timestamp for latency calculation
          auto receive_time = std::chrono::high_resolution_clock::now();
          auto receive_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
              receive_time.time_since_epoch()).count();
          
          // ================================================================
          // STEP 3: Parse JSON message
          // ================================================================
          hft::MarketData market_data;
          bool parse_success = hft::MarketData::from_json(json_line, market_data);
          
          if (parse_success) {
            // Calculate latency (receive_time - message_timestamp)
            int64_t latency_ns = receive_time_ns - market_data.timestamp_ns;
            double latency_us = latency_ns / 1000.0; // Convert to microseconds
            
            // ============================================================
            // STEP 4: Structured logging with fmt
            // ============================================================
            fmt::print("MSG #{:4d} | {} | BID: {:8.2f} | ASK: {:8.2f} | LATENCY: {:8.2f}μs\n",
                      message_count,
                      market_data.instrument,
                      market_data.bid,
                      market_data.ask,
                      latency_us);
            
            // Every 10 messages, print a summary
            if (message_count % 10 == 0) {
              fmt::print("--- Processed {} messages, {} parse errors ---\n", 
                        message_count, parse_errors);
            }
            
          } else {
            parse_errors++;
            fmt::print("ERROR: Failed to parse JSON message #{}: {}\n", 
                      message_count, json_line);
          }
          
          // Stop after receiving some messages for this implementation
          if (message_count >= 50) {
            fmt::print("\nReceived and parsed {} messages successfully!\n", message_count);
            fmt::print("Parse errors: {}\n", parse_errors);
            break;
          }
        }
        
      } catch (const boost::system::system_error& e) {
        if (e.code() == boost::asio::error::eof) {
          fmt::print("Publisher disconnected (EOF)\n");
          break;
        } else if (e.code() == boost::asio::error::connection_reset) {
          fmt::print("Connection reset by publisher\n");
          break;
        } else {
          fmt::print("Network error: {} ({})\n", e.what(), e.code().value());
          break;
        }
      } catch (const std::exception& e) {
        fmt::print("Unexpected error: {}\n", e.what());
        break;
      }
    }
    
    fmt::print("\n[Task 8.2 Complete] JSON parsing and structured logging working!\n");
    fmt::print("Next: Add property tests for TCP consumer\n");
    
  } catch (const std::exception& e) {
    fmt::print("ERROR: {}\n", e.what());
    return 1;
  }

  return 0;
}
