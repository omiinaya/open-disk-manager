#include "opm/ext4_impl.hpp"
#include "opm/disk_io.hpp"
#include <cstring>
#include <ctime>

namespace opm {
namespace ext4 {

// ============================================================================
// ext4 Root Directory - Phase 3.3.3
// ============================================================================

Result createRootDirectory(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                            const EXT4Layout& layout, [[maybe_unused]] const std::string& label) {
    
    // Group 0, inode 2 is root
    uint32_t root_inode_num = EXT4_ROOT_INO;
    
    // Calculate inode location
    uint32_t group = getInodeGroup(root_inode_num, layout.inodes_per_group);
    uint32_t offset = getInodeOffset(root_inode_num, layout.inodes_per_group);
    
    if (group != 0) {
        return Result::error("Root inode should be in group 0");
    }
    
    // Read inode table for group 0
    uint64_t inode_table_block = 5;  // Inode table starts at block 5 in group 0
    uint64_t inode_offset = (start_sector * layout.bytes_per_sector) + 
                             (inode_table_block * layout.block_size) + 
                             (offset * layout.inode_size);
    
    // Create root inode
    EXT4Inode root_inode;
    root_inode.init_directory(0755);
    
    // Set timestamps
    time_t now = std::time(nullptr);
    root_inode.i_atime = static_cast<uint32_t>(now);
    root_inode.i_ctime = static_cast<uint32_t>(now);
    root_inode.i_mtime = static_cast<uint32_t>(now);
    
    // Create extent for root directory data
    // Allocate block 2 for root directory data (first data block)
    uint32_t root_data_block = 2;  // Block 2 (blocks 0,1 are reserved/superblock)
    
    // Initialize extent tree
    EXT4ExtentHeader* eh = reinterpret_cast<EXT4ExtentHeader*>(root_inode.i_block_union.i_block);
    eh->eh_magic = EXT4_EXTENT_MAGIC;
    eh->eh_entries = 1;
    eh->eh_max = 4;  // Can hold 4 extents in inode
    eh->eh_depth = 0;  // Leaf node
    eh->eh_generation = 0;
    
    // Add extent
    EXT4Extent* extent = reinterpret_cast<EXT4Extent*>(
        reinterpret_cast<uint8_t*>(root_inode.i_block_union.i_block) + sizeof(EXT4ExtentHeader));
    extent->ee_block = 0;  // Logical block 0
    extent->ee_len = 1;    // 1 block
    extent->ee_start_hi = 0;
    extent->ee_start_lo = root_data_block;
    
    // Update inode size and blocks
    root_inode.i_size_lo = layout.block_size;  // 1 block
    root_inode.i_blocks_lo = 2;  // 2 * 512-byte blocks (ext4 uses 512-byte units)
    
    // Write root inode
    Result result = disk->write(&root_inode, inode_offset, sizeof(EXT4Inode));
    if (result.failed()) {
        return Result::error("Failed to write root inode: " + result.message);
    }
    
    // Create root directory data
    uint64_t root_data_offset = (start_sector * layout.bytes_per_sector) + 
                                  (root_data_block * layout.block_size);
    
    std::vector<uint8_t> dir_block(layout.block_size, 0);
    EXT4DirEntry* entries = reinterpret_cast<EXT4DirEntry*>(dir_block.data());
    
    // Entry 0: . (current directory)
    entries[0].inode = EXT4_ROOT_INO;
    entries[0].rec_len = 12;
    entries[0].name_len = 1;
    entries[0].file_type = EXT4_FT_DIR;
    entries[0].name[0] = '.';
    
    // Entry 1: .. (parent directory, same as . for root)
    entries[1].inode = EXT4_ROOT_INO;
    entries[1].rec_len = layout.block_size - 12;  // Rest of block
    entries[1].name_len = 2;
    entries[1].file_type = EXT4_FT_DIR;
    entries[1].name[0] = '.';
    entries[1].name[1] = '.';
    
    // Write directory block
    result = disk->write(dir_block.data(), root_data_offset, layout.block_size);
    if (result.failed()) {
        return Result::error("Failed to write root directory data: " + result.message);
    }
    
    // Mark block 2 as used in block bitmap
    result = markBlockUsed(disk, start_sector, layout, 0, root_data_block);
    if (result.failed()) {
        return result;
    }
    
    // Mark inode 2 as used in inode bitmap
    result = markInodeUsed(disk, start_sector, layout, 0, EXT4_ROOT_INO);
    if (result.failed()) {
        return result;
    }
    
    // Update group descriptor
    result = updateGroupDescriptorUsed(disk, start_sector, layout, 0, 
                                        1, 1, 1);  // 1 block, 1 dir, 1 inode
    if (result.failed()) {
        return result;
    }
    
    return Result::ok();
}

// ============================================================================
// Mark Block Used
// ============================================================================

Result markBlockUsed(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                      const EXT4Layout& layout, uint32_t group, uint32_t block) {
    
    uint64_t group_start = static_cast<uint64_t>(group) * layout.blocks_per_group;
    uint64_t bitmap_block = group_start + 3;  // Block bitmap at block 3 in group
    uint64_t bitmap_offset = (start_sector * layout.bytes_per_sector) + 
                              (bitmap_block * layout.block_size);
    
    uint32_t bit_idx = block % layout.blocks_per_group;
    uint32_t byte_idx = bit_idx / 8;
    uint32_t bit_pos = bit_idx % 8;
    
    uint8_t byte;
    Result result = disk->read(&byte, bitmap_offset + byte_idx, 1);
    if (result.failed()) {
        return result;
    }
    
    byte |= (1 << bit_pos);
    
    result = disk->write(&byte, bitmap_offset + byte_idx, 1);
    return result;
}

// ============================================================================
// Mark Inode Used
// ============================================================================

Result markInodeUsed(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                      const EXT4Layout& layout, uint32_t group, uint32_t inode) {
    
    uint64_t group_start = static_cast<uint64_t>(group) * layout.blocks_per_group;
    uint64_t bitmap_block = group_start + 4;  // Inode bitmap at block 4 in group
    uint64_t bitmap_offset = (start_sector * layout.bytes_per_sector) + 
                              (bitmap_block * layout.block_size);
    
    uint32_t bit_idx = (inode - 1) % layout.inodes_per_group;  // Inodes are 1-based
    uint32_t byte_idx = bit_idx / 8;
    uint32_t bit_pos = bit_idx % 8;
    
    uint8_t byte;
    Result result = disk->read(&byte, bitmap_offset + byte_idx, 1);
    if (result.failed()) {
        return result;
    }
    
    byte |= (1 << bit_pos);
    
    result = disk->write(&byte, bitmap_offset + byte_idx, 1);
    return result;
}

// ============================================================================
// Update Group Descriptor
// ============================================================================

Result updateGroupDescriptorUsed(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                                   const EXT4Layout& layout, uint32_t group,
                                   uint32_t blocks_used, uint32_t dirs_used, 
                                   uint32_t inodes_used) {
    
    // Read group descriptor
    uint64_t gdt_start_block = (layout.block_size == 1024) ? 2 : 1;
    uint64_t gd_offset = (start_sector * layout.bytes_per_sector) + 
                           (gdt_start_block * layout.block_size) + 
                           (group * sizeof(EXT4GroupDesc));
    
    EXT4GroupDesc gd;
    Result result = disk->read(&gd, gd_offset, sizeof(EXT4GroupDesc));
    if (result.failed()) {
        return result;
    }
    
    // Update counts
    uint32_t free_blocks = (gd.bg_free_blocks_count_hi << 16) | gd.bg_free_blocks_count_lo;
    if (free_blocks >= blocks_used) {
        free_blocks -= blocks_used;
    }
    gd.bg_free_blocks_count_lo = free_blocks & 0xFFFF;
    gd.bg_free_blocks_count_hi = (free_blocks >> 16) & 0xFFFF;
    
    uint32_t free_inodes = (gd.bg_free_inodes_count_hi << 16) | gd.bg_free_inodes_count_lo;
    if (free_inodes >= inodes_used) {
        free_inodes -= inodes_used;
    }
    gd.bg_free_inodes_count_lo = free_inodes & 0xFFFF;
    gd.bg_free_inodes_count_hi = (free_inodes >> 16) & 0xFFFF;
    
    uint32_t used_dirs = (gd.bg_used_dirs_count_hi << 16) | gd.bg_used_dirs_count_lo;
    used_dirs += dirs_used;
    gd.bg_used_dirs_count_lo = used_dirs & 0xFFFF;
    gd.bg_used_dirs_count_hi = (used_dirs >> 16) & 0xFFFF;
    
    // Write back
    result = disk->write(&gd, gd_offset, sizeof(EXT4GroupDesc));
    return result;
}

} // namespace ext4
} // namespace opm
