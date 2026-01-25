# Requirements Document

## Introduction

A high-frequency trading (HFT) market data publishing system designed for educational purposes to demonstrate low-latency data distribution patterns in modern C++17, integrated with comprehensive DevOps CI/CD practices. The system distributes real-time market data through both TCP and shared memory channels to multiple consumers with microsecond-level latency requirements, while maintaining production-grade security, quality, and deployment automation.

## Glossary

- **Publisher**: The main process (Process A) that generates and distributes market data
- **TCP_Consumer**: A process (Process C) that receives market data via TCP connection
- **SHM_Consumer**: A process (Process B) that receives market data via shared memory
- **MarketData**: A structured data type containing instrument, bid, ask, and timestamp information
- **Ring_Buffer**: A lock-free circular buffer for shared memory communication
- **Fast_Clock**: A high-performance timestamp mechanism avoiding syscall overhead
- **CI_Pipeline**: Continuous Integration pipeline for automated testing and quality assurance
- **CD_Pipeline**: Continuous Deployment pipeline for automated application deployment
- **SAST**: Static Application Security Testing for code vulnerability detection
- **SCA**: Software Composition Analysis for dependency vulnerability scanning

## Requirements

### Requirement 1: Market Data Generation

**User Story:** As a system operator, I want the publisher to generate realistic market data, so that consumers can process representative trading information.

#### Acceptance Criteria

1. THE Publisher SHALL generate market data with instrument names, bid prices, ask prices, and nanosecond timestamps
2. WHEN generating market data, THE Publisher SHALL create at least 1000 different quote messages
3. THE Publisher SHALL use fixed-size character arrays for instrument names to avoid heap allocation
4. THE Publisher SHALL embed nanosecond-precision timestamps in each message
5. THE Publisher SHALL generate bid and ask prices using deterministic random values

### Requirement 2: TCP Distribution Channel

**User Story:** As a market data consumer, I want to receive data via TCP connection, so that I can process market updates from remote locations.

#### Acceptance Criteria

1. THE Publisher SHALL bind to TCP address 127.0.0.1 port 9000
2. WHEN a TCP client connects, THE Publisher SHALL accept the connection and begin streaming data
3. THE Publisher SHALL serialize market data to JSON format for TCP transmission
4. WHEN a TCP client disconnects, THE Publisher SHALL handle the disconnection gracefully
5. THE Publisher SHALL send market data in a continuous loop over the TCP connection

### Requirement 3: TCP Consumer Implementation

**User Story:** As a market data analyst, I want a TCP consumer process, so that I can receive and log market data over network connections.

#### Acceptance Criteria

1. THE TCP_Consumer SHALL connect to the Publisher at 127.0.0.1:9000
2. WHEN receiving data, THE TCP_Consumer SHALL parse JSON messages from the TCP stream
3. THE TCP_Consumer SHALL handle partial reads and message boundaries correctly
4. THE TCP_Consumer SHALL log received messages with formatted output including timestamps
5. WHEN connection fails, THE TCP_Consumer SHALL handle errors gracefully

### Requirement 4: Shared Memory Ring Buffer

**User Story:** As a low-latency trading system, I want shared memory communication, so that I can achieve microsecond-level data distribution.

#### Acceptance Criteria

1. THE Ring_Buffer SHALL implement a single-producer single-consumer lock-free design
2. THE Ring_Buffer SHALL use atomic operations with proper memory ordering semantics
3. THE Ring_Buffer SHALL align data structures to 64-byte boundaries to prevent false sharing
4. WHEN the buffer is full, THE Ring_Buffer SHALL indicate overflow condition to the producer
5. WHEN the buffer is empty, THE Ring_Buffer SHALL indicate underflow condition to the consumer
6. THE Ring_Buffer SHALL use acquire-release memory ordering for correctness

### Requirement 5: Shared Memory Management

**User Story:** As a system administrator, I want proper shared memory lifecycle management, so that resources are allocated and cleaned up correctly.

#### Acceptance Criteria

1. THE Publisher SHALL create named shared memory using shm_open system call
2. THE Publisher SHALL set appropriate size using ftruncate system call
3. THE Publisher SHALL map shared memory into process address space using mmap
4. WHEN the Publisher terminates, THE system SHALL clean up shared memory resources
5. THE SHM_Consumer SHALL attach to existing shared memory in read-only mode

### Requirement 6: Shared Memory Consumer

**User Story:** As a high-frequency trader, I want a shared memory consumer, so that I can receive market data with minimal latency.

#### Acceptance Criteria

1. THE SHM_Consumer SHALL attach to the Publisher's shared memory segment
2. THE SHM_Consumer SHALL poll the ring buffer for new messages using spin-wait
3. WHEN new data is available, THE SHM_Consumer SHALL read and log the market data
4. THE SHM_Consumer SHALL calculate and display latency measurements
5. THE SHM_Consumer SHALL handle ring buffer empty conditions without blocking

### Requirement 7: High-Performance Timestamping

**User Story:** As a latency-sensitive application, I want fast timestamp generation, so that I can avoid syscall overhead in the hot path.

#### Acceptance Criteria

1. THE Fast_Clock SHALL provide nanosecond-precision timestamps without syscalls in hot path
2. THE Fast_Clock SHALL use a background thread to periodically update cached time
3. THE Fast_Clock SHALL update the cached timestamp every 200 milliseconds maximum
4. THE Fast_Clock SHALL store timestamps in atomic variables for thread-safe access
5. THE Publisher SHALL use Fast_Clock for message timestamping

### Requirement 8: Performance Optimizations

**User Story:** As a performance engineer, I want system-level optimizations, so that I can achieve maximum throughput and minimum latency.

#### Acceptance Criteria

1. THE Publisher SHALL disable Nagle's algorithm using TCP_NODELAY socket option
2. THE Publisher SHALL set CPU affinity to bind processes to specific cores
3. THE Publisher SHALL configure socket buffer sizes for optimal throughput
4. THE system SHALL use proper memory alignment and cache-line padding
5. THE system SHALL avoid unnecessary memory allocations in hot paths

### Requirement 9: Latency Measurement and Comparison

**User Story:** As a system analyst, I want latency measurements, so that I can compare TCP versus shared memory performance.

#### Acceptance Criteria

1. THE Publisher SHALL embed send timestamps in all market data messages
2. THE TCP_Consumer SHALL calculate receive latency as receive_time minus message timestamp
3. THE SHM_Consumer SHALL calculate receive latency as receive_time minus message timestamp
4. THE consumers SHALL log latency measurements for performance analysis
5. THE system SHALL demonstrate measurable latency difference between TCP and shared memory

### Requirement 10: Build System and Dependencies

**User Story:** As a developer, I want a proper build system, so that I can compile and manage dependencies easily.

#### Acceptance Criteria

1. THE build system SHALL use CMake with C++17 standard
2. THE build system SHALL find and link Boost 1.85 libraries
3. THE build system SHALL include nlohmann/json for JSON serialization
4. THE build system SHALL include fmt library for formatted logging
5. THE build system SHALL produce separate executables for publisher and consumers

