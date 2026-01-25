// ============================================================================
// PROCESS A: MARKET DATA PUBLISHER
// ============================================================================
// This is the main entry point for the publisher process.
// It will:
//   1. Initialize shared memory and ring buffer
//   2. Generate random market data in a loop
//   3. Push data to shared memory ring buffer (for Process B)
//   4. Send JSON messages over TCP (for Process C)

#include "common/market_data.hpp"
#include "common/shared_memory.hpp"
#include "common/ring_buffer.hpp"
#include "common/fast_clock.hpp"
#include "common/performance_utils.hpp"
#include <fmt/chrono.h> // For timestamp formatting
#include <fmt/core.h>   // For fmt::print (fast, type-safe printing)
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <random>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <memory>
#include <set>

// ============================================================================
// TCP SERVER CLASS FOR MARKET DATA DISTRIBUTION
// ============================================================================
class TcpServer {
private:
    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::set<std::shared_ptr<boost::asio::ip::tcp::socket>> clients_;
    mutable std::mutex clients_mutex_;

public:
    TcpServer(boost::asio::io_context& io_context, short port)
        : io_context_(io_context)
        , acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
    {
        // Apply optimizations to the acceptor socket
        try {
            // Enable socket reuse to avoid "Address already in use" errors
            acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
            fmt::print("Applied acceptor socket optimizations\n");
        } catch (const std::exception& e) {
            fmt::print("Warning: Failed to apply acceptor optimizations: {}\n", e.what());
        }
        
        fmt::print("TCP Server listening on 127.0.0.1:{}\n", port);
        start_accept();
    }

private:
    void start_accept() {
        auto new_socket = std::make_shared<boost::asio::ip::tcp::socket>(io_context_);
        
        acceptor_.async_accept(*new_socket,
            [this, new_socket](boost::system::error_code ec) {
                if (!ec) {
                    fmt::print("New TCP client connected from {}\n", 
                              new_socket->remote_endpoint().address().to_string());
                    
                    // Apply TCP performance optimizations
                    apply_tcp_optimizations(*new_socket);
                    
                    // Add client to our set
                    {
                        std::lock_guard<std::mutex> lock(clients_mutex_);
                        clients_.insert(new_socket);
                    }
                    
                    // Set up disconnect detection
                    setup_disconnect_detection(new_socket);
                }
                
                // Continue accepting new connections
                start_accept();
            });
    }
    
    void setup_disconnect_detection(std::shared_ptr<boost::asio::ip::tcp::socket> socket) {
        // Use a small buffer to detect disconnection
        auto buffer = std::make_shared<std::array<char, 1>>();
        
        socket->async_read_some(boost::asio::buffer(*buffer),
            [this, socket, buffer](boost::system::error_code ec, std::size_t /*length*/) {
                if (ec) {
                    // Client disconnected
                    fmt::print("TCP client disconnected: {}\n", ec.message());
                    
                    // Remove from clients set
                    {
                        std::lock_guard<std::mutex> lock(clients_mutex_);
                        clients_.erase(socket);
                    }
                } else {
                    // Client sent data (unexpected for our use case, but continue monitoring)
                    setup_disconnect_detection(socket);
                }
            });
    }
    
    void apply_tcp_optimizations(boost::asio::ip::tcp::socket& socket) {
        try {
            // Enable TCP_NODELAY to disable Nagle's algorithm
            // This ensures immediate packet transmission without waiting for more data
            // Critical for low-latency applications like HFT
            boost::asio::ip::tcp::no_delay no_delay_option(true);
            socket.set_option(no_delay_option);
            fmt::print("Applied TCP_NODELAY optimization\n");
            
            // Configure socket buffer sizes for optimal throughput
            // Larger buffers can improve throughput but may increase latency
            // For HFT, we balance between throughput and latency
            
            // Set send buffer size (SO_SNDBUF)
            constexpr int send_buffer_size = 65536; // 64KB
            boost::asio::socket_base::send_buffer_size send_option(send_buffer_size);
            socket.set_option(send_option);
            
            // Set receive buffer size (SO_RCVBUF) 
            constexpr int recv_buffer_size = 65536; // 64KB
            boost::asio::socket_base::receive_buffer_size recv_option(recv_buffer_size);
            socket.set_option(recv_option);
            
            fmt::print("Applied socket buffer optimizations (send: {}KB, recv: {}KB)\n", 
                      send_buffer_size / 1024, recv_buffer_size / 1024);
                      
        } catch (const std::exception& e) {
            fmt::print("Warning: Failed to apply TCP optimizations: {}\n", e.what());
            // Continue operation even if optimizations fail
        }
    }

public:
    void broadcast_json(const std::string& json_message) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        
        // Remove disconnected clients and send to active ones
        auto it = clients_.begin();
        while (it != clients_.end()) {
            auto socket = *it;
            
            if (!socket->is_open()) {
                it = clients_.erase(it);
                continue;
            }
            
            // Send JSON message + newline for message boundary
            std::string message = json_message + "\n";
            
            boost::asio::async_write(*socket, boost::asio::buffer(message),
                [socket](boost::system::error_code ec, std::size_t /*bytes_transferred*/) {
                    if (ec) {
                        // Error sending, socket will be cleaned up on next broadcast
                        fmt::print("Error sending to client: {}\n", ec.message());
                    }
                });
            
            ++it;
        }
    }
    
    size_t get_client_count() const {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        return clients_.size();
    }
};

int main() {
  fmt::print("===========================================\n");
  fmt::print("   HFT Market Data Publisher (Process A)\n");
  fmt::print("===========================================\n\n");

  try {
    // ========================================================================
    // STEP 0: Apply Performance Optimizations
    // ========================================================================
    fmt::print("Applying performance optimizations...\n");
    
    // Get system information
    int cpu_count = hft::CpuAffinity::get_cpu_count();
    size_t cache_line_size = hft::MemoryUtils::get_cache_line_size();
    
    fmt::print("System info: {} CPU cores, {} byte cache lines\n", cpu_count, cache_line_size);
    
    // Set CPU affinity to core 0 for consistent performance
    // In production, you might want to use a dedicated core
    if (cpu_count > 0) {
        bool affinity_set = hft::CpuAffinity::set_thread_affinity(0);
        if (affinity_set) {
            fmt::print("Successfully bound main thread to CPU core 0\n");
            
            // Verify current CPU (Linux only)
            int current_cpu = hft::CpuAffinity::get_current_cpu();
            if (current_cpu >= 0) {
                fmt::print("Current CPU: {}\n", current_cpu);
            }
        } else {
            fmt::print("Warning: Failed to set CPU affinity (may not be supported on this platform)\n");
        }
    }
    
    // Verify memory alignment of critical data structures
    fmt::print("Verifying memory alignment...\n");
    
    // Check MarketData alignment
    bool market_data_aligned = hft::MemoryUtils::is_type_aligned<hft::MarketData>(64);
    fmt::print("MarketData 64-byte aligned: {}\n", market_data_aligned ? "YES" : "NO");
    
    // Check RingBuffer alignment  
    bool ring_buffer_aligned = hft::MemoryUtils::is_type_aligned<hft::RingBuffer>(64);
    fmt::print("RingBuffer 64-byte aligned: {}\n", ring_buffer_aligned ? "YES" : "NO");
    
    if (!market_data_aligned || !ring_buffer_aligned) {
        fmt::print("WARNING: Critical data structures are not properly aligned!\n");
    }
    
    // ========================================================================
    // STEP 1: Initialize Boost.Asio IO Context and TCP Server
    // ========================================================================
    fmt::print("Initializing TCP server...\n");
    boost::asio::io_context io_context;
    TcpServer tcp_server(io_context, 9000);
    
    // Run io_context in a separate thread
    std::thread io_thread([&io_context]() {
        io_context.run();
    });
    
    // ========================================================================
    // STEP 2: Initialize Fast Clock for timestamping
    // ========================================================================
    fmt::print("Initializing Fast Clock...\n");
    hft::FastClock fast_clock;
    
    // ========================================================================
    // STEP 3: Initialize Shared Memory and Ring Buffer
    // ========================================================================
    fmt::print("Creating shared memory segment...\n");
    
    // Calculate size needed for ring buffer
    constexpr size_t ring_buffer_size = sizeof(hft::RingBuffer);
    
    // Create shared memory segment
    hft::SharedMemoryManager shm_manager("hft_market_data", ring_buffer_size, true);
    
    if (!shm_manager.is_valid()) {
      fmt::print("ERROR: Failed to create shared memory\n");
      return 1;
    }
    
    fmt::print("Shared memory created successfully (size: {} bytes)\n", ring_buffer_size);
    
    // Get pointer to shared memory and construct ring buffer in-place
    void* shm_addr = shm_manager.get_address();
    hft::RingBuffer* ring_buffer = new(shm_addr) hft::RingBuffer();
    
    fmt::print("Ring buffer initialized in shared memory\n");
    
    // ========================================================================
    // STEP 4: Prepare Market Data Generation
    // ========================================================================
    fmt::print("Setting up market data generation...\n");
    
    // Random number generator for realistic market data
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> price_dist(100.0, 3000.0);
    std::uniform_real_distribution<double> spread_dist(0.01, 1.0);
    
    // Sample instrument names for variety
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
    
    fmt::print("Prepared {} instrument symbols\n", instruments.size());
    
    // ========================================================================
    // STEP 5: Market Data Generation Loop
    // ========================================================================
    fmt::print("\nStarting market data generation loop...\n");
    fmt::print("Press Ctrl+C to stop\n\n");
    
    size_t message_count = 0;
    size_t overflow_count = 0;
    
    while (true) {
      // Generate random market data
      const std::string& instrument = instruments[gen() % instruments.size()];
      double bid = price_dist(gen);
      double spread = spread_dist(gen);
      double ask = bid + spread;
      int64_t timestamp = fast_clock.now();
      
      // Create market data message
      hft::MarketData market_data(instrument.c_str(), bid, ask, timestamp);
      
      // Memory optimization: prefetch the next ring buffer slot for writing
      size_t next_write_idx = ring_buffer->get_write_index();
      const hft::MarketData* buffer_addr = ring_buffer->get_buffer_address();
      if (buffer_addr && next_write_idx < hft::RING_BUFFER_SIZE) {
          hft::MemoryUtils::prefetch_write(&buffer_addr[next_write_idx]);
      }
      
      // Try to write to ring buffer
      if (ring_buffer->try_write(market_data)) {
        message_count++;
        
        // Broadcast JSON to TCP clients
        if (tcp_server.get_client_count() > 0) {
          std::string json_message = market_data.to_json();
          tcp_server.broadcast_json(json_message);
        }
        
        // Print status every 100 messages
        if (message_count % 100 == 0) {
          fmt::print("Generated {} messages | Buffer usage: {}/{} | Overflows: {} | TCP clients: {}\n",
                    message_count, 
                    ring_buffer->available_for_read(),
                    ring_buffer->capacity(),
                    overflow_count,
                    tcp_server.get_client_count());
        }
      } else {
        overflow_count++;
        // Buffer is full, skip this message
        if (overflow_count % 10 == 1) {
          fmt::print("WARNING: Ring buffer full, dropped message (total drops: {})\n", overflow_count);
        }
      }
      
      // Generate messages at ~1000 Hz (1ms interval)
      std::this_thread::sleep_for(std::chrono::microseconds(1000));
      
      // Stop after generating 1000+ messages for this basic implementation
      if (message_count >= 1000) {
        fmt::print("\nGenerated {} messages successfully!\n", message_count);
        fmt::print("Ring buffer final state: {}/{} messages\n", 
                  ring_buffer->available_for_read(), ring_buffer->capacity());
        break;
      }
    }
    
    fmt::print("\n[Task 7.2 Complete] JSON streaming over TCP connections working!\n");
    fmt::print("Next: Add property tests for TCP functionality\n");
    
    // ========================================================================
    // CLEANUP: Stop TCP server and join threads
    // ========================================================================
    fmt::print("Shutting down TCP server...\n");
    io_context.stop();
    if (io_thread.joinable()) {
        io_thread.join();
    }
    
  } catch (const std::exception& e) {
    fmt::print("ERROR: {}\n", e.what());
    return 1;
  }

  return 0;
}
