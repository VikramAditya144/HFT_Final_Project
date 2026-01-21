#include <catch2/catch_test_macros.hpp>
#include <common/market_data.hpp>
#include <string>
#include <cstring>
#include <random>
#include <chrono>

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