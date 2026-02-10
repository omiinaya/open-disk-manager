#pragma once

#include "types.hpp"
#include <string>
#include <memory>
#include <vector>

namespace opm {

// Forward declarations
class DiskIO;

// File system information
struct FSInfo {
    FileSystemType type;
    std::string label;
    std::string uuid;
    uint64_t total_size;
    uint64_t used_size;
    uint64_t free_size;
    uint32_t block_size;
    uint64_t block_count;
    uint64_t free_blocks;
    uint32_t cluster_size;     // For FAT
    bool dirty;                // Needs check
    bool has_errors;
    std::string mount_point;
    bool mounted;
};

// Base class for file system operations
class FileSystem {
public:
    virtual ~FileSystem() = default;
    
    // Get file system type
    virtual FileSystemType type() const = 0;
    
    // Get file system name
    virtual std::string name() const = 0;
    
    // Create filesystem (format)
    virtual Result create(std::shared_ptr<DiskIO> disk, 
                         uint64_t start_sector,
                         uint64_t size_bytes,
                         const std::string& label = "",
                         uint32_t cluster_size = 0) = 0;
    
    // Check filesystem
    virtual Result check(std::shared_ptr<DiskIO> disk,
                        uint64_t start_sector,
                        bool repair = false,
                        std::vector<std::string>* errors = nullptr) = 0;
    
    // Resize filesystem
    virtual Result resize(std::shared_ptr<DiskIO> disk,
                         uint64_t start_sector,
                         uint64_t new_size_bytes) = 0;
    
    // Get filesystem info
    virtual Result getInfo(std::shared_ptr<DiskIO> disk,
                            uint64_t start_sector,
                            FSInfo& info) = 0;
    
protected:
    std::shared_ptr<DiskIO> disk_;
    uint64_t start_sector_;
};

// Factory function
std::unique_ptr<FileSystem> createFileSystem(FileSystemType type);

// Get filesystem name
std::string getFilesystemName(FileSystemType type);

// Check if filesystem type is supported for format
bool isFilesystemSupported(FileSystemType type);

} // namespace opm
