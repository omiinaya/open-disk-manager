#include "opm/ext4_impl.hpp"
#include "opm/disk_io.hpp"
#include <random>
#include <ctime>

namespace opm {
namespace ext4 {

// ============================================================================
// EXT4Layout Implementation - Phase 3.3.1
// ============================================================================

void EXT4Layout::calculate(uint64_t volume_size_bytes, uint32_t block_size_val) {
    total_size = volume_size_bytes;
    block_size = block_size_val;
    inode_size = 256;  // Standard for ext4
    desc_size = 64;    // 64-bit group descriptors
    flex_bg = true;
    flex_bg_size = 16;
    
    // Calculate blocks per group (usually 8 * block_size, max 32768)
    blocks_per_group = 8 * (block_size / 1024);
    if (blocks_per_group > 32768) {
        blocks_per_group = 32768;
    }
    
    // Clusters per group (same as blocks for now)
    clusters_per_group = blocks_per_group;
    
    // Calculate total blocks
    uint64_t total_blocks = volume_size_bytes / block_size;
    
    // Calculate number of groups
    num_groups = static_cast<uint32_t>((total_blocks + blocks_per_group - 1) / blocks_per_group);
    
    // Calculate inodes per group (one inode per 16KB)
    inodes_per_group = (blocks_per_group * block_size) / 16384;
    if (inodes_per_group > 8192) {
        inodes_per_group = 8192;
    }
    
    // Journal size (about 1% of filesystem, max 128MB)
    journal_size = volume_size_bytes / 100;
    if (journal_size > 128 * 1024 * 1024) {
        journal_size = 128 * 1024 * 1024;
    }
    if (journal_size < 32 * 1024 * 1024) {
        journal_size = 32 * 1024 * 1024;  // Minimum 32MB
    }
}

bool EXT4Layout::validate() const {
    if (block_size != 1024 && block_size != 2048 && 
        block_size != 4096 && block_size != 65536) {
        return false;
    }
    
    if (blocks_per_group == 0 || blocks_per_group > 32768) {
        return false;
    }
    
    if (inodes_per_group == 0 || inodes_per_group > 8192) {
        return false;
    }
    
    if (num_groups == 0) {
        return false;
    }
    
    return true;
}

// ============================================================================
// EXT4Superblock Implementation
// ============================================================================

void EXT4Superblock::init(uint64_t volume_size, uint32_t block_size) {
    // Calculate layout
    EXT4Layout layout;
    layout.calculate(volume_size, block_size);
    
    // Initialize basic fields
    s_inodes_count = layout.num_groups * layout.inodes_per_group;
    s_blocks_count_lo = static_cast<uint32_t>((volume_size / block_size) & 0xFFFFFFFF);
    s_blocks_count_hi = static_cast<uint32_t>((volume_size / block_size) >> 32);
    
    // Reserved blocks (5%)
    uint64_t reserved_blocks = (volume_size / block_size) / 20;
    s_r_blocks_count_lo = static_cast<uint32_t>(reserved_blocks & 0xFFFFFFFF);
    s_r_blocks_count_hi = static_cast<uint32_t>(reserved_blocks >> 32);
    
    // Free blocks (all initially)
    s_free_blocks_count_lo = s_blocks_count_lo;
    s_free_blocks_count_hi = s_blocks_count_hi;
    
    // Free inodes
    s_free_inodes_count = s_inodes_count;
    
    // First data block
    s_first_data_block = (block_size == 1024) ? 1 : 0;
    
    // Block size
    s_log_block_size = 0;
    uint32_t tmp = block_size;
    while (tmp > 1024) {
        tmp >>= 1;
        s_log_block_size++;
    }
    
    // Cluster size (same as block size)
    s_log_cluster_size = s_log_block_size;
    
    // Blocks per group
    s_blocks_per_group = layout.blocks_per_group;
    s_clusters_per_group = layout.clusters_per_group;
    s_inodes_per_group = layout.inodes_per_group;
    
    // Times
    time_t now = std::time(nullptr);
    s_mtime = static_cast<uint32_t>(now);
    s_wtime = static_cast<uint32_t>(now);
    s_lastcheck = static_cast<uint32_t>(now);
    s_mkfs_time = static_cast<uint32_t>(now);
    
    // Mount count
    s_mnt_count = 0;
    s_max_mnt_count = 20;
    
    // Magic and state
    s_magic = EXT4_SUPER_MAGIC;
    s_state = EXT4_VALID_FS;
    s_errors = 0;  // Continue on error
    s_minor_rev_level = 0;
    
    // Revision level (1.0 for ext4)
    s_rev_level = 1;
    s_creator_os = 0;  // Linux
    
    // Default UID/GID
    s_def_resuid = 0;
    s_def_resgid = 0;
    
    // First inode (ext4 reserves 11 inodes)
    s_first_ino = 11;
    s_inode_size = layout.inode_size;
    s_block_group_nr = 0;
    
    // Feature flags
    s_feature_compat = EXT4_FEATURE_COMPAT_EXT_ATTR |
                       EXT4_FEATURE_COMPAT_RESIZE_INODE |
                       EXT4_FEATURE_COMPAT_DIR_INDEX;
    
    s_feature_incompat = EXT4_FEATURE_INCOMPAT_FILETYPE |
                         EXT4_FEATURE_INCOMPAT_EXTENTS |
                         EXT4_FEATURE_INCOMPAT_64BIT |
                         EXT4_FEATURE_INCOMPAT_FLEX_BG;
    
    s_feature_ro_compat = EXT4_FEATURE_RO_COMPAT_SPARSE_SUPER |
                          EXT4_FEATURE_RO_COMPAT_LARGE_FILE |
                          EXT4_FEATURE_RO_COMPAT_GDT_CSUM |
                          EXT4_FEATURE_RO_COMPAT_DIR_NLINK |
                          EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE;
    
    // UUID
    generateUUID(s_uuid);
    
    // Preallocation
    s_prealloc_blocks = 17;
    s_prealloc_dir_blocks = 2;
    
    // Group descriptor size
    s_desc_size = layout.desc_size;
    
    // Default mount options
    s_default_mount_opts = 0;
    s_first_meta_bg = 0;
    
    // Reserved GDT blocks
    s_reserved_gdt_blocks = 0;
    
    // Journal (will be initialized later)
    s_journal_inum = 0;  // Will be set when journal is created
    s_journal_dev = 0;
    
    // MMP
    s_mmp_interval = 0;
    s_mmp_block = 0;
    
    // RAID
    s_raid_stride = 0;
    s_raid_stripe_width = 0;
    
    // FLEX_BG
    s_log_groups_per_flex = 4;  // 16 groups per flex
    
    // Initialize unused fields
    std::memset(s_volume_name, 0, sizeof(s_volume_name));
    std::memset(s_last_mounted, 0, sizeof(s_last_mounted));
    std::memset(s_journal_uuid, 0, sizeof(s_journal_uuid));
    std::memset(s_hash_seed, 0, sizeof(s_hash_seed));
    std::memset(s_mount_opts, 0, sizeof(s_mount_opts));
}

// ============================================================================
// EXT4GroupDesc Implementation
// ============================================================================

void EXT4GroupDesc::init(uint32_t group_num, uint32_t block_size) {
    (void)block_size;
    // Recalculate layout for this specific group
    // This is a simplified version
    uint32_t blocks_per_group = 8 * (block_size / 1024);
    if (blocks_per_group > 32768) {
        blocks_per_group = 32768;
    }
    
    // Calculate bitmap locations
    uint64_t group_start = static_cast<uint64_t>(group_num) * blocks_per_group;
    
    // Block bitmap at group start + 3 (after superblock and GDT)
    bg_block_bitmap_lo = static_cast<uint32_t>((group_start + 3) & 0xFFFFFFFF);
    bg_block_bitmap_hi = static_cast<uint32_t>((group_start + 3) >> 32);
    
    // Inode bitmap at block bitmap + 1
    bg_inode_bitmap_lo = static_cast<uint32_t>((group_start + 4) & 0xFFFFFFFF);
    bg_inode_bitmap_hi = static_cast<uint32_t>((group_start + 4) >> 32);
    
    // Inode table at inode bitmap + 1
    uint64_t inode_table = group_start + 5;
    bg_inode_table_lo = static_cast<uint32_t>(inode_table & 0xFFFFFFFF);
    bg_inode_table_hi = static_cast<uint32_t>(inode_table >> 32);
    
    // Initialize counts
    bg_free_blocks_count_lo = blocks_per_group - 5;  // 5 blocks used for metadata
    bg_free_blocks_count_hi = 0;
    bg_free_inodes_count_lo = 8192;  // Will be adjusted
    bg_free_inodes_count_hi = 0;
    bg_used_dirs_count_lo = 0;
    bg_used_dirs_count_hi = 0;
    
    // Flags
    bg_flags = 0;
    
    // Reserved
    bg_reserved = 0;
    
    // Unused inodes
    bg_itable_unused_lo = 8192;
    bg_itable_unused_hi = 0;
    
    // Checksums (will be calculated)
    bg_block_bitmap_csum_lo = 0;
    bg_block_bitmap_csum_hi = 0;
    bg_inode_bitmap_csum_lo = 0;
    bg_inode_bitmap_csum_hi = 0;
    bg_checksum = 0;
    
    // Exclude bitmap (not used)
    bg_exclude_bitmap_lo = 0;
    bg_exclude_bitmap_hi = 0;
}

// ============================================================================
// EXT4Inode Implementation
// ============================================================================

void EXT4Inode::init_directory(uint32_t mode) {
    std::memset(this, 0, sizeof(*this));
    
    i_mode = S_IFDIR | (mode & 0777);
    i_uid = 0;
    i_uid_high = 0;
    i_gid = 0;
    i_gid_high = 0;
    i_size_lo = 0;  // Will be set when directory is created
    i_size_high = 0;
    i_atime = static_cast<uint32_t>(std::time(nullptr));
    i_ctime = i_atime;
    i_mtime = i_atime;
    i_dtime = 0;
    i_links_count = 2;  // . and ..
    i_blocks_lo = 0;
    i_blocks_high = 0;
    i_flags = 0;
    i_generation = 0;
    i_file_acl_lo = 0;
    i_file_acl_high = 0;
    i_obso_faddr = 0;
    i_checksum_lo = 0;
    i_reserved = 0;
    i_extra_isize = 256 - 128;  // Extra inode size beyond 128 bytes
    i_checksum_hi = 0;
    i_ctime_extra = 0;
    i_mtime_extra = 0;
    i_atime_extra = 0;
    i_crtime = 0;
    i_crtime_extra = 0;
    i_version_hi = 0;
    
    // Initialize extent tree header
    EXT4ExtentHeader* eh = reinterpret_cast<EXT4ExtentHeader*>(i_block_union.i_block);
    eh->eh_magic = EXT4_EXTENT_MAGIC;
    eh->eh_entries = 0;
    eh->eh_max = 4;  // Can hold 4 extents in inode
    eh->eh_depth = 0;
    eh->eh_generation = 0;
}

void EXT4Inode::init_file(uint32_t mode) {
    std::memset(this, 0, sizeof(*this));
    
    i_mode = S_IFREG | (mode & 0777);
    i_uid = 0;
    i_uid_high = 0;
    i_gid = 0;
    i_gid_high = 0;
    i_size_lo = 0;
    i_size_high = 0;
    i_atime = static_cast<uint32_t>(std::time(nullptr));
    i_ctime = i_atime;
    i_mtime = i_atime;
    i_dtime = 0;
    i_links_count = 1;
    i_blocks_lo = 0;
    i_blocks_high = 0;
    i_flags = 0;
    i_generation = 0;
    i_file_acl_lo = 0;
    i_file_acl_high = 0;
    i_obso_faddr = 0;
    i_checksum_lo = 0;
    i_reserved = 0;
    i_extra_isize = 256 - 128;
    i_checksum_hi = 0;
    i_ctime_extra = 0;
    i_mtime_extra = 0;
    i_atime_extra = 0;
    i_crtime = 0;
    i_crtime_extra = 0;
    i_version_hi = 0;
    
    // Initialize extent tree
    EXT4ExtentHeader* eh = reinterpret_cast<EXT4ExtentHeader*>(i_block_union.i_block);
    eh->eh_magic = EXT4_EXTENT_MAGIC;
    eh->eh_entries = 0;
    eh->eh_max = 4;
    eh->eh_depth = 0;
    eh->eh_generation = 0;
}

// ============================================================================
// Utility Functions
// ============================================================================

void initSuperblock(EXT4Superblock& sb, const EXT4Layout& layout,
                    uint32_t feature_incompat, uint32_t feature_compat,
                    uint32_t feature_ro_compat) {
    sb.init(layout.total_size, layout.block_size);
    
    // Override feature flags
    sb.s_feature_incompat = feature_incompat;
    sb.s_feature_compat = feature_compat;
    sb.s_feature_ro_compat = feature_ro_compat;
}

void initGroupDesc(EXT4GroupDesc& gd, uint32_t group_num, uint32_t block_size) {
    gd.init(group_num, block_size);
}

void initInode(EXT4Inode& inode, uint16_t mode, uint16_t uid, 
               uint16_t gid, uint32_t size) {
    (void)uid;
    (void)gid;
    (void)size;
    
    // Check if directory by testing mode bits
    if ((mode & S_IFDIR) == S_IFDIR) {
        inode.init_directory(mode & 0777);
    } else {
        inode.init_file(mode & 0777);
    }
}

uint32_t getBlockGroup(uint64_t block, uint32_t blocks_per_group) {
    return static_cast<uint32_t>(block / blocks_per_group);
}

uint32_t getInodeGroup(uint32_t inode, uint32_t inodes_per_group) {
    return (inode - 1) / inodes_per_group;
}

uint32_t getInodeOffset(uint32_t inode, uint32_t inodes_per_group) {
    return (inode - 1) % inodes_per_group;
}

uint32_t getNumGroups(uint64_t total_blocks, uint32_t blocks_per_group) {
    return static_cast<uint32_t>((total_blocks + blocks_per_group - 1) / blocks_per_group);
}

void generateUUID(uint8_t uuid[16]) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(0, 255);
    
    for (int i = 0; i < 16; i++) {
        uuid[i] = static_cast<uint8_t>(dis(gen));
    }
    
    // Set version (4 = random)
    uuid[6] = (uuid[6] & 0x0F) | 0x40;
    
    // Set variant (10 = RFC 4122)
    uuid[8] = (uuid[8] & 0x3F) | 0x80;
}

} // namespace ext4
} // namespace opm
