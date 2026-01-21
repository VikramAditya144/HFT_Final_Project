#include <catch2/catch_test_macros.hpp>
#include <common/shared_memory.hpp>
#include <sys/wait.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <random>
#include <sstream>

using namespace hft;

// Test data structure for shared memory testing
struct TestData {
    int value;
    char message[32];
    double price;
};

// Helper function to generate unique test names
std::string generate_unique_name(const std::string& base) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(1000, 9999);
    
    std::ostringstream oss;
    oss << base << "_" << dis(gen) << "_" << getpid();
    return oss.str();
}

TEST_CASE("SharedMemoryManager creation and basic operations", "[shared_memory][unit]") {
    const std::string test_name = generate_unique_name("test_shm_basic");
    const size_t test_size = sizeof(TestData);
    
    SECTION("Create new shared memory segment") {
        SharedMemoryManager shm(test_name, test_size, true);
        
        REQUIRE(shm.is_valid());
        REQUIRE(shm.get_address() != MAP_FAILED);
        REQUIRE(shm.get_size() == test_size);
        REQUIRE(shm.get_name() == "/" + test_name);
        REQUIRE(shm.is_creator());
        
        // Test writing to shared memory
        TestData* data = static_cast<TestData*>(shm.get_address());
        data->value = 42;
        strcpy(data->message, "Hello SharedMemory");
        data->price = 123.45;
        
        // Verify data was written correctly
        REQUIRE(data->value == 42);
        REQUIRE(strcmp(data->message, "Hello SharedMemory") == 0);
        REQUIRE(data->price == 123.45);
    }
    
    SECTION("Attach to existing shared memory segment") {
        const std::string attach_test_name = generate_unique_name("test_shm_attach");
        
        // First create a shared memory segment
        std::string segment_name;
        {
            SharedMemoryManager creator(attach_test_name, test_size, true);
            REQUIRE(creator.is_valid());
            segment_name = creator.get_name();
            
            // Write test data
            TestData* data = static_cast<TestData*>(creator.get_address());
            data->value = 99;
            strcpy(data->message, "Test Message");
            data->price = 456.78;
            
            // Keep the creator alive while we test attachment
            // Now attach to the existing segment (read-only)
            SharedMemoryManager reader(attach_test_name, test_size, false);
            
            REQUIRE(reader.is_valid());
            REQUIRE(reader.get_address() != MAP_FAILED);
            REQUIRE(reader.get_size() == test_size);
            REQUIRE(reader.get_name() == "/" + attach_test_name);
            REQUIRE_FALSE(reader.is_creator());
            
            // Verify we can read the data written by creator
            const TestData* read_data = static_cast<const TestData*>(reader.get_address());
            REQUIRE(read_data->value == 99);
            REQUIRE(strcmp(read_data->message, "Test Message") == 0);
            REQUIRE(read_data->price == 456.78);
        }
        // Creator goes out of scope and cleans up the segment
    }
}

TEST_CASE("SharedMemoryManager RAII resource management", "[shared_memory][unit]") {
    const std::string test_name = generate_unique_name("test_shm_raii");
    const size_t test_size = 1024;
    
    SECTION("Automatic cleanup on destruction") {
        const std::string cleanup_test_name = generate_unique_name("test_cleanup");
        {
            SharedMemoryManager shm(cleanup_test_name, test_size, true);
            REQUIRE(shm.is_valid());
            
            // Write some data to verify the segment exists
            int* data = static_cast<int*>(shm.get_address());
            *data = 12345;
            REQUIRE(*data == 12345);
        } // shm goes out of scope here, should clean up automatically
        
        // Try to attach to the segment - should fail because it was cleaned up
        REQUIRE_THROWS_AS(SharedMemoryManager(cleanup_test_name, test_size, false), std::runtime_error);
    }
    
    SECTION("Move constructor transfers ownership") {
        SharedMemoryManager original(test_name, test_size, true);
        REQUIRE(original.is_valid());
        REQUIRE(original.is_creator());
        
        // Write test data
        int* data = static_cast<int*>(original.get_address());
        *data = 54321;
        
        // Move construct
        SharedMemoryManager moved = std::move(original);
        
        // Verify moved object has ownership
        REQUIRE(moved.is_valid());
        REQUIRE(moved.is_creator());
        REQUIRE(moved.get_size() == test_size);
        REQUIRE(moved.get_name() == "/" + test_name);
        
        // Verify data is still accessible through moved object
        int* moved_data = static_cast<int*>(moved.get_address());
        REQUIRE(*moved_data == 54321);
        
        // Original should be in moved-from state
        REQUIRE_FALSE(original.is_valid());
        REQUIRE(original.get_address() == MAP_FAILED);
        REQUIRE_FALSE(original.is_creator());
    }
    
    SECTION("Move assignment transfers ownership") {
        const std::string move_test_name = generate_unique_name("test_move");
        SharedMemoryManager original(move_test_name, test_size, true);
        REQUIRE(original.is_valid());
        
        // Write test data
        int* data = static_cast<int*>(original.get_address());
        *data = 98765;
        
        // Create another object to move assign to
        const std::string temp_name = generate_unique_name("temp");
        SharedMemoryManager target(temp_name, 512, true);
        REQUIRE(target.is_valid());
        
        // Move assign
        target = std::move(original);
        
        // Verify target now has ownership of original's memory
        REQUIRE(target.is_valid());
        REQUIRE(target.is_creator());
        REQUIRE(target.get_size() == test_size);
        REQUIRE(target.get_name() == "/" + move_test_name);
        
        // Verify data is accessible
        int* target_data = static_cast<int*>(target.get_address());
        REQUIRE(*target_data == 98765);
        
        // Original should be in moved-from state
        REQUIRE_FALSE(original.is_valid());
    }
}

TEST_CASE("SharedMemoryManager error conditions", "[shared_memory][unit]") {
    const std::string test_name = generate_unique_name("test_shm_errors");
    const size_t test_size = 1024;
    
    SECTION("Attach to non-existent shared memory") {
        // Try to attach to a segment that doesn't exist
        const std::string nonexistent_name = generate_unique_name("nonexistent_segment");
        REQUIRE_THROWS_AS(SharedMemoryManager(nonexistent_name, test_size, false), std::runtime_error);
    }
    
    SECTION("Invalid shared memory name") {
        // Test with empty name
        REQUIRE_THROWS_AS(SharedMemoryManager("", test_size, true), std::runtime_error);
    }
    
    SECTION("Zero size shared memory") {
        // Test with zero size
        REQUIRE_THROWS_AS(SharedMemoryManager(test_name, 0, true), std::runtime_error);
    }
    
    SECTION("Multiple creators for same segment") {
        const std::string multi_test_name = generate_unique_name("test_multi");
        
        // Create first segment
        SharedMemoryManager first(multi_test_name, test_size, true);
        REQUIRE(first.is_valid());
        
        // Write some data
        int* first_data = static_cast<int*>(first.get_address());
        *first_data = 12345;
        
        // Create second manager for the same segment while first is still alive
        // This should succeed and both should share the same underlying memory
        SharedMemoryManager second(multi_test_name, test_size, true);
        REQUIRE(second.is_valid());
        
        // Both should see the same data since they map to the same segment
        int* second_data = static_cast<int*>(second.get_address());
        REQUIRE(*second_data == 12345);
        
        // Modify through second manager
        *second_data = 54321;
        
        // First manager should see the change
        REQUIRE(*first_data == 54321);
    }
}

TEST_CASE("SharedMemoryManager edge cases", "[shared_memory][unit]") {
    const std::string test_name = generate_unique_name("test_shm_edge");
    
    SECTION("Large shared memory segment") {
        const size_t large_size = 1024 * 1024; // 1MB
        
        SharedMemoryManager shm(test_name, large_size, true);
        REQUIRE(shm.is_valid());
        REQUIRE(shm.get_size() == large_size);
        
        // Test writing to beginning and end of large segment
        char* data = static_cast<char*>(shm.get_address());
        data[0] = 'A';
        data[large_size - 1] = 'Z';
        
        REQUIRE(data[0] == 'A');
        REQUIRE(data[large_size - 1] == 'Z');
    }
    
    SECTION("Reasonably long segment name") {
        // Test with a reasonably long name (but within system limits)
        // Keep it shorter to avoid system limits
        std::string long_name = generate_unique_name("test_long_name");
        const size_t test_size = 512;
        
        SharedMemoryManager shm(long_name, test_size, true);
        REQUIRE(shm.is_valid());
        REQUIRE(shm.get_name() == "/" + long_name);
    }
    
    SECTION("Concurrent access simulation") {
        const std::string concurrent_name = generate_unique_name("test_concurrent");
        const size_t test_size = sizeof(int) * 100;
        
        SharedMemoryManager shm(concurrent_name, test_size, true);
        REQUIRE(shm.is_valid());
        
        // Initialize array in shared memory
        int* array = static_cast<int*>(shm.get_address());
        for (int i = 0; i < 100; ++i) {
            array[i] = i;
        }
        
        // Verify all values are correct
        for (int i = 0; i < 100; ++i) {
            REQUIRE(array[i] == i);
        }
        
        // Simulate concurrent modification
        for (int i = 0; i < 100; ++i) {
            array[i] = array[i] * 2;
        }
        
        // Verify modifications
        for (int i = 0; i < 100; ++i) {
            REQUIRE(array[i] == i * 2);
        }
    }
}

TEST_CASE("SharedMemoryManager copy prevention", "[shared_memory][unit]") {
    const std::string test_name = generate_unique_name("test_shm_copy");
    const size_t test_size = 1024;
    
    SECTION("Copy constructor is deleted") {
        SharedMemoryManager original(test_name, test_size, true);
        REQUIRE(original.is_valid());
        
        // This should not compile - copy constructor is deleted
        // SharedMemoryManager copy(original); // Would cause compilation error
        
        // Verify we can't accidentally copy
        static_assert(!std::is_copy_constructible_v<SharedMemoryManager>, 
                     "SharedMemoryManager should not be copy constructible");
    }
    
    SECTION("Copy assignment is deleted") {
        const std::string copy_test_name = generate_unique_name("test_copy_assign");
        SharedMemoryManager original(copy_test_name, test_size, true);
        const std::string temp_name = generate_unique_name("temp");
        SharedMemoryManager target(temp_name, 512, true);
        
        REQUIRE(original.is_valid());
        REQUIRE(target.is_valid());
        
        // This should not compile - copy assignment is deleted
        // target = original; // Would cause compilation error
        
        // Verify we can't accidentally copy assign
        static_assert(!std::is_copy_assignable_v<SharedMemoryManager>, 
                     "SharedMemoryManager should not be copy assignable");
    }
}