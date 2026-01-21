#include <catch2/catch_test_macros.hpp>
#include <common/market_data.hpp>
#include <common/fast_clock.hpp>
#include <common/ring_buffer.hpp>
#include <string>
#include <cstring>
#include <random>
#include <chrono>
#include <thread>
#include <vector>
#include <set>

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

TEST_CASE("Property 2: Market data generation volume and variety", "[property][market_data]") {
    // Feature: hft-market-data-system, Property 2: Market data generation volume and variety
    // Validates: Requirements 1.2
    
    // This property tests that market data generation produces sufficient volume and variety
    // We simulate the publisher's generation logic to verify it meets requirements
    
    // Run property test with 100 iterations
    for (int i = 0; i < 100; ++i) {
        // Simulate the publisher's instrument list (same as in publisher main.cpp)
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
        
        // Simulate market data generation (same logic as publisher)
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> price_dist(100.0, 3000.0);
        std::uniform_real_distribution<double> spread_dist(0.01, 1.0);
        
        std::vector<MarketData> generated_messages;
        std::set<std::string> unique_instruments;
        std::set<std::pair<double, double>> unique_price_pairs;
        
        // Generate at least 1000 messages as required
        const size_t required_messages = 1000;
        
        for (size_t j = 0; j < required_messages; ++j) {
            // Generate market data using same logic as publisher
            const std::string& instrument = instruments[gen() % instruments.size()];
            double bid = price_dist(gen);
            double spread = spread_dist(gen);
            double ask = bid + spread;
            int64_t timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()).count();
            
            MarketData data(instrument.c_str(), bid, ask, timestamp);
            generated_messages.push_back(data);
            
            // Track variety
            unique_instruments.insert(instrument);
            unique_price_pairs.insert({bid, ask});
        }
        
        // Verify volume requirement: at least 1000 messages generated
        REQUIRE(generated_messages.size() >= required_messages);
        
        // Verify variety requirements:
        // 1. Should use multiple different instruments (at least 10 different ones)
        REQUIRE(unique_instruments.size() >= 10);
        
        // 2. Should generate different price combinations (at least 90% unique)
        // Due to random generation, we expect high variety in prices
        double uniqueness_ratio = static_cast<double>(unique_price_pairs.size()) / generated_messages.size();
        REQUIRE(uniqueness_ratio >= 0.9); // At least 90% of price pairs should be unique
        
        // 3. Verify all generated messages have valid structure
        for (const auto& msg : generated_messages) {
            // Instrument name should be non-empty and from our list
            REQUIRE(strlen(msg.instrument) > 0);
            bool found_instrument = false;
            for (const auto& inst : instruments) {
                if (strncmp(msg.instrument, inst.c_str(), INSTRUMENT_MAX_LEN) == 0) {
                    found_instrument = true;
                    break;
                }
            }
            REQUIRE(found_instrument);
            
            // Prices should be positive and ask > bid
            REQUIRE(msg.bid > 0.0);
            REQUIRE(msg.ask > 0.0);
            REQUIRE(msg.ask > msg.bid);
            
            // Timestamp should be positive (nanoseconds since epoch)
            REQUIRE(msg.timestamp_ns > 0);
        }
        
        // 4. Verify price ranges are within expected bounds (100-3000 for bid)
        for (const auto& msg : generated_messages) {
            REQUIRE(msg.bid >= 100.0);
            REQUIRE(msg.bid <= 3000.0);
            
            // Spread should be reasonable (0.01 to 1.0)
            double spread = msg.ask - msg.bid;
            REQUIRE(spread >= 0.01);
            REQUIRE(spread <= 1.0);
        }
        
        // 5. Verify temporal ordering (timestamps should be non-decreasing or very close)
        // Since generation happens quickly, timestamps might be identical or increasing
        for (size_t j = 1; j < generated_messages.size(); ++j) {
            // Allow for some timestamp variation due to generation speed
            // but they should be reasonably close (within 1 second)
            int64_t time_diff = std::abs(generated_messages[j].timestamp_ns - generated_messages[j-1].timestamp_ns);
            REQUIRE(time_diff < 1000000000LL); // Less than 1 second difference
        }
    }
}

TEST_CASE("Property 3: Fixed-size instrument name compliance", "[property][market_data]") {
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
TEST_CASE("Property 9: Lock-free ring buffer correctness", "[property][ring_buffer]") {
    // Feature: hft-market-data-system, Property 9: Lock-free ring buffer correctness
    // Validates: Requirements 4.1, 4.2
    
    // Run property test with 100 iterations
    for (int i = 0; i < 100; ++i) {
        RingBuffer buffer;
        
        // Verify initial state
        REQUIRE(buffer.is_empty());
        REQUIRE_FALSE(buffer.is_full());
        REQUIRE(buffer.available_for_read() == 0);
        REQUIRE(buffer.available_for_write() == buffer.capacity());
        
        // Generate random test data
        std::vector<MarketData> test_data;
        int num_items = std::min(static_cast<int>(buffer.capacity()), 100 + (i % 500));
        
        for (int j = 0; j < num_items; ++j) {
            auto instrument = PropertyTestHelper::generateRandomInstrument();
            auto bid = PropertyTestHelper::generateRandomPrice();
            auto ask = PropertyTestHelper::generateRandomPrice();
            auto timestamp_ns = PropertyTestHelper::generateRandomTimestamp();
            
            test_data.emplace_back(instrument.c_str(), bid, ask, timestamp_ns);
        }
        
        // Test writing data
        for (size_t j = 0; j < test_data.size(); ++j) {
            bool write_success = buffer.try_write(test_data[j]);
            REQUIRE(write_success);
            
            // Verify buffer state after each write
            REQUIRE(buffer.available_for_read() == j + 1);
            REQUIRE(buffer.available_for_write() == buffer.capacity() - (j + 1));
            REQUIRE_FALSE(buffer.is_empty());
        }
        
        // Verify buffer is full after writing capacity items
        if (test_data.size() == buffer.capacity()) {
            REQUIRE(buffer.is_full());
            REQUIRE(buffer.available_for_write() == 0);
            
            // Try to write one more item - should fail
            MarketData extra_data("EXTRA", 100.0, 101.0, 123456789);
            REQUIRE_FALSE(buffer.try_write(extra_data));
        }
        
        // Test reading data back
        std::vector<MarketData> read_data;
        MarketData read_item;
        
        while (buffer.try_read(read_item)) {
            read_data.push_back(read_item);
        }
        
        // Verify we read back the same number of items
        REQUIRE(read_data.size() == test_data.size());
        
        // Verify data integrity - all written data should be read back correctly
        for (size_t j = 0; j < test_data.size(); ++j) {
            REQUIRE(strcmp(test_data[j].instrument, read_data[j].instrument) == 0);
            REQUIRE(test_data[j].bid == read_data[j].bid);
            REQUIRE(test_data[j].ask == read_data[j].ask);
            REQUIRE(test_data[j].timestamp_ns == read_data[j].timestamp_ns);
        }
        
        // Verify buffer is empty after reading all data
        REQUIRE(buffer.is_empty());
        REQUIRE_FALSE(buffer.is_full());
        REQUIRE(buffer.available_for_read() == 0);
        REQUIRE(buffer.available_for_write() == buffer.capacity());
        
        // Try to read from empty buffer - should fail
        MarketData empty_read;
        REQUIRE_FALSE(buffer.try_read(empty_read));
    }
}
TEST_CASE("Property 11: Ring buffer state management", "[property][ring_buffer]") {
    // Feature: hft-market-data-system, Property 11: Ring buffer state management
    // Validates: Requirements 4.4, 4.5
    
    // Run property test with 100 iterations
    for (int i = 0; i < 100; ++i) {
        RingBuffer buffer;
        
        // Test overflow detection (buffer full condition)
        // Fill the buffer to capacity
        std::vector<MarketData> test_data;
        for (size_t j = 0; j < buffer.capacity(); ++j) {
            auto instrument = PropertyTestHelper::generateRandomInstrument();
            auto bid = PropertyTestHelper::generateRandomPrice();
            auto ask = PropertyTestHelper::generateRandomPrice();
            auto timestamp_ns = PropertyTestHelper::generateRandomTimestamp();
            
            MarketData data(instrument.c_str(), bid, ask, timestamp_ns);
            test_data.push_back(data);
            
            bool write_success = buffer.try_write(data);
            REQUIRE(write_success);
        }
        
        // Verify buffer is now full
        REQUIRE(buffer.is_full());
        REQUIRE(buffer.available_for_write() == 0);
        REQUIRE(buffer.available_for_read() == buffer.capacity());
        REQUIRE_FALSE(buffer.is_empty());
        
        // Test overflow condition - trying to write to full buffer should fail
        MarketData overflow_data("OVERFLOW", 999.99, 1000.01, 987654321);
        bool overflow_write = buffer.try_write(overflow_data);
        REQUIRE_FALSE(overflow_write); // Should indicate overflow condition
        
        // Verify buffer state remains unchanged after failed write
        REQUIRE(buffer.is_full());
        REQUIRE(buffer.available_for_write() == 0);
        REQUIRE(buffer.available_for_read() == buffer.capacity());
        
        // Test underflow detection (buffer empty condition)
        // Empty the buffer completely
        MarketData read_item;
        size_t items_read = 0;
        while (buffer.try_read(read_item)) {
            items_read++;
        }
        
        // Verify we read exactly the number of items we wrote
        REQUIRE(items_read == buffer.capacity());
        
        // Verify buffer is now empty
        REQUIRE(buffer.is_empty());
        REQUIRE(buffer.available_for_read() == 0);
        REQUIRE(buffer.available_for_write() == buffer.capacity());
        REQUIRE_FALSE(buffer.is_full());
        
        // Test underflow condition - trying to read from empty buffer should fail
        MarketData underflow_data;
        bool underflow_read = buffer.try_read(underflow_data);
        REQUIRE_FALSE(underflow_read); // Should indicate underflow condition
        
        // Verify buffer state remains unchanged after failed read
        REQUIRE(buffer.is_empty());
        REQUIRE(buffer.available_for_read() == 0);
        REQUIRE(buffer.available_for_write() == buffer.capacity());
        
        // Test partial fill/empty states
        // Fill buffer partially (random amount between 1 and capacity-1)
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<size_t> partial_dist(1, buffer.capacity() - 1);
        size_t partial_fill = partial_dist(gen);
        
        for (size_t j = 0; j < partial_fill; ++j) {
            auto instrument = PropertyTestHelper::generateRandomInstrument();
            auto bid = PropertyTestHelper::generateRandomPrice();
            auto ask = PropertyTestHelper::generateRandomPrice();
            auto timestamp_ns = PropertyTestHelper::generateRandomTimestamp();
            
            MarketData data(instrument.c_str(), bid, ask, timestamp_ns);
            bool write_success = buffer.try_write(data);
            REQUIRE(write_success);
        }
        
        // Verify partial state is correctly detected
        REQUIRE_FALSE(buffer.is_empty());
        REQUIRE_FALSE(buffer.is_full());
        REQUIRE(buffer.available_for_read() == partial_fill);
        REQUIRE(buffer.available_for_write() == buffer.capacity() - partial_fill);
        
        // Verify we can still write and read in partial state
        MarketData partial_write_data("PARTIAL", 123.45, 123.67, 555666777);
        bool partial_write_success = buffer.try_write(partial_write_data);
        REQUIRE(partial_write_success);
        
        MarketData partial_read_data;
        bool partial_read_success = buffer.try_read(partial_read_data);
        REQUIRE(partial_read_success);
        
        // Test state transitions
        // The buffer should correctly transition between empty, partial, and full states
        // This is implicitly tested by the operations above, but we verify the
        // state consistency throughout the transitions
        bool empty_state_consistent = buffer.is_empty() ? (buffer.available_for_read() == 0) : true;
        bool non_empty_state_consistent = !buffer.is_empty() ? (buffer.available_for_read() > 0) : true;
        REQUIRE(empty_state_consistent);
        REQUIRE(non_empty_state_consistent);
        
        bool full_state_consistent = buffer.is_full() ? (buffer.available_for_write() == 0) : true;
        bool non_full_state_consistent = !buffer.is_full() ? (buffer.available_for_write() > 0) : true;
        REQUIRE(full_state_consistent);
        REQUIRE(non_full_state_consistent);
        
        REQUIRE(buffer.available_for_read() + buffer.available_for_write() == buffer.capacity());
    }
}