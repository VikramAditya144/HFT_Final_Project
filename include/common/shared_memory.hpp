#pragma once

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <stdexcept>

namespace hft {

/**
 * RAII wrapper for POSIX shared memory operations
 * Provides safe creation, mapping, and cleanup of shared memory segments
 */
class SharedMemoryManager {
private:
    int shm_fd_;
    void* mapped_addr_;
    size_t size_;
    std::string name_;
    bool is_creator_;

public:
    /**
     * Constructor for shared memory management
     * @param name Shared memory segment name (without leading slash)
     * @param size Size of the shared memory segment in bytes
     * @param create True to create new segment, false to attach to existing
     */
    SharedMemoryManager(const std::string& name, size_t size, bool create = true)
        : shm_fd_(-1), mapped_addr_(MAP_FAILED), size_(size), name_("/" + name), is_creator_(create) {
        
        // Validate inputs
        if (name.empty()) {
            throw std::runtime_error("Shared memory name cannot be empty");
        }
        if (size == 0) {
            throw std::runtime_error("Shared memory size cannot be zero");
        }
        
        if (create) {
            // Create new shared memory segment (or open existing)
            shm_fd_ = shm_open(name_.c_str(), O_CREAT | O_RDWR, 0666);
            if (shm_fd_ == -1) {
                throw std::runtime_error("Failed to create shared memory segment: " + name_);
            }
            
            // Get current size to check if we need to set it
            struct stat shm_stat;
            if (fstat(shm_fd_, &shm_stat) == -1) {
                close(shm_fd_);
                shm_unlink(name_.c_str());
                throw std::runtime_error("Failed to get shared memory stats");
            }
            
            // Only set size if the segment is new (size is 0)
            if (shm_stat.st_size == 0) {
                if (ftruncate(shm_fd_, size_) == -1) {
                    close(shm_fd_);
                    shm_unlink(name_.c_str());
                    throw std::runtime_error("Failed to set shared memory size");
                }
            }
            // If segment already exists with some size, we'll use it as-is
            // This allows multiple creators to open the same segment
        } else {
            // Attach to existing shared memory segment
            shm_fd_ = shm_open(name_.c_str(), O_RDONLY, 0666);
            if (shm_fd_ == -1) {
                throw std::runtime_error("Failed to open existing shared memory segment: " + name_);
            }
        }
        
        // Map the shared memory into process address space
        int prot = create ? (PROT_READ | PROT_WRITE) : PROT_READ;
        mapped_addr_ = mmap(nullptr, size_, prot, MAP_SHARED, shm_fd_, 0);
        
        if (mapped_addr_ == MAP_FAILED) {
            close(shm_fd_);
            if (create) {
                shm_unlink(name_.c_str());
            }
            throw std::runtime_error("Failed to map shared memory");
        }
    }

    /**
     * Destructor - automatically cleans up resources
     */
    ~SharedMemoryManager() {
        cleanup();
    }

    // Disable copy constructor and assignment operator
    SharedMemoryManager(const SharedMemoryManager&) = delete;
    SharedMemoryManager& operator=(const SharedMemoryManager&) = delete;

    // Enable move constructor and assignment operator
    SharedMemoryManager(SharedMemoryManager&& other) noexcept
        : shm_fd_(other.shm_fd_), mapped_addr_(other.mapped_addr_), 
          size_(other.size_), name_(std::move(other.name_)), is_creator_(other.is_creator_) {
        other.shm_fd_ = -1;
        other.mapped_addr_ = MAP_FAILED;
        other.is_creator_ = false;
    }

    SharedMemoryManager& operator=(SharedMemoryManager&& other) noexcept {
        if (this != &other) {
            cleanup();
            
            shm_fd_ = other.shm_fd_;
            mapped_addr_ = other.mapped_addr_;
            size_ = other.size_;
            name_ = std::move(other.name_);
            is_creator_ = other.is_creator_;
            
            other.shm_fd_ = -1;
            other.mapped_addr_ = MAP_FAILED;
            other.is_creator_ = false;
        }
        return *this;
    }

    /**
     * Get the mapped memory address
     * @return Pointer to the mapped shared memory, or MAP_FAILED if invalid
     */
    void* get_address() const {
        return mapped_addr_;
    }

    /**
     * Check if the shared memory is valid and mapped
     * @return True if valid, false otherwise
     */
    bool is_valid() const {
        return mapped_addr_ != MAP_FAILED && shm_fd_ != -1;
    }

    /**
     * Get the size of the shared memory segment
     * @return Size in bytes
     */
    size_t get_size() const {
        return size_;
    }

    /**
     * Get the name of the shared memory segment
     * @return Name string (with leading slash)
     */
    const std::string& get_name() const {
        return name_;
    }

    /**
     * Check if this instance created the shared memory segment
     * @return True if creator, false if attached to existing
     */
    bool is_creator() const {
        return is_creator_;
    }

private:
    /**
     * Internal cleanup method
     */
    void cleanup() {
        if (mapped_addr_ != MAP_FAILED) {
            munmap(mapped_addr_, size_);
            mapped_addr_ = MAP_FAILED;
        }
        
        if (shm_fd_ != -1) {
            close(shm_fd_);
            shm_fd_ = -1;
        }
        
        // Only unlink if we created the segment
        if (is_creator_ && !name_.empty()) {
            shm_unlink(name_.c_str());
            is_creator_ = false;
        }
    }
};

} // namespace hft