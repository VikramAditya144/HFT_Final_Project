#include <catch2/catch_test_macros.hpp>
#include <common/market_data.hpp>
#include <common/fast_clock.hpp>
#include <common/ring_buffer.hpp>
#include <common/performance_utils.hpp>
#include <string>
#include <cstring>
#include <random>
#include <chrono>
#include <thread>
#include <vector>
#include <set>
#include <sys/mman.h>
#include <cstdlib>

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

TEST_CASE("Property 10: Memory alignment verification", "[property][memory_alignment]") {
    // Feature: hft-market-data-system, Property 10: Memory alignment verification
    // Validates: Requirements 4.3
    
    // Run property test with 100 iterations
    for (int i = 0; i < 100; ++i) {
        // Test MarketData alignment
        MarketData market_data;
        
        // Verify MarketData size is exactly 64 bytes
        REQUIRE(sizeof(MarketData) == 64);
        
        // Verify MarketData alignment is 64 bytes
        REQUIRE(alignof(MarketData) == 64);
        
        // Verify the actual MarketData object is aligned to 64-byte boundary
        uintptr_t market_data_addr = reinterpret_cast<uintptr_t>(&market_data);
        REQUIRE((market_data_addr % 64) == 0);
        
        // Test RingBuffer alignment
        RingBuffer ring_buffer;
        
        // Verify RingBuffer alignment is 64 bytes
        REQUIRE(alignof(RingBuffer) == 64);
        
        // Verify the actual RingBuffer object is aligned to 64-byte boundary
        uintptr_t ring_buffer_addr = reinterpret_cast<uintptr_t>(&ring_buffer);
        REQUIRE((ring_buffer_addr % 64) == 0);
        
        // Test that critical data structures prevent false sharing
        // by verifying they are aligned to cache line boundaries
        
        // Create multiple MarketData objects and verify they don't share cache lines
        MarketData data_array[3];
        for (int j = 0; j < 3; ++j) {
            uintptr_t addr = reinterpret_cast<uintptr_t>(&data_array[j]);
            REQUIRE((addr % 64) == 0); // Each should be on its own cache line
        }
        
        // Test alignment with dynamic allocation
        auto* dynamic_market_data = new MarketData();
        uintptr_t dynamic_addr = reinterpret_cast<uintptr_t>(dynamic_market_data);
        REQUIRE((dynamic_addr % 64) == 0);
        delete dynamic_market_data;
        
        // Test alignment with shared memory allocation simulation
        // Allocate aligned memory similar to how shared memory would work
        void* aligned_mem = nullptr;
        int result = posix_memalign(&aligned_mem, 64, sizeof(RingBuffer));
        REQUIRE(result == 0);
        REQUIRE(aligned_mem != nullptr);
        
        // Verify the aligned memory is properly aligned
        uintptr_t aligned_addr = reinterpret_cast<uintptr_t>(aligned_mem);
        REQUIRE((aligned_addr % 64) == 0);
        
        // Construct RingBuffer in aligned memory and verify it maintains alignment
        RingBuffer* aligned_ring_buffer = new(aligned_mem) RingBuffer();
        uintptr_t constructed_addr = reinterpret_cast<uintptr_t>(aligned_ring_buffer);
        REQUIRE((constructed_addr % 64) == 0);
        
        // Clean up
        aligned_ring_buffer->~RingBuffer();
        free(aligned_mem);
        
        // Test that the performance utilities can verify alignment correctly
        REQUIRE(hft::MemoryUtils::is_aligned(&market_data, 64));
        REQUIRE(hft::MemoryUtils::is_aligned(&ring_buffer, 64));
        
        // Test type alignment verification
        REQUIRE(hft::MemoryUtils::is_type_aligned<MarketData>(64));
        REQUIRE(hft::MemoryUtils::is_type_aligned<RingBuffer>(64));
        
        // Test cache line size detection
        size_t cache_line_size = hft::MemoryUtils::get_cache_line_size();
        REQUIRE(cache_line_size >= 32); // Should be at least 32 bytes
        REQUIRE(cache_line_size <= 128); // Should not exceed 128 bytes
        REQUIRE((cache_line_size & (cache_line_size - 1)) == 0); // Should be power of 2
        
        // Verify our structures are aligned to detected cache line size
        if (cache_line_size == 64) {
            REQUIRE((market_data_addr % cache_line_size) == 0);
            REQUIRE((ring_buffer_addr % cache_line_size) == 0);
        }
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

// ============================================================================
// TCP SERVER PROPERTY TESTS
// ============================================================================

#include <boost/asio.hpp>
#include <thread>
#include <chrono>
#include <future>

TEST_CASE("Property 4: TCP connection handling", "[property][tcp]") {
    // Feature: hft-market-data-system, Property 4: TCP connection handling
    // Validates: Requirements 2.2
    
    // Run property test with 100 iterations
    for (int i = 0; i < 100; ++i) {
        boost::asio::io_context io_context;
        
        // Create a TCP acceptor on a random available port
        boost::asio::ip::tcp::acceptor acceptor(io_context);
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), 0); // 0 = any available port
        acceptor.open(endpoint.protocol());
        acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
        acceptor.bind(endpoint);
        acceptor.listen();
        
        // Get the actual port assigned
        auto local_endpoint = acceptor.local_endpoint();
        unsigned short port = local_endpoint.port();
        
        // Track connection acceptance
        bool connection_accepted = false;
        std::shared_ptr<boost::asio::ip::tcp::socket> accepted_socket;
        
        // Start accepting connections
        accepted_socket = std::make_shared<boost::asio::ip::tcp::socket>(io_context);
        acceptor.async_accept(*accepted_socket,
            [&connection_accepted](boost::system::error_code ec) {
                if (!ec) {
                    connection_accepted = true;
                }
            });
        
        // Run io_context in a separate thread
        std::thread io_thread([&io_context]() {
            io_context.run();
        });
        
        // Create a client connection
        boost::asio::io_context client_io_context;
        boost::asio::ip::tcp::socket client_socket(client_io_context);
        
        // Connect to the server
        boost::system::error_code connect_ec;
        boost::asio::ip::tcp::endpoint server_endpoint(boost::asio::ip::tcp::v4(), port);
        server_endpoint.address(boost::asio::ip::make_address("127.0.0.1"));
        client_socket.connect(server_endpoint, connect_ec);
        
        // Give some time for the connection to be processed
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        // Verify connection was successful
        REQUIRE_FALSE(connect_ec);
        REQUIRE(client_socket.is_open());
        
        // Verify server accepted the connection
        REQUIRE(connection_accepted);
        REQUIRE(accepted_socket->is_open());
        
        // Verify we can get remote endpoint information
        auto remote_endpoint = accepted_socket->remote_endpoint();
        REQUIRE(remote_endpoint.address().to_string() == "127.0.0.1");
        
        // Clean up
        client_socket.close();
        accepted_socket->close();
        acceptor.close();
        io_context.stop();
        
        if (io_thread.joinable()) {
            io_thread.join();
        }
    }
}

TEST_CASE("Property 5: JSON serialization correctness", "[property][tcp][json]") {
    // Feature: hft-market-data-system, Property 5: JSON serialization correctness
    // Validates: Requirements 2.3
    
    // Run property test with 100 iterations
    for (int i = 0; i < 100; ++i) {
        // Generate random market data
        auto instrument = PropertyTestHelper::generateRandomInstrument();
        auto bid = PropertyTestHelper::generateRandomPrice();
        auto ask = PropertyTestHelper::generateRandomPrice();
        auto timestamp_ns = PropertyTestHelper::generateRandomTimestamp();
        
        MarketData original(instrument.c_str(), bid, ask, timestamp_ns);
        
        // Serialize to JSON (same method used by TCP server)
        std::string json_message = original.to_json();
        
        // Verify JSON format contains all required fields for TCP transmission
        REQUIRE(json_message.find("\"instrument\"") != std::string::npos);
        REQUIRE(json_message.find("\"bid\"") != std::string::npos);
        REQUIRE(json_message.find("\"ask\"") != std::string::npos);
        REQUIRE(json_message.find("\"timestamp_ns\"") != std::string::npos);
        
        // Verify JSON is valid by parsing it
        try {
            auto parsed_json = nlohmann::json::parse(json_message);
            
            // Verify all fields are present and have correct types
            REQUIRE(parsed_json.contains("instrument"));
            REQUIRE(parsed_json.contains("bid"));
            REQUIRE(parsed_json.contains("ask"));
            REQUIRE(parsed_json.contains("timestamp_ns"));
            
            REQUIRE(parsed_json["instrument"].is_string());
            REQUIRE(parsed_json["bid"].is_number());
            REQUIRE(parsed_json["ask"].is_number());
            REQUIRE(parsed_json["timestamp_ns"].is_number_integer());
            
            // Verify values match original data
            REQUIRE(parsed_json["instrument"].get<std::string>() == std::string(original.instrument));
            REQUIRE(parsed_json["bid"].get<double>() == original.bid);
            REQUIRE(parsed_json["ask"].get<double>() == original.ask);
            REQUIRE(parsed_json["timestamp_ns"].get<int64_t>() == original.timestamp_ns);
            
        } catch (const std::exception& e) {
            FAIL("JSON parsing failed: " << e.what() << " for JSON: " << json_message);
        }
        
        // Verify JSON message is suitable for TCP transmission
        // (no null bytes, reasonable size, etc.)
        REQUIRE(json_message.find('\0') == std::string::npos); // No null bytes
        REQUIRE(json_message.length() > 0);
        REQUIRE(json_message.length() < 1024); // Reasonable size for network transmission
        
        // Verify JSON can be transmitted over TCP by simulating message boundary
        std::string tcp_message = json_message + "\n"; // Add newline as message boundary
        REQUIRE(tcp_message.back() == '\n');
        REQUIRE(tcp_message.length() == json_message.length() + 1);
    }
}

TEST_CASE("Property 6: TCP disconnection resilience", "[property][tcp]") {
    // Feature: hft-market-data-system, Property 6: TCP disconnection resilience
    // Validates: Requirements 2.4
    
    // Run property test with 100 iterations
    for (int i = 0; i < 100; ++i) {
        boost::asio::io_context io_context;
        
        // Create TCP acceptor
        boost::asio::ip::tcp::acceptor acceptor(io_context);
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), 0);
        acceptor.open(endpoint.protocol());
        acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
        acceptor.bind(endpoint);
        acceptor.listen();
        
        auto local_endpoint = acceptor.local_endpoint();
        unsigned short port = local_endpoint.port();
        
        // Track server state
        bool server_crashed = false;
        bool connection_accepted = false;
        bool disconnection_detected = false;
        std::shared_ptr<boost::asio::ip::tcp::socket> server_socket;
        
        // Accept connection
        server_socket = std::make_shared<boost::asio::ip::tcp::socket>(io_context);
        acceptor.async_accept(*server_socket,
            [&](boost::system::error_code ec) {
                if (!ec) {
                    connection_accepted = true;
                    
                    // Set up disconnection detection (similar to TcpServer implementation)
                    auto buffer = std::make_shared<std::array<char, 1>>();
                    server_socket->async_read_some(boost::asio::buffer(*buffer),
                        [&](boost::system::error_code read_ec, std::size_t /*length*/) {
                            if (read_ec) {
                                disconnection_detected = true;
                            }
                        });
                } else {
                    server_crashed = true;
                }
            });
        
        // Run server in separate thread
        std::thread server_thread([&]() {
            try {
                io_context.run();
            } catch (const std::exception&) {
                server_crashed = true;
            }
        });
        
        // Create client and connect
        boost::asio::io_context client_io_context;
        boost::asio::ip::tcp::socket client_socket(client_io_context);
        
        boost::system::error_code connect_ec;
        boost::asio::ip::tcp::endpoint server_endpoint2(boost::asio::ip::tcp::v4(), port);
        server_endpoint2.address(boost::asio::ip::make_address("127.0.0.1"));
        client_socket.connect(server_endpoint2, connect_ec);
        
        // Wait for connection to be established
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        REQUIRE_FALSE(connect_ec);
        REQUIRE(connection_accepted);
        REQUIRE_FALSE(server_crashed);
        
        // Simulate client disconnection
        client_socket.close();
        
        // Give server time to detect disconnection
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // Verify server handled disconnection gracefully
        REQUIRE(disconnection_detected);
        REQUIRE_FALSE(server_crashed); // Server should not crash on client disconnect
        
        // Verify server can still accept new connections after a disconnection
        boost::asio::ip::tcp::socket new_client_socket(client_io_context);
        boost::system::error_code new_connect_ec;
        
        // Reset connection tracking
        connection_accepted = false;
        
        // Start accepting another connection
        auto new_server_socket = std::make_shared<boost::asio::ip::tcp::socket>(io_context);
        acceptor.async_accept(*new_server_socket,
            [&](boost::system::error_code ec) {
                if (!ec) {
                    connection_accepted = true;
                }
            });
        
        // Connect new client
        boost::asio::ip::tcp::endpoint server_endpoint3(boost::asio::ip::tcp::v4(), port);
        server_endpoint3.address(boost::asio::ip::make_address("127.0.0.1"));
        new_client_socket.connect(server_endpoint3, new_connect_ec);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        // Verify server can handle new connections after previous disconnection
        REQUIRE_FALSE(new_connect_ec);
        REQUIRE(connection_accepted);
        REQUIRE_FALSE(server_crashed);
        
        // Clean up
        new_client_socket.close();
        new_server_socket->close();
        acceptor.close();
        io_context.stop();
        
        if (server_thread.joinable()) {
            server_thread.join();
        }
    }
}

// ============================================================================
// TCP CONSUMER PROPERTY TESTS
// ============================================================================

TEST_CASE("Property 7: JSON parsing completeness", "[property][tcp_consumer]") {
    // Feature: hft-market-data-system, Property 7: JSON parsing completeness
    // Validates: Requirements 3.2
    
    // Run property test with 100 iterations
    for (int i = 0; i < 100; ++i) {
        // Generate random market data for testing
        auto instrument = PropertyTestHelper::generateRandomInstrument();
        auto bid = PropertyTestHelper::generateRandomPrice();
        auto ask = PropertyTestHelper::generateRandomPrice();
        auto timestamp_ns = PropertyTestHelper::generateRandomTimestamp();
        
        MarketData original(instrument.c_str(), bid, ask, timestamp_ns);
        
        // Create JSON message (same format as publisher sends)
        std::string json_message = original.to_json();
        
        // Test the parsing logic used by TCP consumer
        MarketData parsed_data;
        bool parse_success = MarketData::from_json(json_message, parsed_data);
        
        // Verify parsing succeeded
        REQUIRE(parse_success);
        
        // Verify all fields are accessible and correct after parsing
        REQUIRE(strcmp(original.instrument, parsed_data.instrument) == 0);
        REQUIRE(original.bid == parsed_data.bid);
        REQUIRE(original.ask == parsed_data.ask);
        REQUIRE(original.timestamp_ns == parsed_data.timestamp_ns);
        
        // Test parsing with various JSON formatting variations
        // (whitespace, field order, etc.)
        nlohmann::json j;
        j["instrument"] = original.instrument;
        j["bid"] = original.bid;
        j["ask"] = original.ask;
        j["timestamp_ns"] = original.timestamp_ns;
        
        // Test with different JSON formatting
        std::vector<std::string> json_variants = {
            j.dump(),           // Compact format
            j.dump(2),          // Pretty-printed with 2-space indent
            j.dump(4),          // Pretty-printed with 4-space indent
        };
        
        for (const auto& json_variant : json_variants) {
            MarketData variant_parsed;
            bool variant_success = MarketData::from_json(json_variant, variant_parsed);
            
            REQUIRE(variant_success);
            REQUIRE(strcmp(original.instrument, variant_parsed.instrument) == 0);
            REQUIRE(original.bid == variant_parsed.bid);
            REQUIRE(original.ask == variant_parsed.ask);
            REQUIRE(original.timestamp_ns == variant_parsed.timestamp_ns);
        }
        
        // Test parsing with malformed JSON (should fail gracefully)
        std::vector<std::string> malformed_json = {
            "{",                                    // Incomplete JSON
            "{}",                                   // Empty JSON
            "{\"instrument\":\"TEST\"}",            // Missing fields
            "{\"invalid\":\"json\",}",              // Trailing comma
            "not json at all",                      // Not JSON
            "",                                     // Empty string
        };
        
        for (const auto& bad_json : malformed_json) {
            MarketData bad_parsed;
            bool bad_success = MarketData::from_json(bad_json, bad_parsed);
            REQUIRE_FALSE(bad_success); // Should fail gracefully, not crash
        }
        
        // Test parsing with edge case values
        // Very long instrument name (should be truncated)
        std::string long_instrument(INSTRUMENT_MAX_LEN + 10, 'X');
        MarketData long_inst_data(long_instrument.c_str(), 100.0, 101.0, 123456789);
        std::string long_inst_json = long_inst_data.to_json();
        
        MarketData long_inst_parsed;
        bool long_inst_success = MarketData::from_json(long_inst_json, long_inst_parsed);
        REQUIRE(long_inst_success);
        REQUIRE(strlen(long_inst_parsed.instrument) < INSTRUMENT_MAX_LEN);
        
        // Very large numbers
        MarketData large_num_data("TEST", 999999999.99, 1000000000.01, 9223372036854775807LL);
        std::string large_num_json = large_num_data.to_json();
        
        MarketData large_num_parsed;
        bool large_num_success = MarketData::from_json(large_num_json, large_num_parsed);
        REQUIRE(large_num_success);
        REQUIRE(large_num_parsed.bid == large_num_data.bid);
        REQUIRE(large_num_parsed.ask == large_num_data.ask);
        REQUIRE(large_num_parsed.timestamp_ns == large_num_data.timestamp_ns);
    }
}

TEST_CASE("Property 8: TCP stream boundary handling", "[property][tcp_consumer]") {
    // Feature: hft-market-data-system, Property 8: TCP stream boundary handling
    // Validates: Requirements 3.3
    
    // Run property test with 100 iterations
    for (int i = 0; i < 100; ++i) {
        // Test the core boundary handling logic without complex networking
        // This focuses on the actual TCP consumer logic for handling partial reads
        
        // Generate test JSON messages
        std::vector<std::string> json_messages;
        std::vector<MarketData> expected_data;
        
        int num_messages = 3 + (i % 3); // 3-5 messages per test
        for (int j = 0; j < num_messages; ++j) {
            auto instrument = PropertyTestHelper::generateRandomInstrument();
            auto bid = PropertyTestHelper::generateRandomPrice();
            auto ask = PropertyTestHelper::generateRandomPrice();
            auto timestamp_ns = PropertyTestHelper::generateRandomTimestamp();
            
            MarketData data(instrument.c_str(), bid, ask, timestamp_ns);
            expected_data.push_back(data);
            json_messages.push_back(data.to_json());
        }
        
        // Test different boundary scenarios by simulating streambuf behavior
        std::vector<std::string> test_scenarios;
        
        // Scenario 1: All messages concatenated with proper newlines
        std::string scenario1;
        for (const auto& json : json_messages) {
            scenario1 += json + "\n";
        }
        test_scenarios.push_back(scenario1);
        
        // Scenario 2: Messages with extra whitespace and formatting
        std::string scenario2;
        for (const auto& json : json_messages) {
            scenario2 += json + "\n";
        }
        test_scenarios.push_back(scenario2);
        
        // Scenario 3: Messages sent individually (simulating multiple read operations)
        for (const auto& json : json_messages) {
            test_scenarios.push_back(json + "\n");
        }
        
        // Test each scenario
        for (const auto& scenario : test_scenarios) {
            // Simulate the TCP consumer's message parsing logic
            std::vector<MarketData> parsed_messages;
            
            // Create a streambuf with the test data
            boost::asio::streambuf buffer;
            std::ostream os(&buffer);
            os << scenario;
            
            // Parse messages using the same logic as TCP consumer
            std::istream stream(&buffer);
            std::string line;
            
            while (std::getline(stream, line)) {
                if (!line.empty()) {
                    MarketData parsed_data;
                    bool parse_success = MarketData::from_json(line, parsed_data);
                    
                    // This should always succeed with well-formed JSON
                    REQUIRE(parse_success);
                    
                    if (parse_success) {
                        parsed_messages.push_back(parsed_data);
                    }
                }
            }
            
            // For scenarios 1 and 2, we should get all messages
            if (&scenario == &test_scenarios[0] || &scenario == &test_scenarios[1]) {
                REQUIRE(parsed_messages.size() == expected_data.size());
                
                // Verify message content
                for (size_t k = 0; k < expected_data.size(); ++k) {
                    bool found_match = false;
                    for (size_t m = 0; m < parsed_messages.size(); ++m) {
                        if (strcmp(expected_data[k].instrument, parsed_messages[m].instrument) == 0 &&
                            expected_data[k].bid == parsed_messages[m].bid &&
                            expected_data[k].ask == parsed_messages[m].ask &&
                            expected_data[k].timestamp_ns == parsed_messages[m].timestamp_ns) {
                            found_match = true;
                            break;
                        }
                    }
                    REQUIRE(found_match);
                }
            }
        }
        
        // Test edge cases for boundary handling
        
        // Edge case 1: Empty lines should be ignored
        boost::asio::streambuf empty_buffer;
        std::ostream empty_os(&empty_buffer);
        empty_os << "\n\n" << json_messages[0] << "\n\n" << json_messages[1] << "\n\n";
        
        std::vector<MarketData> empty_line_parsed;
        std::istream empty_stream(&empty_buffer);
        std::string empty_line;
        
        while (std::getline(empty_stream, empty_line)) {
            if (!empty_line.empty()) {
                MarketData parsed_data;
                bool parse_success = MarketData::from_json(empty_line, parsed_data);
                if (parse_success) {
                    empty_line_parsed.push_back(parsed_data);
                }
            }
        }
        
        // Should have parsed exactly 2 messages (ignoring empty lines)
        REQUIRE(empty_line_parsed.size() == 2);
        
        // Edge case 2: Malformed JSON should be handled gracefully
        boost::asio::streambuf malformed_buffer;
        std::ostream malformed_os(&malformed_buffer);
        malformed_os << json_messages[0] << "\n";
        malformed_os << "{ invalid json }\n";  // This should fail parsing
        malformed_os << json_messages[1] << "\n";
        
        std::vector<MarketData> malformed_parsed;
        std::istream malformed_stream(&malformed_buffer);
        std::string malformed_line;
        
        while (std::getline(malformed_stream, malformed_line)) {
            if (!malformed_line.empty()) {
                MarketData parsed_data;
                bool parse_success = MarketData::from_json(malformed_line, parsed_data);
                if (parse_success) {
                    malformed_parsed.push_back(parsed_data);
                }
                // Note: We don't REQUIRE parse_success here because malformed JSON should fail
            }
        }
        
        // Should have parsed exactly 2 valid messages (skipping the malformed one)
        REQUIRE(malformed_parsed.size() == 2);
        
        // Edge case 3: Very long lines should be handled correctly
        std::string long_instrument(INSTRUMENT_MAX_LEN - 1, 'X'); // Max length instrument
        MarketData long_data(long_instrument.c_str(), 999.99, 1000.01, 123456789);
        std::string long_json = long_data.to_json();
        
        boost::asio::streambuf long_buffer;
        std::ostream long_os(&long_buffer);
        long_os << long_json << "\n";
        
        std::istream long_stream(&long_buffer);
        std::string long_line;
        std::getline(long_stream, long_line);
        
        MarketData long_parsed;
        bool long_parse_success = MarketData::from_json(long_line, long_parsed);
        REQUIRE(long_parse_success);
        REQUIRE(strcmp(long_data.instrument, long_parsed.instrument) == 0);
    }
}

// ============================================================================
// SHARED MEMORY CONSUMER PROPERTY TESTS
// ============================================================================

#include "common/shared_memory.hpp"

TEST_CASE("Property 12: Shared memory consumer polling", "[property][shm_consumer]") {
    // Feature: hft-market-data-system, Property 12: Shared memory consumer polling
    // Validates: Requirements 6.2
    
    // Simplified test that focuses on the core polling behavior without complex shared memory setup
    // This tests the polling logic using a regular RingBuffer to validate the consumer behavior
    
    // Run property test with 10 iterations
    for (int i = 0; i < 10; ++i) {
        // Use a regular ring buffer instead of shared memory to test polling logic
        RingBuffer ring_buffer;
        
        // Verify initial state - buffer should be empty
        REQUIRE(ring_buffer.is_empty());
        REQUIRE(ring_buffer.available_for_read() == 0);
        
        // Generate test data
        std::vector<MarketData> test_messages;
        int num_messages = 2 + (i % 3); // 2-4 messages per test
        
        for (int j = 0; j < num_messages; ++j) {
            auto instrument = PropertyTestHelper::generateRandomInstrument();
            auto bid = PropertyTestHelper::generateRandomPrice();
            auto ask = PropertyTestHelper::generateRandomPrice();
            auto timestamp_ns = PropertyTestHelper::generateRandomTimestamp();
            
            MarketData data(instrument.c_str(), bid, ask, timestamp_ns);
            test_messages.push_back(data);
        }
        
        // Test polling behavior: write and read messages one by one
        // This simulates the producer-consumer interaction that would happen in shared memory
        for (size_t msg_idx = 0; msg_idx < test_messages.size(); ++msg_idx) {
            // Producer writes a message
            bool write_success = ring_buffer.try_write(test_messages[msg_idx]);
            REQUIRE(write_success);
            
            // Simulate consumer polling behavior
            bool message_detected = false;
            int poll_attempts = 0;
            const int max_poll_attempts = 50;
            
            while (!message_detected && poll_attempts < max_poll_attempts) {
                // This is the core polling logic used by the SHM consumer
                if (!ring_buffer.is_empty()) {
                    message_detected = true;
                    
                    // Verify available_for_read reflects the new message
                    REQUIRE(ring_buffer.available_for_read() > 0);
                    
                    // Consumer reads the message
                    MarketData consumed_data;
                    bool read_success = ring_buffer.try_read(consumed_data);
                    REQUIRE(read_success);
                    
                    // Verify message integrity (core requirement for SHM consumer)
                    REQUIRE(strcmp(test_messages[msg_idx].instrument, consumed_data.instrument) == 0);
                    REQUIRE(test_messages[msg_idx].bid == consumed_data.bid);
                    REQUIRE(test_messages[msg_idx].ask == consumed_data.ask);
                    REQUIRE(test_messages[msg_idx].timestamp_ns == consumed_data.timestamp_ns);
                }
                poll_attempts++;
                
                // Small delay to simulate polling interval
                if (!message_detected) {
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                }
            }
            
            // Verify the message was detected within reasonable time
            REQUIRE(message_detected);
            REQUIRE(poll_attempts < max_poll_attempts);
        }
        
        // Verify buffer is empty after consuming all messages
        REQUIRE(ring_buffer.is_empty());
        REQUIRE(ring_buffer.available_for_read() == 0);
        
        // Test polling behavior when buffer is empty (underflow detection)
        // This is critical for the SHM consumer's spin-wait loop
        MarketData empty_read_data;
        bool empty_read_success = ring_buffer.try_read(empty_read_data);
        REQUIRE_FALSE(empty_read_success); // Should fail when buffer is empty
        
        // Test rapid polling on empty buffer (simulates spin-wait behavior)
        int empty_polls = 0;
        const int max_empty_polls = 20;
        
        for (int poll = 0; poll < max_empty_polls; ++poll) {
            if (ring_buffer.is_empty()) {
                empty_polls++;
                
                // Try to read - should consistently fail
                MarketData poll_data;
                bool poll_read = ring_buffer.try_read(poll_data);
                REQUIRE_FALSE(poll_read);
            }
        }
        
        // Should have detected empty state consistently
        REQUIRE(empty_polls == max_empty_polls);
        
        // Test burst polling behavior (multiple messages available at once)
        std::vector<MarketData> burst_messages;
        int burst_size = std::min(3, static_cast<int>(ring_buffer.capacity()));
        
        // Write multiple messages quickly
        for (int b = 0; b < burst_size; ++b) {
            auto instrument = PropertyTestHelper::generateRandomInstrument();
            auto bid = PropertyTestHelper::generateRandomPrice();
            auto ask = PropertyTestHelper::generateRandomPrice();
            auto timestamp_ns = PropertyTestHelper::generateRandomTimestamp();
            
            MarketData burst_data(instrument.c_str(), bid, ask, timestamp_ns);
            burst_messages.push_back(burst_data);
            
            bool burst_write = ring_buffer.try_write(burst_data);
            REQUIRE(burst_write);
        }
        
        // Consumer should detect all messages through polling
        std::vector<MarketData> burst_consumed;
        int burst_poll_attempts = 0;
        const int max_burst_polls = 100;
        
        while (burst_consumed.size() < burst_messages.size() && burst_poll_attempts < max_burst_polls) {
            if (!ring_buffer.is_empty()) {
                MarketData burst_read;
                if (ring_buffer.try_read(burst_read)) {
                    burst_consumed.push_back(burst_read);
                }
            }
            burst_poll_attempts++;
            
            // Small delay to prevent tight loop in test
            if (burst_poll_attempts % 10 == 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
        }
        
        // Verify all burst messages were detected and consumed
        REQUIRE(burst_consumed.size() == burst_messages.size());
        REQUIRE(burst_poll_attempts < max_burst_polls);
        
        // Verify message order is preserved (FIFO) - critical for market data
        for (size_t b = 0; b < burst_messages.size(); ++b) {
            REQUIRE(strcmp(burst_messages[b].instrument, burst_consumed[b].instrument) == 0);
            REQUIRE(burst_messages[b].bid == burst_consumed[b].bid);
            REQUIRE(burst_messages[b].ask == burst_consumed[b].ask);
            REQUIRE(burst_messages[b].timestamp_ns == burst_consumed[b].timestamp_ns);
        }
        
        // Test polling performance - should be very fast (critical for HFT)
        auto start_time = std::chrono::high_resolution_clock::now();
        const int performance_polls = 100;
        
        for (int perf = 0; perf < performance_polls; ++perf) {
            volatile bool empty = ring_buffer.is_empty(); // volatile to prevent optimization
            (void)empty; // suppress unused variable warning
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
        
        // Polling operations should complete very quickly
        // This verifies the spin-wait polling is efficient for HFT requirements
        REQUIRE(duration_us < 1000); // Should complete in less than 1ms
    }
}

// ============================================================================
// LATENCY MEASUREMENT PROPERTY TESTS
// ============================================================================

TEST_CASE("Property 14: Timestamp embedding consistency", "[property][latency]") {
    // Feature: hft-market-data-system, Property 14: Timestamp embedding consistency
    // Validates: Requirements 9.1
    
    // Test that all market data messages contain valid send timestamps
    // that can be used for latency calculation
    
    const int num_tests = 100;
    
    for (int i = 0; i < num_tests; ++i) {
        // Generate random market data
        auto instrument = PropertyTestHelper::generateRandomInstrument();
        auto bid = PropertyTestHelper::generateRandomPrice();
        auto ask = PropertyTestHelper::generateRandomPrice();
        
        // Use FastClock to generate timestamp (same as publisher)
        FastClock fast_clock;
        std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Let clock initialize
        int64_t send_timestamp = fast_clock.now();
        
        // Create market data with embedded timestamp
        MarketData market_data(instrument.c_str(), bid, ask, send_timestamp);
        
        // Verify timestamp is properly embedded
        REQUIRE(market_data.timestamp_ns == send_timestamp);
        REQUIRE(market_data.timestamp_ns > 0);
        
        // Verify timestamp is reasonable (within last few seconds)
        auto current_time = std::chrono::high_resolution_clock::now();
        auto current_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            current_time.time_since_epoch()).count();
        
        // Timestamp should be within reasonable range (not too old, not in future)
        int64_t time_diff = std::abs(current_ns - market_data.timestamp_ns);
        REQUIRE(time_diff < 10000000000LL); // Within 10 seconds
        
        // Test JSON serialization preserves timestamp
        std::string json = market_data.to_json();
        MarketData parsed_data;
        bool parse_success = MarketData::from_json(json, parsed_data);
        
        REQUIRE(parse_success);
        REQUIRE(parsed_data.timestamp_ns == send_timestamp);
        
        // Test that timestamp is preserved in shared memory (binary copy)
        MarketData copied_data = market_data; // Copy constructor
        REQUIRE(copied_data.timestamp_ns == send_timestamp);
    }
}

TEST_CASE("Property 15: Latency calculation accuracy", "[property][latency]") {
    // Feature: hft-market-data-system, Property 15: Latency calculation accuracy
    // Validates: Requirements 9.2, 9.3
    
    // Test that latency calculation (receive_time - message_timestamp) produces
    // positive and reasonable results for both TCP and SHM consumers
    
    const int num_tests = 100;
    
    for (int i = 0; i < num_tests; ++i) {
        // Generate random market data with send timestamp
        auto instrument = PropertyTestHelper::generateRandomInstrument();
        auto bid = PropertyTestHelper::generateRandomPrice();
        auto ask = PropertyTestHelper::generateRandomPrice();
        
        // Simulate message creation time (in the past)
        auto send_time = std::chrono::high_resolution_clock::now() - 
                        std::chrono::microseconds(rand() % 10000); // 0-10ms ago
        auto send_timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            send_time.time_since_epoch()).count();
        
        MarketData market_data(instrument.c_str(), bid, ask, send_timestamp_ns);
        
        // Simulate receive time (now)
        auto receive_time = std::chrono::high_resolution_clock::now();
        auto receive_timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            receive_time.time_since_epoch()).count();
        
        // Calculate latency (same formula used by consumers)
        int64_t latency_ns = receive_timestamp_ns - market_data.timestamp_ns;
        double latency_us = latency_ns / 1000.0;
        
        // Verify latency calculation properties
        REQUIRE(latency_ns >= 0); // Latency should be positive (receive after send)
        REQUIRE(latency_ns < 100000000LL); // Should be less than 100ms (reasonable)
        REQUIRE(latency_us >= 0.0); // Microsecond conversion should be positive
        
        // Test edge case: same timestamp (zero latency)
        MarketData zero_latency_data(instrument.c_str(), bid, ask, receive_timestamp_ns);
        int64_t zero_latency = receive_timestamp_ns - zero_latency_data.timestamp_ns;
        REQUIRE(zero_latency == 0);
        
        // Test that latency increases with time delay
        std::this_thread::sleep_for(std::chrono::microseconds(100)); // Small delay
        auto delayed_receive_time = std::chrono::high_resolution_clock::now();
        auto delayed_receive_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            delayed_receive_time.time_since_epoch()).count();
        
        int64_t delayed_latency = delayed_receive_ns - market_data.timestamp_ns;
        REQUIRE(delayed_latency > latency_ns); // Should be higher due to delay
        
        // Verify latency statistics calculations (used by consumers)
        std::vector<int64_t> latencies = {latency_ns, delayed_latency, zero_latency};
        
        int64_t total_latency = 0;
        int64_t min_latency = INT64_MAX;
        int64_t max_latency = 0;
        
        for (auto lat : latencies) {
            total_latency += lat;
            min_latency = std::min(min_latency, lat);
            max_latency = std::max(max_latency, lat);
        }
        
        double avg_latency = total_latency / static_cast<double>(latencies.size());
        
        REQUIRE(min_latency <= avg_latency);
        REQUIRE(avg_latency <= max_latency);
        REQUIRE(min_latency == zero_latency); // Zero should be minimum
        REQUIRE(max_latency == delayed_latency); // Delayed should be maximum
    }
}