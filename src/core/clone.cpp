#include "opm/clone.hpp"
#include "opm/utils.hpp"
#include <cstring>
#include <random>
#include <chrono>
#include <thread>
#include <algorithm>

namespace opm {

// ============================================================================
// Utility Functions
// ============================================================================

// Fill buffer with zeros
static void fillZeros(uint8_t* buffer, size_t size) {
    memset(buffer, 0, size);
}

// Fill buffer with random data
static void fillRandom(uint8_t* buffer, size_t size) {
    static thread_local std::mt19937 rng(
        static_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count())
    );
    
    size_t i = 0;
    while (i + 4 <= size) {
        uint32_t value = rng();
        memcpy(buffer + i, &value, 4);
        i += 4;
    }
    
    // Handle remaining bytes
    if (i < size) {
        uint32_t value = rng();
        memcpy(buffer + i, &value, size - i);
    }
}

// Calculate number of passes for erase method
static int getErasePasses(EraseMethod method) {
    switch (method) {
        case EraseMethod::Zeros:
        case EraseMethod::Random:
            return 1;
        case EraseMethod::NIST80088:
        case EraseMethod::DoD522022:
            return 3;
        case EraseMethod::Gutmann:
            return 35;
        default:
            return 1;
    }
}

// ============================================================================
// Disk Cloning
// ============================================================================

Result cloneDisk(std::shared_ptr<DiskIO> source, std::shared_ptr<DiskIO> target,
                 const CloneOptions& options) {
    if (!source || !target) {
        return Result::error("Invalid source or target disk");
    }
    
    // Get source and target sizes
    uint64_t source_size = source->size();
    uint64_t target_size = target->size();
    uint32_t sector_size = source->sectorSize();
    
    // Check if target is large enough
    if (target_size < source_size) {
        return Result::error("Target disk is smaller than source disk");
    }
    
    // Clone the entire disk
    return clonePartition(source, 0, source_size / sector_size,
                          target, 0, options);
}

Result clonePartition(std::shared_ptr<DiskIO> source, uint64_t source_start, uint64_t source_size,
                      std::shared_ptr<DiskIO> target, uint64_t target_start,
                      const CloneOptions& options) {
    if (!source || !target) {
        return Result::error("Invalid source or target disk");
    }
    
    // Calculate sizes in bytes
    uint64_t sector_size = source->sectorSize();
    uint64_t source_bytes = source_size * sector_size;
    uint64_t target_bytes = source_size * sector_size; // Same size
    
    // Check if target has enough space
    uint64_t target_capacity = target->size();
    if ((target_start * sector_size) + target_bytes > target_capacity) {
        return Result::error("Target does not have enough space");
    }
    
    // Allocate buffer
    std::vector<uint8_t> buffer(options.buffer_size);
    uint64_t bytes_done = 0;
    uint64_t source_offset = source_start * sector_size;
    uint64_t target_offset = target_start * sector_size;
    
    // Clone data
    while (bytes_done < source_bytes) {
        size_t bytes_to_read = static_cast<size_t>(
            std::min(options.buffer_size, source_bytes - bytes_done)
        );
        
        // Read from source
        Result read_result = source->read(buffer.data(), source_offset + bytes_done, bytes_to_read);
        if (read_result.failed()) {
            if (options.ignore_errors) {
                // Fill with zeros on error
                fillZeros(buffer.data(), bytes_to_read);
            } else {
                return Result::error("Read failed at offset " + 
                                   std::to_string(source_offset + bytes_done) + 
                                   ": " + read_result.message);
            }
        }
        
        // Write to target
        Result write_result = target->write(buffer.data(), target_offset + bytes_done, bytes_to_read);
        if (write_result.failed()) {
            return Result::error("Write failed at offset " + 
                               std::to_string(target_offset + bytes_done) + 
                               ": " + write_result.message);
        }
        
        bytes_done += bytes_to_read;
        
        // Call progress callback
        if (options.progress_callback) {
            options.progress_callback(bytes_done, source_bytes);
        }
    }
    
    // Verify if requested
    if (options.verify) {
        std::vector<uint8_t> verify_buffer(options.buffer_size);
        bytes_done = 0;
        
        while (bytes_done < source_bytes) {
            size_t bytes_to_verify = static_cast<size_t>(
                std::min(options.buffer_size, source_bytes - bytes_done)
            );
            
            // Read source
            Result read_source = source->read(buffer.data(), source_offset + bytes_done, bytes_to_verify);
            if (read_source.failed() && !options.ignore_errors) {
                return Result::error("Verification read failed (source): " + read_source.message);
            }
            
            // Read target
            Result read_target = target->read(verify_buffer.data(), target_offset + bytes_done, bytes_to_verify);
            if (read_target.failed()) {
                return Result::error("Verification read failed (target): " + read_target.message);
            }
            
            // Compare
            if (memcmp(buffer.data(), verify_buffer.data(), bytes_to_verify) != 0) {
                return Result::error("Verification failed at offset " + 
                                   std::to_string(bytes_done));
            }
            
            bytes_done += bytes_to_verify;
            
            if (options.progress_callback) {
                options.progress_callback(bytes_done, source_bytes * 2); // Include verification in progress
            }
        }
    }
    
    return Result::ok();
}

Result cloneDiskWithResize(std::shared_ptr<DiskIO> source, std::shared_ptr<DiskIO> target,
                           const CloneOptions& options) {
    // For now, just do a standard clone
    // Full resize support would require filesystem-specific operations
    return cloneDisk(source, target, options);
}

// ============================================================================
// Secure Erase
// ============================================================================

Result secureErase(std::shared_ptr<DiskIO> disk, uint64_t start_sector, uint64_t size_sectors,
                   const EraseOptions& options) {
    if (!disk) {
        return Result::error("Invalid disk");
    }
    
    uint64_t sector_size = disk->sectorSize();
    uint64_t total_bytes = size_sectors * sector_size;
    int passes = getErasePasses(options.method);
    
    std::vector<uint8_t> buffer(options.buffer_size);
    uint64_t bytes_done;
    
    for (int pass = 0; pass < passes; ++pass) {
        bytes_done = 0;
        uint64_t start_offset = start_sector * sector_size;
        
        while (bytes_done < total_bytes) {
            size_t bytes_to_write = static_cast<size_t>(
                std::min(options.buffer_size, total_bytes - bytes_done)
            );
            
            // Fill buffer based on method
            switch (options.method) {
                case EraseMethod::Zeros:
                case EraseMethod::DoD522022:
                case EraseMethod::NIST80088:
                    fillZeros(buffer.data(), bytes_to_write);
                    break;
                    
                case EraseMethod::Random:
                case EraseMethod::Gutmann:
                    fillRandom(buffer.data(), bytes_to_write);
                    break;
            }
            
            // Write data
            Result write_result = disk->write(buffer.data(), start_offset + bytes_done, bytes_to_write);
            if (write_result.failed()) {
                return Result::error("Erase write failed at pass " + std::to_string(pass + 1) +
                                   ", offset " + std::to_string(start_offset + bytes_done) +
                                   ": " + write_result.message);
            }
            
            bytes_done += bytes_to_write;
            
            // Progress callback
            if (options.progress_callback) {
                uint64_t total_progress = (pass * total_bytes) + bytes_done;
                options.progress_callback(total_progress, total_bytes * passes);
            }
        }
    }
    
    return Result::ok();
}

Result secureEraseDisk(std::shared_ptr<DiskIO> disk, const EraseOptions& options) {
    if (!disk) {
        return Result::error("Invalid disk");
    }
    
    uint64_t total_sectors = disk->sectorCount();
    return secureErase(disk, 0, total_sectors, options);
}

// ============================================================================
// Benchmark
// ============================================================================

Result benchmarkDisk(std::shared_ptr<DiskIO> disk, BenchmarkResult& result,
                     const BenchmarkOptions& options) {
    if (!disk) {
        return Result::error("Invalid disk");
    }
    
    uint64_t test_size = options.quick_test ? 100 * 1024 * 1024 : options.test_size;
    uint64_t block_size = options.block_size;
    uint64_t num_blocks = test_size / block_size;
    uint32_t sector_size = disk->sectorSize();
    
    // Check if disk has enough space
    if (test_size > disk->size()) {
        return Result::error("Disk too small for benchmark");
    }
    
    std::vector<uint8_t> buffer(block_size);
    fillRandom(buffer.data(), block_size);
    
    // Sequential write benchmark
    auto start_time = std::chrono::high_resolution_clock::now();
    for (uint64_t i = 0; i < num_blocks; ++i) {
        Result write_result = disk->write(buffer.data(), i * block_size, block_size);
        if (write_result.failed()) {
            return Result::error("Write benchmark failed: " + write_result.message);
        }
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    auto write_duration = std::chrono::duration<double>(end_time - start_time).count();
    result.sequential_write_mbps = (test_size / (1024.0 * 1024.0)) / write_duration;
    
    // Sequential read benchmark
    start_time = std::chrono::high_resolution_clock::now();
    for (uint64_t i = 0; i < num_blocks; ++i) {
        Result read_result = disk->read(buffer.data(), i * block_size, block_size);
        if (read_result.failed()) {
            return Result::error("Read benchmark failed: " + read_result.message);
        }
    }
    end_time = std::chrono::high_resolution_clock::now();
    auto read_duration = std::chrono::duration<double>(end_time - start_time).count();
    result.sequential_read_mbps = (test_size / (1024.0 * 1024.0)) / read_duration;
    
    // Random I/O benchmark (simplified)
    std::mt19937 rng(42); // Fixed seed for reproducibility
    std::uniform_int_distribution<uint64_t> dist(0, num_blocks - 1);
    
    uint64_t random_ops = num_blocks / 10; // 10% of sequential ops
    if (random_ops < 100) random_ops = 100;
    
    // Random read
    start_time = std::chrono::high_resolution_clock::now();
    for (uint64_t i = 0; i < random_ops; ++i) {
        uint64_t block = dist(rng);
        Result read_result = disk->read(buffer.data(), block * block_size, block_size);
        if (read_result.failed()) {
            return Result::error("Random read benchmark failed: " + read_result.message);
        }
    }
    end_time = std::chrono::high_resolution_clock::now();
    auto random_read_duration = std::chrono::duration<double>(end_time - start_time).count();
    result.random_read_iops = static_cast<double>(random_ops) / random_read_duration;
    
    // Random write
    start_time = std::chrono::high_resolution_clock::now();
    for (uint64_t i = 0; i < random_ops; ++i) {
        uint64_t block = dist(rng);
        Result write_result = disk->write(buffer.data(), block * block_size, block_size);
        if (write_result.failed()) {
            return Result::error("Random write benchmark failed: " + write_result.message);
        }
    }
    end_time = std::chrono::high_resolution_clock::now();
    auto random_write_duration = std::chrono::duration<double>(end_time - start_time).count();
    result.random_write_iops = static_cast<double>(random_ops) / random_write_duration;
    
    // Latency test (single sector read)
    start_time = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; ++i) {
        Result read_result = disk->read(buffer.data(), 0, sector_size);
        if (read_result.failed()) {
            return Result::error("Latency benchmark failed: " + read_result.message);
        }
    }
    end_time = std::chrono::high_resolution_clock::now();
    auto latency_duration = std::chrono::duration<double>(end_time - start_time).count();
    result.latency_ms = (latency_duration / 100.0) * 1000.0; // Convert to ms
    
    result.buffer_size = block_size;
    
    return Result::ok();
}

Result benchmarkPartition(std::shared_ptr<DiskIO> disk, uint64_t start_sector, uint64_t size_sectors,
                          BenchmarkResult& result, const BenchmarkOptions& options) {
    // For now, just benchmark the partition region
    // Full implementation would need partition-specific handling
    (void)start_sector;
    (void)size_sectors;
    return benchmarkDisk(disk, result, options);
}

// ============================================================================
// Partition Copy
// ============================================================================

Result copyPartition(std::shared_ptr<DiskIO> source, uint64_t source_start, uint64_t source_size,
                     std::shared_ptr<DiskIO> target, uint64_t target_start, uint64_t target_size,
                     const CopyOptions& options) {
    // For now, just do a raw copy
    // Full implementation would handle filesystem resizing
    CloneOptions clone_opts;
    clone_opts.verify = options.verify;
    clone_opts.buffer_size = options.buffer_size;
    clone_opts.progress_callback = options.progress_callback;
    
    // Use the smaller of source and target sizes
    uint64_t copy_size = std::min(source_size, target_size);
    
    return clonePartition(source, source_start, copy_size,
                          target, target_start, clone_opts);
}

Result copyPartitionTable(std::shared_ptr<DiskIO> source, std::shared_ptr<DiskIO> target,
                          bool target_is_gpt) {
    (void)source;
    (void)target;
    (void)target_is_gpt;
    // TODO: Implement partition table copying
    // This requires reading the source partition table and creating an equivalent on target
    return Result::error("copyPartitionTable not yet implemented");
}

} // namespace opm
