#include "opm/ext4_impl.hpp"
#include "opm/disk_io.hpp"
#include <cstring>
#include <ctime>
#include <vector>

namespace opm {
namespace ext4 {

// ============================================================================
// ext4 Journal - Phase 3.3.4
// ============================================================================

// Journal superblock structure (in journal, not ext4 superblock)
struct JournalSuperblock {
    uint32_t h_magic;           // 0xC03B3998
    uint32_t h_block_type;      // 4 (superblock)
    uint32_t h_sequence;        // Sequence number
    uint32_t h_blocksize;       // Block size
    uint32_t h_total_blocks;    // Total blocks in journal
    uint32_t h_start;           // First transaction start block
    uint32_t h_sequence2;       // Sequence number
    uint32_t h_err_no;          // Error number
    uint32_t h_features;        // Compatible features
    uint32_t h_feature_incompat; // Incompatible features
    uint32_t h_feature_ro_compat; // Read-only compatible features
    uint8_t h_uuid[16];         // Journal UUID
    uint32_t h_nr_users;        // Number of users
    uint32_t h_dynsuper;        // Dynamic superblock block
    uint32_t h_max_transaction; // Max transaction blocks
    uint32_t h_max_trans_data;  // Max data blocks per transaction
    uint8_t h_reserved[44];     // Padding
    uint32_t h_checksum;        // Superblock checksum
};

// Journal block types
constexpr uint32_t JFS_DESCRIPTOR_BLOCK = 1;
constexpr uint32_t JFS_COMMIT_BLOCK = 2;
constexpr uint32_t JFS_SUPERBLOCK = 4;
constexpr uint32_t JFS_REVOKE_BLOCK = 8;

// Journal magic
constexpr uint32_t JFS_MAGIC_NUMBER = 0xC03B3998;

Result createJournal(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                      const EXT4Layout& layout) {
    
    // Calculate journal inode
    uint32_t journal_inode = EXT4_JOURNAL_INO;  // Inode 8
    
    // Calculate journal blocks
    uint64_t journal_blocks = layout.journal_size / layout.block_size;
    if (journal_blocks < 1024) {
        journal_blocks = 1024;  // Minimum 1024 blocks (4MB)
    }
    
    // Allocate space for journal (typically at end of filesystem or in inode)
    // For simplicity, we'll place it after the data blocks
    uint64_t journal_start_block = layout.total_size / layout.block_size - journal_blocks;
    
    // Initialize journal superblock
    JournalSuperblock jsb;
    std::memset(&jsb, 0, sizeof(JournalSuperblock));
    
    jsb.h_magic = JFS_MAGIC_NUMBER;
    jsb.h_block_type = JFS_SUPERBLOCK;
    jsb.h_sequence = 1;
    jsb.h_blocksize = layout.block_size;
    jsb.h_total_blocks = static_cast<uint32_t>(journal_blocks);
    jsb.h_start = 0;
    jsb.h_sequence2 = 1;
    jsb.h_err_no = 0;
    jsb.h_features = 0;
    jsb.h_feature_incompat = 0;
    jsb.h_feature_ro_compat = 0;
    
    // Copy UUID from filesystem
    // (In real implementation, would copy from EXT4Superblock)
    std::memset(jsb.h_uuid, 0, 16);
    
    jsb.h_nr_users = 1;
    jsb.h_dynsuper = 0;
    jsb.h_max_transaction = 0;
    jsb.h_max_trans_data = 0;
    jsb.h_checksum = 0;
    
    // Write journal superblock
    uint64_t jsb_offset = (start_sector * layout.bytes_per_sector) + 
                           (journal_start_block * layout.block_size);
    
    Result result = disk->write(&jsb, jsb_offset, sizeof(JournalSuperblock));
    if (result.failed()) {
        return Result::error("Failed to write journal superblock: " + result.message);
    }
    
    // Initialize journal blocks (set to zeros)
    std::vector<uint8_t> empty_block(layout.block_size, 0);
    for (uint64_t i = 1; i < journal_blocks; i++) {
        uint64_t block_offset = jsb_offset + (i * layout.block_size);
        result = disk->write(empty_block.data(), block_offset, layout.block_size);
        if (result.failed()) {
            return Result::error("Failed to write journal block " + 
                               std::to_string(i) + ": " + result.message);
        }
    }
    
    // Create journal inode
    EXT4Inode journal_inode_struct;
    journal_inode_struct.init_file(0600);
    
    // Set journal inode data
    journal_inode_struct.i_size_lo = static_cast<uint32_t>(journal_blocks * layout.block_size);
    journal_inode_struct.i_size_high = static_cast<uint32_t>((journal_blocks * layout.block_size) >> 32);
    journal_inode_struct.i_blocks_lo = static_cast<uint32_t>(journal_blocks * 2);  // 512-byte units
    journal_inode_struct.i_uid = 0;
    journal_inode_struct.i_gid = 0;
    
    // Set timestamps
    time_t now = std::time(nullptr);
    journal_inode_struct.i_atime = static_cast<uint32_t>(now);
    journal_inode_struct.i_ctime = static_cast<uint32_t>(now);
    journal_inode_struct.i_mtime = static_cast<uint32_t>(now);
    
    // Create extent for journal
    EXT4ExtentHeader* eh = reinterpret_cast<EXT4ExtentHeader*>(
        journal_inode_struct.i_block_union.i_block);
    eh->eh_magic = EXT4_EXTENT_MAGIC;
    eh->eh_entries = 1;
    eh->eh_max = 4;
    eh->eh_depth = 0;
    eh->eh_generation = 0;
    
    EXT4Extent* extent = reinterpret_cast<EXT4Extent*>(
        reinterpret_cast<uint8_t*>(journal_inode_struct.i_block_union.i_block) + 
        sizeof(EXT4ExtentHeader));
    extent->ee_block = 0;
    extent->ee_len = static_cast<uint16_t>(journal_blocks);
    extent->ee_start_hi = static_cast<uint16_t>((journal_start_block >> 32) & 0xFFFF);
    extent->ee_start_lo = static_cast<uint32_t>(journal_start_block & 0xFFFFFFFF);
    
    // Write journal inode
    uint32_t group = getInodeGroup(journal_inode, layout.inodes_per_group);
    uint32_t offset = getInodeOffset(journal_inode, layout.inodes_per_group);
    
    uint64_t inode_table_block = 5;  // Inode table starts at block 5
    uint64_t inode_offset = (start_sector * layout.bytes_per_sector) + 
                             (inode_table_block * layout.block_size) + 
                             (offset * layout.inode_size);
    
    result = disk->write(&journal_inode_struct, inode_offset, sizeof(EXT4Inode));
    if (result.failed()) {
        return Result::error("Failed to write journal inode: " + result.message);
    }
    
    // Mark journal blocks as used in bitmap
    for (uint64_t i = 0; i < journal_blocks; i++) {
        result = markBlockUsed(disk, start_sector, layout, 
                                static_cast<uint32_t>(group),
                                static_cast<uint32_t>(journal_start_block + i));
        if (result.failed()) {
            return result;
        }
    }
    
    // Mark journal inode as used
    result = markInodeUsed(disk, start_sector, layout, 0, journal_inode);
    if (result.failed()) {
        return result;
    }
    
    // ==================================================================
    // Link the journal into the ext4 superblock
    // ==================================================================
    // Read the primary superblock back from disk, set the journal inode and
    // UUID fields, and rewrite both primary + backup superblocks so the FS
    // advertises the journal that exists on disk.
    uint64_t sb_offset = (start_sector * layout.bytes_per_sector) + 1024;
    EXT4Superblock sb;
    Result sr = disk->read(&sb, sb_offset, sizeof(EXT4Superblock));
    if (sr.failed()) {
        return Result::error("Failed to read superblock for journal linkage: " +
                             sr.message);
    }

    // Sanity check magic before mutating (avoid corrupting non-ext4 data)
    if (sb.s_magic != EXT4_SUPER_MAGIC) {
        return Result::error("Superblock magic mismatch during journal linkage "
                             "(expected 0xEF53)");
    }

    sb.s_journal_inum = EXT4_JOURNAL_INO;
    std::memcpy(sb.s_journal_uuid, sb.s_uuid, 16);

    // Mirror the FS UUID into the journal superblock for consistency
    std::memcpy(jsb.h_uuid, sb.s_uuid, 16);
    result = disk->write(&jsb, jsb_offset, sizeof(JournalSuperblock));
    if (result.failed()) {
        return Result::error("Failed to update journal superblock UUID: " +
                             result.message);
    }

    result = disk->write(&sb, sb_offset, sizeof(EXT4Superblock));
    if (result.failed()) {
        return Result::error("Failed to write superblock with journal linkage: " +
                             result.message);
    }

    // Keep the backup superblocks consistent (groups 1, 3, 5)
    std::vector<uint32_t> backup_groups = {1, 3, 5};
    for (uint32_t bg : backup_groups) {
        if (bg < layout.num_groups) {
            uint64_t backup_offset = calculateBackupSuperblockOffset(
                start_sector, layout, bg);
            disk->write(&sb, backup_offset, sizeof(EXT4Superblock));
        }
    }

    return Result::ok();
}

} // namespace ext4
} // namespace opm
