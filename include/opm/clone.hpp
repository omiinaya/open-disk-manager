#pragma once

#include "types.hpp"
#include "disk_io.hpp"
#include <memory>
#include <functional>

namespace opm {

// ============================================================================
// Clone Options
// ============================================================================
struct CloneOptions {
    bool verify = true;              // Verify data after clone
    bool compress = false;           // Compress during transfer
    bool sparse = true;              // Handle sparse files efficiently
    bool ignore_errors = false;      // Continue on read errors
    uint64_t buffer_size = 64 * 1024 * 1024; // 64MB buffer
    std::function<void(uint64_t, uint64_t)> progress_callback; // Progress callback (bytes_done, bytes_total)
};

// ============================================================================
// Disk Cloning Operations
// ============================================================================

// Clone entire disk
Result cloneDisk(std::shared_ptr<DiskIO> source, std::shared_ptr<DiskIO> target,
                 const CloneOptions& options = CloneOptions{});

// Clone partition
Result clonePartition(std::shared_ptr<DiskIO> source, uint64_t source_start, uint64_t source_size,
                      std::shared_ptr<DiskIO> target, uint64_t target_start,
                      const CloneOptions& options = CloneOptions{});

// Clone with resizing (fit to target)
Result cloneDiskWithResize(std::shared_ptr<DiskIO> source, std::shared_ptr<DiskIO> target,
                           const CloneOptions& options = CloneOptions{});

// ============================================================================
// Secure Erase Operations
// ============================================================================

enum class EraseMethod {
    Zeros,          // Write zeros
    Random,         // Write random data
    DoD522022,      // DoD 5220.22-M (3-pass)
    DoD522022ECE,   // DoD 5220.22-M ECE (7-pass)
    Gutmann,        // Gutmann 35-pass
    NIST80088,      // NIST 800-88 Clear (zeros)
    NIST80088Purge, // NIST 800-88 Purge (random)
    RCMP_TSSIT,     // RCMP TSSIT OPS-II (4-pass: 0x00, 0xFF, 0xFF, random)
    VSITR,          // German BSI VSITR (7-pass)
    GOST_P50739,    // GOST R 50739-95 (2-pass: zeros, random)
    US_Army_AR380,  // US Army AR380-19 (3-pass: 0xFF, 0x00, random)
    ATA_Erase,      // ATA Secure Erase (via TRIM/BLKDISCARD passthrough; honest fallback)
};

struct EraseOptions {
    EraseMethod method = EraseMethod::Zeros;
    uint64_t buffer_size = 64 * 1024 * 1024; // 64MB buffer
    std::function<void(uint64_t, uint64_t)> progress_callback; // Progress callback
};

// Secure erase disk/partition
Result secureErase(std::shared_ptr<DiskIO> disk, uint64_t start_sector, uint64_t size_sectors,
                   const EraseOptions& options = EraseOptions{});

// Secure erase entire disk
Result secureEraseDisk(std::shared_ptr<DiskIO> disk, const EraseOptions& options = EraseOptions{});

// ============================================================================
// Benchmark Operations
// ============================================================================

struct BenchmarkResult {
    double sequential_read_mbps = 0.0;
    double sequential_write_mbps = 0.0;
    double random_read_iops = 0.0;
    double random_write_iops = 0.0;
    double latency_ms = 0.0;
    uint64_t buffer_size = 0;
};

struct BenchmarkOptions {
    uint64_t test_size = 1024 * 1024 * 1024; // 1GB test size
    uint64_t block_size = 4096;              // 4KB blocks
    uint64_t duration_seconds = 10;          // Test duration
    bool quick_test = false;                 // Quick 100MB test
};

// Benchmark disk
Result benchmarkDisk(std::shared_ptr<DiskIO> disk, BenchmarkResult& result,
                     const BenchmarkOptions& options = BenchmarkOptions{});

// Benchmark partition
Result benchmarkPartition(std::shared_ptr<DiskIO> disk, uint64_t start_sector, uint64_t size_sectors,
                          BenchmarkResult& result, const BenchmarkOptions& options = BenchmarkOptions{});

// ============================================================================
// Partition Copy Operations
// ============================================================================

struct CopyOptions {
    bool resize_to_fit = false;      // Resize filesystem to fit target
    bool verify = true;              // Verify after copy
    uint64_t buffer_size = 64 * 1024 * 1024;
    std::function<void(uint64_t, uint64_t)> progress_callback;
};

// Copy partition (including filesystem)
Result copyPartition(std::shared_ptr<DiskIO> source, uint64_t source_start, uint64_t source_size,
                     std::shared_ptr<DiskIO> target, uint64_t target_start, uint64_t target_size,
                     const CopyOptions& options = CopyOptions{});

// Copy partition table from one disk to another
Result copyPartitionTable(std::shared_ptr<DiskIO> source, std::shared_ptr<DiskIO> target,
                          bool target_is_gpt);

} // namespace opm
