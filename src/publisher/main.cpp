// ============================================================================
// PROCESS A: MARKET DATA PUBLISHER
// ============================================================================
// This is the main entry point for the publisher process.
// It will:
//   1. Generate random market data
//   2. Send JSON messages over TCP (for Process C)
//   3. Push data to shared memory ring buffer (for Process B)
//
// For now, this is a placeholder to verify our build system works.

#include "common/market_data.hpp"
#include <fmt/chrono.h> // For timestamp formatting
#include <fmt/core.h>   // For fmt::print (fast, type-safe printing)

int main() {
  // fmt::print is like printf, but type-safe and faster
  // {} is a placeholder, like %s or %d in printf
  fmt::print("===========================================\n");
  fmt::print("   HFT Market Data Publisher (Process A)\n");
  fmt::print("===========================================\n\n");

  // Create a sample MarketData to test our struct
  hft::MarketData quote("RELIANCE", 2850.25, 2850.75, 1234567890123);

  // Test JSON serialization
  std::string json = quote.to_json();
  fmt::print("Sample market data:\n");
  fmt::print("{}\n\n", json);

  // Test JSON deserialization
  hft::MarketData parsed;
  if (hft::MarketData::from_json(json, parsed)) {
    fmt::print("Successfully parsed JSON back to struct:\n");
    fmt::print("  Instrument: {}\n", parsed.instrument);
    fmt::print("  Bid: {:.2f}\n", parsed.bid); // .2f = 2 decimal places
    fmt::print("  Ask: {:.2f}\n", parsed.ask);
    fmt::print("  Timestamp: {} ns\n", parsed.timestamp_ns);
  } else {
    fmt::print("ERROR: Failed to parse JSON\n");
    return 1;
  }

  fmt::print("\n[Phase 1 Complete] Build system working!\n");
  fmt::print("Next: Implement TCP server in Phase 2\n");

  return 0;
}
