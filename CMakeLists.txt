cmake_minimum_required(VERSION 3.16)
project(HFT_MarketData 
    VERSION 1.0.0 
    LANGUAGES CXX
)

# ============================================================================
# C++ STANDARD CONFIGURATION
# ============================================================================
# We use C++17 for:
#   - std::optional, std::string_view (cleaner APIs)
#   - Structured bindings (auto [a, b] = ...)
#   - if constexpr (compile-time branching)
#   - Inline variables (useful for header-only code)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)  # Fail if C++17 not available
set(CMAKE_CXX_EXTENSIONS OFF)        # Use -std=c++17, not -std=gnu++17

# ============================================================================
# BUILD TYPE (Debug vs Release)
# ============================================================================
# If user didn't specify, default to Release for performance testing
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags for optimization
# -O3: Aggressive optimization
# -march=native: Use all CPU features available on this machine
# -Wall -Wextra: Enable all warnings (good practice!)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")

# ============================================================================
# FIND DEPENDENCIES
# ============================================================================

# --- Boost ---
# Modern Boost.Asio (since ~1.66) is HEADER-ONLY
# We don't need to link any Boost libraries!
# Just need to find the headers
find_package(Boost 1.80 REQUIRED)

# If Boost is found, Boost_INCLUDE_DIRS contains the header path
# We use BOOST_ASIO_HAS_STD_CHRONO to use C++ standard chrono

# --- Threads ---
# Required for std::thread, std::atomic, pthread
find_package(Threads REQUIRED)

# ============================================================================
# FETCH EXTERNAL DEPENDENCIES
# ============================================================================
include(FetchContent)

# --- fmt (Fast formatting library) ---
# Using system-installed fmt from Homebrew (faster than downloading)
# If not found, we'll download it
find_package(fmt QUIET)
if(NOT fmt_FOUND)
    message(STATUS "fmt not found, fetching from GitHub...")
    FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG        11.1.4  # Match closer to system version
    )
    FetchContent_MakeAvailable(fmt)
else()
    message(STATUS "Using system fmt: ${fmt_VERSION}")
endif()

# --- nlohmann/json (JSON for Modern C++) ---
# Header-only, easy to use JSON library
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)

# Download json only
FetchContent_MakeAvailable(json)

# --- Catch2 (Testing framework) ---
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
)
FetchContent_MakeAvailable(Catch2)

# ============================================================================
# INCLUDE DIRECTORIES
# ============================================================================
# This tells the compiler where to find our header files
# ${CMAKE_SOURCE_DIR} is the root of our project
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${Boost_INCLUDE_DIRS}
)

# ============================================================================
# EXECUTABLE TARGETS
# ============================================================================

# --- Process A: Market Data Publisher ---
add_executable(publisher
    src/publisher/main.cpp
)
target_link_libraries(publisher
    PRIVATE
        Boost::headers         # Boost.Asio is header-only
        Threads::Threads       # For threading support
        fmt::fmt               # Formatting library
        nlohmann_json::nlohmann_json  # JSON library
)

# --- Process B: Shared Memory Consumer ---
add_executable(shm_consumer
    src/shm_consumer/main.cpp
)
target_link_libraries(shm_consumer
    PRIVATE
        Threads::Threads
        fmt::fmt
        nlohmann_json::nlohmann_json
)
# On Linux, we need librt for shm_open/shm_unlink
# macOS includes these in the system library, no extra link needed
if(NOT APPLE)
    target_link_libraries(shm_consumer PRIVATE rt)
endif()

# --- Process C: TCP Consumer ---
add_executable(tcp_consumer
    src/tcp_consumer/main.cpp
)
target_link_libraries(tcp_consumer
    PRIVATE
        Boost::headers         # Boost.Asio is header-only
        Threads::Threads
        fmt::fmt
        nlohmann_json::nlohmann_json
)

# ============================================================================
# TEST TARGETS
# ============================================================================

# Enable testing
enable_testing()

# --- Property-based tests ---
add_executable(property_tests
    tests/test_market_data.cpp
)
target_link_libraries(property_tests
    PRIVATE
        Catch2::Catch2WithMain
        fmt::fmt
        nlohmann_json::nlohmann_json
)

# --- Shared memory unit tests ---
add_executable(shared_memory_tests
    tests/test_shared_memory.cpp
)
target_link_libraries(shared_memory_tests
    PRIVATE
        Catch2::Catch2WithMain
        fmt::fmt
        nlohmann_json::nlohmann_json
)

# On Linux, we need librt for shm_open/shm_unlink
# macOS includes these in the system library, no extra link needed
if(NOT APPLE)
    target_link_libraries(shared_memory_tests PRIVATE rt)
endif()

# Register the tests with CTest
add_test(NAME PropertyTests COMMAND property_tests)
add_test(NAME SharedMemoryTests COMMAND shared_memory_tests)

# ============================================================================
# SUMMARY
# ============================================================================
message(STATUS "===========================================")
message(STATUS "HFT Market Data System Configuration")
message(STATUS "===========================================")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Boost Version: ${Boost_VERSION}")
message(STATUS "===========================================")
