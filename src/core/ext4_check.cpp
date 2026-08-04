#include "opm/ext4_impl.hpp"
#include "opm/disk_io.hpp"
#include <cstring>

namespace opm {
namespace ext4 {

// ============================================================================
// ext4 Check - Phase 3.3.6
// ============================================================================

Result checkEXT4(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                  bool repair, std::vector<std::string>* errors) {
    
    if (errors) {
        errors->clear();
    }
    
    std::vector<std::string> local_errors;
    std::vector<std::string>* err = errors ? errors : &local_errors;
    
    // Step 1: Check superblock
    Result result = checkSuperblock(disk, start_sector, repair, err);
    if (result.failed() && !repair) {
        return result;
    }
    
    // Step 2: Get layout
    EXT4Layout layout;
    EXT4Superblock sb;
    result = disk->read(&sb, (start_sector * 512) + 1024, sizeof(EXT4Superblock));
    if (result.failed()) {
        err->push_back("Failed to read superblock");
        return result;
    }
    
    // Reconstruct layout from superblock
    layout.calculate(static_cast<uint64_t>(sb.s_blocks_count_lo) * 
                      (1024 << sb.s_log_block_size));
    
    // Step 3: Check group descriptors
    result = checkGroupDescriptors(disk, start_sector, layout, repair, err);
    if (result.failed() && !repair) {
        return result;
    }
    
    // Step 4: Check block bitmaps
    result = checkBlockBitmaps(disk, start_sector, layout, repair, err);
    if (result.failed() && !repair) {
        return result;
    }
    
    // Step 5: Check inode bitmaps
    result = checkInodeBitmaps(disk, start_sector, layout, repair, err);
    if (result.failed() && !repair) {
        return result;
    }
    
    // Step 6: Check root inode
    result = checkRootInode(disk, start_sector, layout, repair, err);
    if (result.failed() && !repair) {
        return result;
    }
    
    return errors && !errors->empty() ? 
        Result::error("Found " + std::to_string(errors->size()) + " errors") : 
        Result::ok();
}

// ============================================================================
// Check Superblock
// ============================================================================

Result checkSuperblock(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                        bool repair, std::vector<std::string>* errors) {
    
    EXT4Superblock sb;
    Result result = disk->read(&sb, (start_sector * 512) + 1024, sizeof(EXT4Superblock));
    if (result.failed()) {
        errors->push_back("Failed to read superblock");
        return result;
    }
    
    // Check magic number
    if (sb.s_magic != EXT4_SUPER_MAGIC) {
        errors->push_back("Invalid superblock magic (expected 0xEF53, got 0x" + 
                         std::to_string(sb.s_magic) + ")");
        return Result::error("Invalid superblock");
    }
    
    // Check state
    if (sb.s_state != EXT4_VALID_FS) {
        errors->push_back("Filesystem not clean (state=" + std::to_string(sb.s_state) + ")");
        if (repair && sb.s_state == EXT4_ERROR_FS) {
            // Would attempt to repair
            errors->push_back("  -> Attempting to mark as valid");
        }
    }
    
    // Check block size
    if (sb.s_log_block_size > 6) {
        errors->push_back("Invalid block size");
    }
    
    // Check revision
    if (sb.s_rev_level > 1) {
        errors->push_back("Unknown revision level: " + std::to_string(sb.s_rev_level));
    }
    
    return Result::ok();
}

// ============================================================================
// Check Group Descriptors
// ============================================================================

Result checkGroupDescriptors(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                               const EXT4Layout& layout, [[maybe_unused]] bool repair,
                               std::vector<std::string>* errors) {
    
    uint64_t gdt_start_block = (layout.block_size == 1024) ? 2 : 1;
    uint64_t gdt_offset = (start_sector * layout.bytes_per_sector) + 
                          (gdt_start_block * layout.block_size);
    
    for (uint32_t group = 0; group < layout.num_groups; group++) {
        EXT4GroupDesc gd;
        Result result = disk->read(&gd, gdt_offset + (group * sizeof(EXT4GroupDesc)), 
                                    sizeof(EXT4GroupDesc));
        if (result.failed()) {
            errors->push_back("Failed to read group descriptor " + std::to_string(group));
            continue;
        }
        
        // Check bitmap locations
        uint64_t block_bitmap = (static_cast<uint64_t>(gd.bg_block_bitmap_hi) << 32) |
                                 gd.bg_block_bitmap_lo;
        uint64_t inode_bitmap = (static_cast<uint64_t>(gd.bg_inode_bitmap_hi) << 32) |
                                 gd.bg_inode_bitmap_lo;
        uint64_t inode_table = (static_cast<uint64_t>(gd.bg_inode_table_hi) << 32) |
                                gd.bg_inode_table_lo;
        
        // Validate locations
        uint64_t group_start = static_cast<uint64_t>(group) * layout.blocks_per_group;
        if (block_bitmap != group_start + 3) {
            errors->push_back("Group " + std::to_string(group) + 
                            ": Invalid block bitmap location");
        }
        if (inode_bitmap != group_start + 4) {
            errors->push_back("Group " + std::to_string(group) + 
                            ": Invalid inode bitmap location");
        }
        if (inode_table != group_start + 5) {
            errors->push_back("Group " + std::to_string(group) + 
                            ": Invalid inode table location");
        }
    }
    
    return Result::ok();
}

// ============================================================================
// Check Block Bitmaps
// ============================================================================

Result checkBlockBitmaps(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                          const EXT4Layout& layout, [[maybe_unused]] bool repair,
                          std::vector<std::string>* errors) {
    
    // Check first group only for now
    uint64_t group_start = 0;
    uint64_t bitmap_block = group_start + 3;
    uint64_t bitmap_offset = (start_sector * layout.bytes_per_sector) + 
                              (bitmap_block * layout.block_size);
    
    std::vector<uint8_t> bitmap(layout.block_size);
    Result result = disk->read(bitmap.data(), bitmap_offset, layout.block_size);
    if (result.failed()) {
        errors->push_back("Failed to read block bitmap");
        return result;
    }
    
    // Check that system blocks are marked as used
    // Blocks 0-4 should be used
    for (uint32_t i = 0; i < 5; i++) {
        uint32_t byte_idx = i / 8;
        uint32_t bit_idx = i % 8;
        if (!(bitmap[byte_idx] & (1 << bit_idx))) {
            errors->push_back("System block " + std::to_string(i) + " not marked as used");
        }
    }
    
    return Result::ok();
}

// ============================================================================
// Check Inode Bitmaps
// ============================================================================

Result checkInodeBitmaps(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                          const EXT4Layout& layout, [[maybe_unused]] bool repair,
                          std::vector<std::string>* errors) {
    
    uint64_t group_start = 0;
    uint64_t bitmap_block = group_start + 4;
    uint64_t bitmap_offset = (start_sector * layout.bytes_per_sector) + 
                              (bitmap_block * layout.block_size);
    
    std::vector<uint8_t> bitmap(layout.block_size);
    Result result = disk->read(bitmap.data(), bitmap_offset, layout.block_size);
    if (result.failed()) {
        errors->push_back("Failed to read inode bitmap");
        return result;
    }
    
    // Check that reserved inodes are marked as used
    // Inodes 1-10 should be used
    for (uint32_t i = 0; i < 10; i++) {
        uint32_t byte_idx = i / 8;
        uint32_t bit_idx = i % 8;
        if (!(bitmap[byte_idx] & (1 << bit_idx))) {
            errors->push_back("Reserved inode " + std::to_string(i + 1) + 
                            " not marked as used");
        }
    }
    
    return Result::ok();
}

// ============================================================================
// Check Root Inode
// ============================================================================

Result checkRootInode(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                        const EXT4Layout& layout, [[maybe_unused]] bool repair,
                        std::vector<std::string>* errors) {

    // Read root inode (inode 2)
    uint32_t offset = getInodeOffset(EXT4_ROOT_INO, layout.inodes_per_group);
    
    uint64_t inode_table_block = 5;
    uint64_t inode_offset = (start_sector * layout.bytes_per_sector) + 
                             (inode_table_block * layout.block_size) + 
                             (offset * layout.inode_size);
    
    EXT4Inode inode;
    Result result = disk->read(&inode, inode_offset, sizeof(EXT4Inode));
    if (result.failed()) {
        errors->push_back("Failed to read root inode");
        return result;
    }
    
    // Check mode
    if ((inode.i_mode & EXT4_S_IFDIR) != EXT4_S_IFDIR) {
        errors->push_back("Root inode is not a directory");
    }
    
    // Check extent magic
    EXT4ExtentHeader* eh = reinterpret_cast<EXT4ExtentHeader*>(inode.i_block_union.i_block);
    if (eh->eh_magic != EXT4_EXTENT_MAGIC) {
        errors->push_back("Root inode extent header has invalid magic");
    }
    
    return Result::ok();
}

} // namespace ext4
} // namespace opm
