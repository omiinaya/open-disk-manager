#include "opm/ext4_impl.hpp"
#include "opm/disk_io.hpp"
#include <cstring>
#include <vector>

namespace opm {
namespace ext4 {

// ============================================================================
// ext4 Boot/Superblock Creation - Phase 3.3.2
// ============================================================================

Result createSuperblock(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                         const EXT4Layout& layout, const std::string& label) {
    
    // Create superblock
    EXT4Superblock sb;
    sb.init(layout.total_size, layout.block_size);
    
    // Set volume label if provided
    if (!label.empty()) {
        std::memset(sb.s_volume_name, 0, sizeof(sb.s_volume_name));
        size_t len = label.length();
        if (len > 15) len = 15;
        std::memcpy(sb.s_volume_name, label.c_str(), len);
    }
    
    // Calculate superblock offset (1024 bytes from partition start)
    uint64_t sb_offset = (start_sector * layout.block_size) + 1024;
    
    // Write superblock
    Result result = disk->write(&sb, sb_offset, sizeof(EXT4Superblock));
    if (result.failed()) {
        return Result::error("Failed to write superblock: " + result.message);
    }
    
    // Write backup superblocks (groups 1, 3, 5, 7, 9, 25, 27...)
    // For simplicity, write to groups 1, 3, and 5
    std::vector<uint32_t> backup_groups = {1, 3, 5};
    for (uint32_t bg : backup_groups) {
        if (bg < layout.num_groups) {
            uint64_t backup_offset = calculateBackupSuperblockOffset(
                start_sector, layout, bg);
            
            result = disk->write(&sb, backup_offset, sizeof(EXT4Superblock));
            if (result.failed()) {
                // Log warning but continue
            }
        }
    }
    
    return Result::ok();
}

// ============================================================================
// Create Group Descriptors
// ============================================================================

Result createGroupDescriptors(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                                const EXT4Layout& layout) {
    
    // Group descriptors start at block 1 (after superblock)
    // For block size 1024, superblock is at offset 1024, GDT starts at block 2
    // For larger blocks, GDT starts at block 1
    
    uint64_t gdt_start_block = (layout.block_size == 1024) ? 2 : 1;
    uint64_t gdt_offset = (start_sector * layout.block_size) + 
                          (gdt_start_block * layout.block_size);
    
    // Create group descriptors
    std::vector<EXT4GroupDesc> gdt(layout.num_groups);
    
    for (uint32_t i = 0; i < layout.num_groups; i++) {
        gdt[i].init(i, layout.block_size);
        
        // Adjust for actual layout
        uint64_t group_start = static_cast<uint64_t>(i) * layout.blocks_per_group;
        
        // Block bitmap
        uint64_t block_bitmap = group_start + 3;
        gdt[i].bg_block_bitmap_lo = static_cast<uint32_t>(block_bitmap & 0xFFFFFFFF);
        gdt[i].bg_block_bitmap_hi = static_cast<uint32_t>(block_bitmap >> 32);
        
        // Inode bitmap
        uint64_t inode_bitmap = group_start + 4;
        gdt[i].bg_inode_bitmap_lo = static_cast<uint32_t>(inode_bitmap & 0xFFFFFFFF);
        gdt[i].bg_inode_bitmap_hi = static_cast<uint32_t>(inode_bitmap >> 32);
        
        // Inode table
        uint64_t inode_table = group_start + 5;
        gdt[i].bg_inode_table_lo = static_cast<uint32_t>(inode_table & 0xFFFFFFFF);
        gdt[i].bg_inode_table_hi = static_cast<uint32_t>(inode_table >> 32);
        
        // Calculate free blocks
        uint32_t used_blocks = 5 + calculateInodeTableSize(layout);
        uint32_t free_blocks = (i == layout.num_groups - 1) ?
            (layout.total_size / layout.block_size - group_start - used_blocks) :
            (layout.blocks_per_group - used_blocks);
        
        gdt[i].bg_free_blocks_count_lo = static_cast<uint16_t>(free_blocks & 0xFFFF);
        gdt[i].bg_free_blocks_count_hi = static_cast<uint16_t>(free_blocks >> 16);
        
        // Free inodes
        gdt[i].bg_free_inodes_count_lo = static_cast<uint16_t>(layout.inodes_per_group);
        gdt[i].bg_free_inodes_count_hi = 0;
        
        // Unused inodes
        gdt[i].bg_itable_unused_lo = static_cast<uint16_t>(layout.inodes_per_group);
        gdt[i].bg_itable_unused_hi = 0;
        
        // Flags
        gdt[i].bg_flags = 0;
    }
    
    // Write GDT
    size_t gdt_size = layout.num_groups * sizeof(EXT4GroupDesc);
    Result result = disk->write(gdt.data(), gdt_offset, gdt_size);
    if (result.failed()) {
        return Result::error("Failed to write group descriptors: " + result.message);
    }
    
    // Write backup GDTs
    std::vector<uint32_t> backup_groups = {1, 3, 5};
    for (uint32_t bg : backup_groups) {
        if (bg < layout.num_groups) {
            uint64_t backup_offset = calculateBackupGDTOffset(
                start_sector, layout, bg);
            
            result = disk->write(gdt.data(), backup_offset, gdt_size);
            // Continue even if backup fails
        }
    }
    
    return Result::ok();
}

// ============================================================================
// Create Block Bitmap
// ============================================================================

Result createBlockBitmaps(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                           const EXT4Layout& layout) {
    
    for (uint32_t group = 0; group < layout.num_groups; group++) {
        uint64_t group_start = static_cast<uint64_t>(group) * layout.blocks_per_group;
        uint64_t bitmap_block = group_start + 3;  // Block 3 in each group
        uint64_t bitmap_offset = (start_sector * layout.block_size) + 
                                  (bitmap_block * layout.block_size);
        
        // Create bitmap
        std::vector<uint8_t> bitmap(layout.block_size, 0);
        
        // Mark system blocks as used
        // Blocks 0-4 are always used (superblock, GDT, block bitmap, inode bitmap, inode table)
        uint32_t used_blocks = 5 + calculateInodeTableSize(layout);
        
        for (uint32_t i = 0; i < used_blocks && i < layout.blocks_per_group; i++) {
            uint32_t byte_idx = i / 8;
            uint32_t bit_idx = i % 8;
            if (byte_idx < layout.block_size) {
                bitmap[byte_idx] |= (1 << bit_idx);
            }
        }
        
        // Write bitmap
        Result result = disk->write(bitmap.data(), bitmap_offset, layout.block_size);
        if (result.failed()) {
            return Result::error("Failed to write block bitmap for group " + 
                               std::to_string(group));
        }
    }
    
    return Result::ok();
}

// ============================================================================
// Create Inode Bitmap
// ============================================================================

Result createInodeBitmaps(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                           const EXT4Layout& layout) {
    
    for (uint32_t group = 0; group < layout.num_groups; group++) {
        uint64_t group_start = static_cast<uint64_t>(group) * layout.blocks_per_group;
        uint64_t bitmap_block = group_start + 4;  // Block 4 in each group
        uint64_t bitmap_offset = (start_sector * layout.block_size) + 
                                  (bitmap_block * layout.block_size);
        
        // Create bitmap
        std::vector<uint8_t> bitmap(layout.block_size, 0);
        
        // Mark used inodes
        // Inode 1-10 are reserved
        for (uint32_t i = 0; i < 10 && i < layout.inodes_per_group; i++) {
            uint32_t byte_idx = i / 8;
            uint32_t bit_idx = i % 8;
            if (byte_idx < layout.block_size) {
                bitmap[byte_idx] |= (1 << bit_idx);
            }
        }
        
        // Mark root inode (inode 2) if this is group 0
        if (group == 0) {
            // Root inode is already marked above (inode 2)
        }
        
        // Write bitmap
        Result result = disk->write(bitmap.data(), bitmap_offset, layout.block_size);
        if (result.failed()) {
            return Result::error("Failed to write inode bitmap for group " + 
                               std::to_string(group));
        }
    }
    
    return Result::ok();
}

// ============================================================================
// Create Inode Table
// ============================================================================

Result createInodeTable(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                         const EXT4Layout& layout) {
    
    for (uint32_t group = 0; group < layout.num_groups; group++) {
        uint64_t group_start = static_cast<uint64_t>(group) * layout.blocks_per_group;
        uint64_t table_start_block = group_start + 5;  // Block 5 in each group
        uint64_t table_offset = (start_sector * layout.block_size) + 
                                (table_start_block * layout.block_size);
        
        // Calculate table size
        uint32_t table_size = layout.inodes_per_group * layout.inode_size;
        
        // Create inode table
        std::vector<uint8_t> table(table_size, 0);
        
        // Initialize special inodes for group 0
        if (group == 0) {
            EXT4Inode* inodes = reinterpret_cast<EXT4Inode*>(table.data());
            
            // Initialize reserved inodes (1-10)
            for (uint32_t i = 0; i < 10 && i < layout.inodes_per_group; i++) {
                inodes[i].init_file(0);
            }
            
            // Mark inode 1 as bad blocks inode
            inodes[0].i_mode = 0;
            
            // Inode 2 is root directory (initialized separately)
            inodes[1].init_directory(0755);
            inodes[1].i_uid = 0;
            inodes[1].i_gid = 0;
            
            // Mark other reserved inodes
            // 3: User quota, 4: Group quota, 5: Boot loader, 6: Undelete
            // 7: Resize inode, 8: Journal, 9-10: Reserved
        }
        
        // Write inode table
        Result result = disk->write(table.data(), table_offset, table_size);
        if (result.failed()) {
            return Result::error("Failed to write inode table for group " + 
                               std::to_string(group));
        }
    }
    
    return Result::ok();
}

// ============================================================================
// Helper Functions
// ============================================================================

uint64_t calculateBackupSuperblockOffset(uint64_t start_sector,
                                            const EXT4Layout& layout,
                                            uint32_t group) {
    uint64_t group_start = static_cast<uint64_t>(group) * layout.blocks_per_group;
    return (start_sector * layout.block_size) + 
           (group_start * layout.block_size) + 1024;
}

uint64_t calculateBackupGDTOffset(uint64_t start_sector,
                                   const EXT4Layout& layout,
                                   uint32_t group) {
    uint64_t group_start = static_cast<uint64_t>(group) * layout.blocks_per_group;
    uint64_t gdt_start_block = (layout.block_size == 1024) ? 2 : 1;
    return (start_sector * layout.block_size) + 
           ((group_start + gdt_start_block) * layout.block_size);
}

uint32_t calculateInodeTableSize(const EXT4Layout& layout) {
    uint32_t table_size_bytes = layout.inodes_per_group * layout.inode_size;
    return (table_size_bytes + layout.block_size - 1) / layout.block_size;
}

} // namespace ext4
} // namespace opm
