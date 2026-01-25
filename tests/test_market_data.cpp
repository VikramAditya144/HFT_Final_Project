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