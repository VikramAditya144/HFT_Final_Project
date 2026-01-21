#pragma once

// ============================================================================
// MARKET DATA STRUCTURE
// ============================================================================
// This header defines the core data structure that represents a market quote.
// Every message flowing through our system (TCP and Shared Memory) uses this
// structure.

#include <cstdint>           // For int64_t (fixed-size integer types)
#include <cstring>           // For strncpy (safe string copy)
#include <nlohmann/json.hpp> // For JSON serialization
#include <string>            // For std::string

namespace hft {

// ============================================================================
// CONSTANTS
// ============================================================================

// Maximum length for instrument names (e.g., "RELIANCE", "AAPL")
// We use 16 bytes because:
//   1. Most stock symbols are < 10 characters
//   2. 16 is a power of 2 (good for alignment)
//   3. Fixed size = no heap allocation = faster!
constexpr size_t INSTRUMENT_MAX_LEN = 16;

// ============================================================================
// MarketData Structure
// ============================================================================
//
// MEMORY LAYOUT (on 64-bit systems with 64-byte alignment):
//
//   Offset    Size    Field
//   ------    ----    -----
//   0         16      instrument[16]  (char array)
//   16        8       bid             (double)
//   24        8       ask             (double)
//   32        8       timestamp_ns    (int64_t)
//   40        24      padding         (explicit padding to 64 bytes)
//   ------
//   Total:    64 bytes (cache-line aligned)
//
// WHY 64-BYTE ALIGNMENT?
//   - Modern CPUs have 64-byte cache lines
//   - Aligning to cache line boundaries prevents "false sharing"
//   - False sharing occurs when two threads access different variables
//     that happen to be on the same cache line, causing unnecessary
//     cache coherency traffic
//   - In HFT systems, this alignment is critical for performance
//
// WHY FIXED-SIZE ARRAYS INSTEAD OF std::string?
//   - std::string allocates memory on the HEAP
//   - Heap allocation is SLOW (malloc/free overhead)
//   - In HFT, we want STACK allocation or pre-allocated buffers
//   - Fixed size = predictable memory layout = cache-friendly
//
// WHY double FOR PRICES?
//   - Prices like 2850.75 need decimal precision
//   - double gives us ~15 decimal digits of precision
//   - In production, some systems use fixed-point integers for speed
//     (e.g., price * 10000 stored as int64_t)
//

struct alignas(64) MarketData {
  // Instrument symbol (e.g., "RELIANCE", "AAPL", "GOOG")
  // Fixed 16-byte array, null-terminated
  char instrument[INSTRUMENT_MAX_LEN];

  // Best bid price (highest price someone is willing to buy at)
  double bid;

  // Best ask price (lowest price someone is willing to sell at)
  double ask;

  // Timestamp in nanoseconds since epoch
  // int64_t can hold dates until year 2262 in nanoseconds!
  // We use nanoseconds because in HFT, microseconds aren't precise enough
  int64_t timestamp_ns;

  // Explicit padding to ensure 64-byte alignment
  // 64 - (16 + 8 + 8 + 8) = 24 bytes of padding needed
  char padding[24];

  // ========================================================================
  // CONSTRUCTORS
  // ========================================================================

  // Default constructor - zero-initialize everything
  // This is important! Uninitialized memory can cause undefined behavior
  MarketData() : bid(0.0), ask(0.0), timestamp_ns(0) {
    // Fill instrument with zeros (null bytes)
    std::memset(instrument, 0, INSTRUMENT_MAX_LEN);
    // Initialize padding to zero for consistent memory layout
    std::memset(padding, 0, sizeof(padding));
  }

  // Parameterized constructor for convenience
  // We use const char* instead of std::string to avoid heap allocation
  MarketData(const char *inst, double b, double a, int64_t ts)
      : bid(b), ask(a), timestamp_ns(ts) {
    // strncpy copies at most N-1 characters and null-terminates
    // This prevents buffer overflow if 'inst' is too long
    std::strncpy(instrument, inst, INSTRUMENT_MAX_LEN - 1);
    instrument[INSTRUMENT_MAX_LEN - 1] = '\0'; // Ensure null termination
    // Initialize padding to zero for consistent memory layout
    std::memset(padding, 0, sizeof(padding));
  }

  // ========================================================================
  // JSON SERIALIZATION
  // ========================================================================
  // Convert MarketData to JSON string
  //
  // Output format:
  // {
  //   "instrument": "RELIANCE",
  //   "bid": 2850.25,
  //   "ask": 2850.75,
  //   "timestamp_ns": 1234567890123
  // }
  //
  // NOTE: JSON is human-readable but SLOW compared to binary formats.
  // In Phase 8, we'll explore Flatbuffers for faster serialization.

  [[nodiscard]] std::string to_json() const {
    nlohmann::json j;
    j["instrument"] = instrument;
    j["bid"] = bid;
    j["ask"] = ask;
    j["timestamp_ns"] = timestamp_ns;
    return j.dump(); // dump() converts to string
  }

  // Parse JSON string back into MarketData
  // Returns true on success, false on failure
  // We return bool instead of throwing because exceptions are SLOW
  static bool from_json(const std::string &json_str, MarketData &out) {
    try {
      auto j = nlohmann::json::parse(json_str);

      // Get instrument as string, then copy to char array
      std::string inst = j["instrument"].get<std::string>();
      std::strncpy(out.instrument, inst.c_str(), INSTRUMENT_MAX_LEN - 1);
      out.instrument[INSTRUMENT_MAX_LEN - 1] = '\0';

      out.bid = j["bid"].get<double>();
      out.ask = j["ask"].get<double>();
      out.timestamp_ns = j["timestamp_ns"].get<int64_t>();

      return true;
    } catch (...) {
      // Any parse error returns false
      return false;
    }
  }
};

// ============================================================================
// COMPILE-TIME CHECKS
// ============================================================================
// static_assert runs at COMPILE TIME, not runtime
// If these fail, compilation stops with an error message

// Verify our size assumptions - should be exactly 64 bytes for cache alignment
static_assert(sizeof(MarketData) == 64, "MarketData size should be 64 bytes for cache line alignment");

// Verify alignment is correct
static_assert(alignof(MarketData) == 64, "MarketData should be aligned to 64-byte boundaries");

// Verify timestamp can hold nanoseconds for a long time
static_assert(sizeof(int64_t) == 8,
              "int64_t should be 8 bytes for nanosecond timestamps");

} // namespace hft
