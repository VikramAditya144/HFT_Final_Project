#include <catch2/catch_test_macros.hpp>
#include <common/market_data.hpp>
#include <common/fast_clock.hpp>
#include <string>
#include <cstring>
#include <random>
#include <chrono>
#include <thread>

using namespace hft;

// ============================================================================
// SIMPLE PROPERTY TEST HELPERS
// ============================================================================

class PropertyTestHelper {
public:
    static std::string generateRandomInstrument() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> len_dist(1, INSTRUMENT_MAX_LEN - 1);
        static std::uniform_int_distribution<> char_dist(0, 35); // 0-9, A-Z
        
        int len = len_dist(gen);
        std::string result;
        result.reserve(len);
        
        for (int i = 0; i < len; ++i) {
            int val = char_dist(gen);
            if (val < 10) {
                result += '0' + val;
            } else {
                result += 'A' + (val - 10);
            }
        }
        return result;
    }
    
    static double generateRandomPrice() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_real_distribution<> price_dist(0.01, 10000.0);
        
        double price = price_dist(gen);
        return std::round(price * 100.0) / 100.0; // Round to 2 decimal places
    }
    
    static int64_t generateRandomTimestamp() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<int64_t> ts_dist(1, 9223372036854775807LL);
        
        return ts_dist(gen);
    }
};

// ============================================================================
// PROPERTY-BASED TESTS
// ============================================================================

TEST_CASE("Property 1: Market data structure completeness", "[property][market_data]") {
    // Feature: hft-market-data-system, Property 1: Market data structure completeness
    // Validates: Requirements 1.1, 1.4
    
    // Run property test with 100 iterations as specified in design
    for (int i = 0; i < 100; ++i) {
        auto instrument = PropertyTestHelper::generateRandomInstrument();
        auto bid = PropertyTestHelper::generateRandomPrice();
        auto ask = PropertyTestHelper::generateRandomPrice();
        auto timestamp_ns = PropertyTestHelper::generateRandomTimestamp();
        
        // Create MarketData object
        MarketData data(instrument.c_str(), bid, ask, timestamp_ns);
        
        // Verify instrument name is non-empty and properly null-terminated
        REQUIRE(strlen(data.instrument) > 0);
        REQUIRE(strlen(data.instrument) < INSTRUMENT_MAX_LEN);
        REQUIRE(strncmp(data.instrument, instrument.c_str(), std::min(instrument.length(), size_t(INSTRUMENT_MAX_LEN - 1))) == 0);
        
        // Verify bid and ask prices are valid
        REQUIRE(data.bid > 0.0);
        REQUIRE(data.ask > 0.0);
        REQUIRE(data.bid == bid);
        REQUIRE(data.ask == ask);
        
        // Verify timestamp is nanosecond precision (positive int64_t)
        REQUIRE(data.timestamp_ns > 0);
        REQUIRE(data.timestamp_ns == timestamp_ns);
    }
}

TEST_CASE("Property 2: Fixed-size instrument name compliance", "[property][market_data]") {
    // Feature: hft-market-data-system, Property 3: Fixed-size instrument name compliance
    // Validates: Requirements 1.3
    
    // Run property test with 100 iterations
    for (int i = 0; i < 100; ++i) {
        auto instrument = PropertyTestHelper::generateRandomInstrument();
        auto bid = PropertyTestHelper::generateRandomPrice();
        auto ask = PropertyTestHelper::generateRandomPrice();
        auto timestamp_ns = PropertyTestHelper::generateRandomTimestamp();
        
        // Create MarketData object
        MarketData data(instrument.c_str(), bid, ask, timestamp_ns);
        
        // Verify the instrument fits in the fixed array
        REQUIRE(strlen(data.instrument) < INSTRUMENT_MAX_LEN);
        
        // Verify null termination
        REQUIRE((data.instrument[INSTRUMENT_MAX_LEN - 1] == '\0' || 
               strlen(data.instrument) < INSTRUMENT_MAX_LEN - 1));
        
        // Verify no heap allocation by checking the instrument is stored in the struct
        // (This is implicit - if we're using a fixed array, no heap allocation occurs)
        REQUIRE(sizeof(data.instrument) == INSTRUMENT_MAX_LEN);
    }
}

TEST_CASE("Property 3: Memory alignment verification", "[property][market_data]") {
    // Feature: hft-market-data-system, Property 10: Memory alignment verification
    // Validates: Requirements 4.3
    
    // Run property test with 100 iterations
    for (int i = 0; i < 100; ++i) {
        MarketData data;
        
        // Verify size is exactly 64 bytes
        REQUIRE(sizeof(MarketData) == 64);
        
        // Verify alignment is 64 bytes
        REQUIRE(alignof(MarketData) == 64);
        
        // Verify the actual object is aligned to 64-byte boundary
        uintptr_t addr = reinterpret_cast<uintptr_t>(&data);
        REQUIRE((addr % 64) == 0);
    }
}

TEST_CASE("Property 4: JSON serialization round-trip", "[property][market_data]") {
    // Feature: hft-market-data-system, Property 5: JSON serialization correctness
    // Validates: Requirements 2.3
    
    // Run property test with 100 iterations
    for (int i = 0; i < 100; ++i) {
        auto instrument = PropertyTestHelper::generateRandomInstrument();
        auto bid = PropertyTestHelper::generateRandomPrice();
        auto ask = PropertyTestHelper::generateRandomPrice();
        auto timestamp_ns = PropertyTestHelper::generateRandomTimestamp();
        
        // Create original MarketData object
        MarketData original(instrument.c_str(), bid, ask, timestamp_ns);
        
        // Serialize to JSON
        std::string json_str = original.to_json();
        
        // Verify JSON contains required fields
        REQUIRE(json_str.find("instrument") != std::string::npos);
        REQUIRE(json_str.find("bid") != std::string::npos);
        REQUIRE(json_str.find("ask") != std::string::npos);
        REQUIRE(json_str.find("timestamp_ns") != std::string::npos);
        
        // Deserialize back to MarketData
        MarketData deserialized;
        bool parse_success = MarketData::from_json(json_str, deserialized);
        
        // Verify parsing succeeded
        REQUIRE(parse_success);
        
        // Verify all fields are preserved
        REQUIRE(strcmp(original.instrument, deserialized.instrument) == 0);
        REQUIRE(original.bid == deserialized.bid);
        REQUIRE(original.ask == deserialized.ask);
        REQUIRE(original.timestamp_ns == deserialized.timestamp_ns);
    }
}

TEST_CASE("Property 13: Fast clock performance and precision", "[property][fast_clock]") {
    // Feature: hft-market-data-system, Property 13: Fast clock performance and precision
    // Validates: Requirements 7.1
    
    // Run property test with 100 iterations
    for (int i = 0; i < 100; ++i) {
        FastClock clock;
        
        // Verify the clock is running
        REQUIRE(clock.is_running());
        
        // Get initial timestamp
        int64_t timestamp1 = clock.now();
        
        // Verify timestamp is positive (nanoseconds since epoch)
        REQUIRE(timestamp1 > 0);
        
        // Verify timestamp is reasonable (not too far in the past or future)
        auto system_now = std::chrono::high_resolution_clock::now();
        auto system_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            system_now.time_since_epoch()).count();
        
        // Allow for some difference due to the 200ms update frequency
        // The cached time should be within 300ms of system time (200ms + buffer)
        int64_t diff = std::abs(timestamp1 - system_ns);
        REQUIRE(diff < 300000000LL); // 300ms in nanoseconds
        
        // Test that the clock provides nanosecond precision timestamps
        // Multiple rapid calls should return values that are either identical
        // (same cached value) or increasing (updated by background thread)
        int64_t timestamp2 = clock.now();
        int64_t timestamp3 = clock.now();
        
        // Timestamps should be monotonic (non-decreasing)
        REQUIRE(timestamp2 >= timestamp1);
        REQUIRE(timestamp3 >= timestamp2);
        
        // Verify the update frequency constant
        REQUIRE(FastClock::get_update_frequency_ms() == 200);
        
        // Test that the clock avoids syscalls in hot path by verifying
        // that calls to now() are very fast (this is implicit - if we're
        // reading from atomic variable, it should be much faster than syscalls)
        auto start = std::chrono::high_resolution_clock::now();
        for (int j = 0; j < 1000; ++j) {
            volatile int64_t ts = clock.now(); // volatile to prevent optimization
            (void)ts; // suppress unused variable warning
        }
        auto end = std::chrono::high_resolution_clock::now();
        
        // 1000 calls should complete very quickly (much less than 1ms)
        auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        REQUIRE(duration_us < 1000); // Should complete in less than 1ms
    }
}