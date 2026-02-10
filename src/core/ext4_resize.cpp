#include "opm/ext4_impl.hpp"
#include "opm/disk_io.hpp"
#include <ctime>
#include <cstring>
#include <vector>

namespace opm {
namespace ext4 {

// ============================================================================
// ext4 Resize - Phase 3.3.7
// ============================================================================

Result resizeEXT4(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                    uint64_t new_size_bytes) {
    
    // Step 1: Read current superblock
    EXT4Superblock sb;
    Result result = disk->read(&sb, (start_sector * 512) + 1024, sizeof(EXT4Superblock));
    if (result.failed()) {
        return Result::error("Failed to read superblock: " + result.message);
    }
    
    // Step 2: Calculate old and new layouts
    EXT4Layout old_layout;
    old_layout.calculate(static_cast<uint64_t>(sb.s_blocks_count_lo) * 
                          (1024 << sb.s_log_block_size));
    
    EXT4Layout new_layout;
    new_layout.calculate(new_size_bytes, 1024 << sb.s_log_block_size);
    
    // Step 3: Validate resize
    if (new_size_bytes <= old_layout.total_size) {
        return Result::error("ext4 shrink not supported in this implementation");
    }
    
    // Step 4: Check if we need to add new block groups
    if (new_layout.num_groups > old_layout.num_groups) {
        result = extendBlockGroups(disk, start_sector, old_layout, new_layout);
        if (result.failed()) {
            return result;
        }
    }
    
    // Step 5: Update superblock with new size
    result = updateSuperblockForResize(disk, start_sector, new_layout);
    if (result.failed()) {
        return result;
    }
    
    // Step 6: Flush changes
    return disk->flush();
}

// ============================================================================
// Extend Block Groups
// ============================================================================

Result extendBlockGroups(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                          const EXT4Layout& old_layout, const EXT4Layout& new_layout) {
    
    uint32_t old_num_groups = old_layout.num_groups;
    uint32_t new_num_groups = new_layout.num_groups;
    
    // Calculate GDT location
    uint64_t gdt_start_block = (old_layout.block_size == 1024) ? 2 : 1;
    uint64_t gdt_offset = (start_sector * old_layout.block_size) + 
                            (gdt_start_block * old_layout.block_size);
    
    // Read existing GDT
    std::vector<EXT4GroupDesc> gdt(new_num_groups);
    Result result = disk->read(gdt.data(), gdt_offset, 
                                old_num_groups * sizeof(EXT4GroupDesc));
    if (result.failed()) {
        return Result::error("Failed to read GDT: " + result.message);
    }
    
    // Initialize new group descriptors
    for (uint32_t group = old_num_groups; group < new_num_groups; group++) {
        gdt[group].init(group, new_layout.block_size);
        
        // Calculate actual positions for this group
        uint64_t group_start = static_cast<uint64_t>(group) * new_layout.blocks_per_group;
        
        // Block bitmap
        uint64_t block_bitmap = group_start + 3;
        gdt[group].bg_block_bitmap_lo = static_cast<uint32_t>(block_bitmap & 0xFFFFFFFF);
        gdt[group].bg_block_bitmap_hi = static_cast<uint32_t>(block_bitmap >> 32);
        
        // Inode bitmap
        uint64_t inode_bitmap = group_start + 4;
        gdt[group].bg_inode_bitmap_lo = static_cast<uint32_t>(inode_bitmap & 0xFFFFFFFF);
        gdt[group].bg_inode_bitmap_hi = static_cast<uint32_t>(inode_bitmap >> 32);
        
        // Inode table
        uint64_t inode_table = group_start + 5;
        gdt[group].bg_inode_table_lo = static_cast<uint32_t>(inode_table & 0xFFFFFFFF);
        gdt[group].bg_inode_table_hi = static_cast<uint32_t>(inode_table >> 32);
        
        // Calculate free blocks
        uint32_t used_blocks = 5 + calculateInodeTableSize(new_layout);
        uint32_t free_blocks = new_layout.blocks_per_group - used_blocks;
        
        // For last group, adjust for actual size
        if (group == new_num_groups - 1) {
            uint64_t total_blocks = new_layout.total_size / new_layout.block_size;
            uint64_t actual_blocks = total_blocks - group_start;
            if (actual_blocks < new_layout.blocks_per_group) {
                free_blocks = static_cast<uint32_t>(actual_blocks) - used_blocks;
            }
        }
        
        gdt[group].bg_free_blocks_count_lo = static_cast<uint16_t>(free_blocks & 0xFFFF);
        gdt[group].bg_free_blocks_count_hi = static_cast<uint16_t>(free_blocks >> 16);
        
        // Free inodes
        gdt[group].bg_free_inodes_count_lo = static_cast<uint16_t>(new_layout.inodes_per_group);
        gdt[group].bg_free_inodes_count_hi = 0;
        
        // Unused inodes
        gdt[group].bg_itable_unused_lo = static_cast<uint16_t>(new_layout.inodes_per_group);
        gdt[group].bg_itable_unused_hi = 0;
        
        // Flags
        gdt[group].bg_flags = 0;
        
        // Create bitmaps and inode table for this group
        result = createGroupBitmaps(disk, start_sector, new_layout, group);
        if (result.failed()) {
            return result;
        }
    }
    
    // Write updated GDT
    result = disk->write(gdt.data(), gdt_offset, 
                         new_num_groups * sizeof(EXT4GroupDesc));
    if (result.failed()) {
        return Result::error("Failed to write updated GDT: " + result.message);
    }
    
    // Update backup GDTs
    std::vector<uint32_t> backup_groups = {1, 3, 5};
    for (uint32_t bg : backup_groups) {
        if (bg < new_num_groups) {
            uint64_t backup_offset = calculateBackupGDTOffset(
                start_sector, new_layout, bg);
            
            result = disk->write(gdt.data(), backup_offset, 
                                 new_num_groups * sizeof(EXT4GroupDesc));
            // Continue even if backup fails
        }
    }
    
    return Result::ok();
}

// ============================================================================
// Create Group Bitmaps
// ============================================================================

Result createGroupBitmaps(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                           const EXT4Layout& layout, uint32_t group) {
    
    uint64_t group_start = static_cast<uint64_t>(group) * layout.blocks_per_group;
    
    // Create block bitmap
    uint64_t bitmap_block = group_start + 3;
    uint64_t bitmap_offset = (start_sector * layout.block_size) + 
                              (bitmap_block * layout.block_size);
    
    std::vector<uint8_t> bitmap(layout.block_size, 0);
    
    // Mark system blocks as used
    uint32_t used_blocks = 5 + calculateInodeTableSize(layout);
    for (uint32_t i = 0; i < used_blocks && i < layout.blocks_per_group; i++) {
        uint32_t byte_idx = i / 8;
        uint32_t bit_idx = i % 8;
        if (byte_idx < layout.block_size) {
            bitmap[byte_idx] |= (1 << bit_idx);
        }
    }
    
    Result result = disk->write(bitmap.data(), bitmap_offset, layout.block_size);
    if (result.failed()) {
        return Result::error("Failed to write block bitmap for group " + 
                           std::to_string(group));
    }
    
    // Create inode bitmap
    uint64_t inode_bitmap_block = group_start + 4;
    uint64_t inode_bitmap_offset = (start_sector * layout.block_size) + 
                                    (inode_bitmap_block * layout.block_size);
    
    std::vector<uint8_t> inode_bitmap(layout.block_size, 0);
    result = disk->write(inode_bitmap.data(), inode_bitmap_offset, layout.block_size);
    if (result.failed()) {
        return Result::error("Failed to write inode bitmap for group " + 
                           std::to_string(group));
    }
    
    // Create inode table
    uint64_t inode_table_block = group_start + 5;
    uint64_t inode_table_offset = (start_sector * layout.block_size) + 
                                   (inode_table_block * layout.block_size);
    
    uint32_t table_size = layout.inodes_per_group * layout.inode_size;
    std::vector<uint8_t> inode_table(table_size, 0);
    result = disk->write(inode_table.data(), inode_table_offset, table_size);
    if (result.failed()) {
        return Result::error("Failed to write inode table for group " + 
                           std::to_string(group));
    }
    
    return Result::ok();
}

// ============================================================================
// Update Superblock for Resize
// ============================================================================

Result updateSuperblockForResize(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                                   const EXT4Layout& layout) {
    
    EXT4Superblock sb;
    Result result = disk->read(&sb, (start_sector * 512) + 1024, sizeof(EXT4Superblock));
    if (result.failed()) {
        return result;
    }
    
    // Update block counts
    uint64_t total_blocks = layout.total_size / layout.block_size;
    sb.s_blocks_count_lo = static_cast<uint32_t>(total_blocks & 0xFFFFFFFF);
    sb.s_blocks_count_hi = static_cast<uint32_t>(total_blocks >> 32);
    
    // Update free blocks
    // Calculate used blocks (rough estimate)
    uint64_t used_blocks = layout.num_groups * (5 + calculateInodeTableSize(layout));
    uint64_t free_blocks = total_blocks - used_blocks;
    sb.s_free_blocks_count_lo = static_cast<uint32_t>(free_blocks & 0xFFFFFFFF);
    sb.s_free_blocks_count_hi = static_cast<uint32_t>(free_blocks >> 32);
    
    // Update inode counts
    sb.s_inodes_count = layout.num_groups * layout.inodes_per_group;
    sb.s_free_inodes_count = sb.s_inodes_count - 10;  // Reserve first 10
    
    // Update groups per flex
    if (layout.flex_bg) {
        sb.s_log_groups_per_flex = 4;  // 16 groups per flex
    }
    
    // Update write time
    sb.s_wtime = static_cast<uint32_t>(std::time(nullptr));
    
    // Write back
    result = disk->write(&sb, (start_sector * 512) + 1024, sizeof(EXT4Superblock));
    if (result.failed()) {
        return Result::error("Failed to update superblock: " + result.message);
    }
    
    // Write backup superblocks
    std::vector<uint32_t> backup_groups = {1, 3, 5};
    for (uint32_t bg : backup_groups) {
        if (bg < layout.num_groups) {
            uint64_t backup_offset = calculateBackupSuperblockOffset(
                start_sector, layout, bg);
            
            result = disk->write(&sb, backup_offset, sizeof(EXT4Superblock));
            // Continue even if backup fails
        }
    }
    
    return Result::ok();
}

} // namespace ext4
} // namespace opm
