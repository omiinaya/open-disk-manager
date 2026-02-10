#include "opm/ext4_impl.hpp"
#include "opm/disk_io.hpp"
#include <cstring>

namespace opm {
namespace ext4 {

// ============================================================================
// ext4 Complete Format - Phase 3.3.5
// ============================================================================

Result formatEXT4(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                   uint64_t size_bytes, const std::string& label) {
    
    // Step 1: Calculate layout
    EXT4Layout layout;
    layout.calculate(size_bytes);
    
    if (!layout.validate()) {
        return Result::error("Invalid ext4 layout");
    }
    
    // Step 2: Create superblock
    Result result = createSuperblock(disk, start_sector, layout, label);
    if (result.failed()) {
        return Result::error("Failed to create superblock: " + result.message);
    }
    
    // Step 3: Create group descriptors
    result = createGroupDescriptors(disk, start_sector, layout);
    if (result.failed()) {
        return Result::error("Failed to create group descriptors: " + result.message);
    }
    
    // Step 4: Create block bitmaps
    result = createBlockBitmaps(disk, start_sector, layout);
    if (result.failed()) {
        return Result::error("Failed to create block bitmaps: " + result.message);
    }
    
    // Step 5: Create inode bitmaps
    result = createInodeBitmaps(disk, start_sector, layout);
    if (result.failed()) {
        return Result::error("Failed to create inode bitmaps: " + result.message);
    }
    
    // Step 6: Create inode table
    result = createInodeTable(disk, start_sector, layout);
    if (result.failed()) {
        return Result::error("Failed to create inode table: " + result.message);
    }
    
    // Step 7: Create root directory
    result = createRootDirectory(disk, start_sector, layout, label);
    if (result.failed()) {
        return Result::error("Failed to create root directory: " + result.message);
    }
    
    // Step 8: Flush all changes
    result = disk->flush();
    if (result.failed()) {
        return Result::error("Failed to flush changes: " + result.message);
    }
    
    return Result::ok();
}

} // namespace ext4
} // namespace opm
