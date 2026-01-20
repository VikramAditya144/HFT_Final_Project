// ============================================================================
// PROCESS C: TCP CONSUMER
// ============================================================================
// This process connects to the publisher over TCP and receives market data.
// TCP is slower than shared memory, but:
//   - Works across different machines
//   - Handles network errors gracefully
//   - Is the standard way exchanges deliver data
//
// For now, this is a placeholder.

#include "common/market_data.hpp"
#include <fmt/core.h>

int main() {
  fmt::print("===========================================\n");
  fmt::print("   HFT TCP Consumer (Process C)\n");
  fmt::print("===========================================\n\n");

  fmt::print("[Placeholder] Will connect to TCP server.\n");
  fmt::print("Implementation coming in Phase 3.\n");

  return 0;
}
