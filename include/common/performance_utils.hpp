#pragma once

// ============================================================================
// PERFORMANCE OPTIMIZATION UTILITIES
// ============================================================================
// This header provides utilities for CPU affinity binding and memory 
// optimization patterns commonly used in high-frequency trading systems.

#include <thread>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

#ifdef __linux__
#include <sched.h>
#include <pthread.h>
#include <unistd.h>
#elif __APPLE__
#include <mach/thread_policy.h>
#include <mach/thread_act.h>
#include <mach/mach.h>
#include <pthread.h>
#include <sys/sysctl.h>
#endif

namespace hft {

// ============================================================================
// CPU AFFINITY UTILITIES
// ============================================================================

class CpuAffinity {
public:
    /**
     * Set CPU affinity for the current thread to a specific CPU core
     * @param cpu_id The CPU core ID to bind to (0-based)
     * @return true on success, false on failure
     */
    static bool set_thread_affinity(int cpu_id) {
        try {
#ifdef __linux__
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(cpu_id, &cpuset);
            
            pthread_t current_thread = pthread_self();
            int result = pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
            
            if (result == 0) {
                return true;
            } else {
                return false;
            }
            
#elif __APPLE__
            // macOS uses thread affinity tags instead of direct CPU binding
            // This is a best-effort approach as macOS doesn't allow direct CPU pinning
            thread_affinity_policy_data_t policy = { cpu_id };
            thread_port_t mach_thread = pthread_mach_thread_np(pthread_self());
            
            kern_return_t result = thread_policy_set(mach_thread,
                                                   THREAD_AFFINITY_POLICY,
                                                   (thread_policy_t)&policy,
                                                   THREAD_AFFINITY_POLICY_COUNT);
            
            return result == KERN_SUCCESS;
#else
            // Unsupported platform
            return false;
#endif
        } catch (...) {
            return false;
        }
    }
    
    /**
     * Get the number of available CPU cores
     * @return Number of CPU cores, or 0 if unable to determine
     */
    static int get_cpu_count() {
        try {
#ifdef __linux__
            return static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
#elif __APPLE__
            int cpu_count;
            size_t size = sizeof(cpu_count);
            if (sysctlbyname("hw.ncpu", &cpu_count, &size, nullptr, 0) == 0) {
                return cpu_count;
            }
            return 0;
#else
            return static_cast<int>(std::thread::hardware_concurrency());
#endif
        } catch (...) {
            return 0;
        }
    }
    
    /**
     * Get current thread's CPU affinity (Linux only)
     * @return CPU ID if successful, -1 if unable to determine
     */
    static int get_current_cpu() {
        try {
#ifdef __linux__
            return sched_getcpu();
#else
            // Not supported on other platforms
            return -1;
#endif
        } catch (...) {
            return -1;
        }
    }
};

// ============================================================================
// MEMORY ALIGNMENT UTILITIES
// ============================================================================

class MemoryUtils {
public:
    /**
     * Verify that a pointer is aligned to the specified boundary
     * @param ptr Pointer to check
     * @param alignment Required alignment (must be power of 2)
     * @return true if aligned, false otherwise
     */
    template<typename T>
    static bool is_aligned(const T* ptr, size_t alignment) {
        if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
            return false; // alignment must be power of 2
        }
        
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        return (addr & (alignment - 1)) == 0;
    }
    
    /**
     * Verify that a type is aligned to the specified boundary
     * @param alignment Required alignment (must be power of 2)
     * @return true if aligned, false otherwise
     */
    template<typename T>
    static bool is_type_aligned(size_t alignment) {
        return alignof(T) >= alignment;
    }
    
    /**
     * Get the cache line size for the current system
     * @return Cache line size in bytes, or 64 as default
     */
    static size_t get_cache_line_size() {
        try {
#ifdef __linux__
            long cache_line_size = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
            if (cache_line_size > 0) {
                return static_cast<size_t>(cache_line_size);
            }
#elif __APPLE__
            size_t cache_line_size;
            size_t size = sizeof(cache_line_size);
            if (sysctlbyname("hw.cachelinesize", &cache_line_size, &size, nullptr, 0) == 0) {
                return cache_line_size;
            }
#endif
            // Default to 64 bytes (common for x86/x64)
            return 64;
        } catch (...) {
            return 64;
        }
    }
    
    /**
     * Prefetch memory for reading (hint to CPU)
     * @param addr Address to prefetch
     */
    static void prefetch_read(const void* addr) {
#ifdef __GNUC__
        __builtin_prefetch(addr, 0, 3); // 0 = read, 3 = high temporal locality
#elif defined(_MSC_VER)
        _mm_prefetch(static_cast<const char*>(addr), _MM_HINT_T0);
#else
        // No prefetch support, do nothing
        (void)addr;
#endif
    }
    
    /**
     * Prefetch memory for writing (hint to CPU)
     * @param addr Address to prefetch
     */
    static void prefetch_write(const void* addr) {
#ifdef __GNUC__
        __builtin_prefetch(addr, 1, 3); // 1 = write, 3 = high temporal locality
#elif defined(_MSC_VER)
        _mm_prefetch(static_cast<const char*>(addr), _MM_HINT_T0);
#else
        // No prefetch support, do nothing
        (void)addr;
#endif
    }
};

// ============================================================================
// MEMORY ALLOCATION OPTIMIZATIONS
// ============================================================================

/**
 * Memory pool for pre-allocated objects to avoid heap allocation
 * in hot paths. This is a simple fixed-size pool for demonstration.
 */
template<typename T, size_t PoolSize>
class alignas(64) MemoryPool {
private:
    alignas(64) T pool_[PoolSize];
    alignas(64) bool used_[PoolSize];
    size_t next_free_;
    
public:
    MemoryPool() : next_free_(0) {
        for (size_t i = 0; i < PoolSize; ++i) {
            used_[i] = false;
        }
    }
    
    /**
     * Allocate an object from the pool
     * @return Pointer to allocated object, or nullptr if pool is full
     */
    T* allocate() {
        // Simple linear search for free slot
        for (size_t i = 0; i < PoolSize; ++i) {
            size_t idx = (next_free_ + i) % PoolSize;
            if (!used_[idx]) {
                used_[idx] = true;
                next_free_ = (idx + 1) % PoolSize;
                return &pool_[idx];
            }
        }
        return nullptr; // Pool is full
    }
    
    /**
     * Deallocate an object back to the pool
     * @param ptr Pointer to object to deallocate
     */
    void deallocate(T* ptr) {
        if (ptr >= pool_ && ptr < pool_ + PoolSize) {
            size_t idx = ptr - pool_;
            used_[idx] = false;
        }
    }
    
    /**
     * Get the number of available slots in the pool
     * @return Number of free slots
     */
    size_t available() const {
        size_t count = 0;
        for (size_t i = 0; i < PoolSize; ++i) {
            if (!used_[i]) {
                count++;
            }
        }
        return count;
    }
    
    /**
     * Get the total pool size
     * @return Total number of slots
     */
    static constexpr size_t capacity() {
        return PoolSize;
    }
};

} // namespace hft