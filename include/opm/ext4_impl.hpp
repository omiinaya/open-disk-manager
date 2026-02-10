#pragma once

#include "types.hpp"
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>
#include <string>

namespace opm {

// Forward declaration
class DiskIO;

namespace ext4 {

// Use the forward declaration
using ::opm::DiskIO;

// ============================================================================
// ext4 Constants
// ============================================================================

// Magic number
constexpr uint16_t EXT4_SUPER_MAGIC = 0xEF53;

// Block sizes
constexpr uint32_t EXT4_MIN_BLOCK_SIZE = 1024;
constexpr uint32_t EXT4_MAX_BLOCK_SIZE = 65536;

// Feature incompat flags
constexpr uint32_t EXT4_FEATURE_INCOMPAT_COMPRESSION = 0x0001;
constexpr uint32_t EXT4_FEATURE_INCOMPAT_FILETYPE = 0x0002;
constexpr uint32_t EXT4_FEATURE_INCOMPAT_RECOVER = 0x0004;
constexpr uint32_t EXT4_FEATURE_INCOMPAT_JOURNAL_DEV = 0x0008;
constexpr uint32_t EXT4_FEATURE_INCOMPAT_META_BG = 0x0010;
constexpr uint32_t EXT4_FEATURE_INCOMPAT_EXTENTS = 0x0040;
constexpr uint32_t EXT4_FEATURE_INCOMPAT_64BIT = 0x0080;
constexpr uint32_t EXT4_FEATURE_INCOMPAT_MMP = 0x0100;
constexpr uint32_t EXT4_FEATURE_INCOMPAT_FLEX_BG = 0x0200;
constexpr uint32_t EXT4_FEATURE_INCOMPAT_EA_INODE = 0x0400;
constexpr uint32_t EXT4_FEATURE_INCOMPAT_DIRDATA = 0x1000;
constexpr uint32_t EXT4_FEATURE_INCOMPAT_CSUM_SEED = 0x2000;
constexpr uint32_t EXT4_FEATURE_INCOMPAT_LARGEDIR = 0x4000;
constexpr uint32_t EXT4_FEATURE_INCOMPAT_INLINE_DATA = 0x8000;
constexpr uint32_t EXT4_FEATURE_INCOMPAT_ENCRYPT = 0x10000;

// Feature compat flags
constexpr uint32_t EXT4_FEATURE_COMPAT_DIR_PREALLOC = 0x0001;
constexpr uint32_t EXT4_FEATURE_COMPAT_IMAGIC_INODES = 0x0002;
constexpr uint32_t EXT4_FEATURE_COMPAT_HAS_JOURNAL = 0x0004;
constexpr uint32_t EXT4_FEATURE_COMPAT_EXT_ATTR = 0x0008;
constexpr uint32_t EXT4_FEATURE_COMPAT_RESIZE_INODE = 0x0010;
constexpr uint32_t EXT4_FEATURE_COMPAT_DIR_INDEX = 0x0020;
constexpr uint32_t EXT4_FEATURE_COMPAT_LAZY_BG = 0x0040;
constexpr uint32_t EXT4_FEATURE_COMPAT_EXCLUDE_INODE = 0x0080;
constexpr uint32_t EXT4_FEATURE_COMPAT_EXCLUDE_BITMAP = 0x0100;
constexpr uint32_t EXT4_FEATURE_COMPAT_SPARSE_SUPER2 = 0x0200;

// Feature ro compat flags
constexpr uint32_t EXT4_FEATURE_RO_COMPAT_SPARSE_SUPER = 0x0001;
constexpr uint32_t EXT4_FEATURE_RO_COMPAT_LARGE_FILE = 0x0002;
constexpr uint32_t EXT4_FEATURE_RO_COMPAT_BTREE_DIR = 0x0004;
constexpr uint32_t EXT4_FEATURE_RO_COMPAT_HUGE_FILE = 0x0008;
constexpr uint32_t EXT4_FEATURE_RO_COMPAT_GDT_CSUM = 0x0010;
constexpr uint32_t EXT4_FEATURE_RO_COMPAT_DIR_NLINK = 0x0020;
constexpr uint32_t EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE = 0x0040;
constexpr uint32_t EXT4_FEATURE_RO_COMPAT_HAS_SNAPSHOT = 0x0080;
constexpr uint32_t EXT4_FEATURE_RO_COMPAT_QUOTA = 0x0100;
constexpr uint32_t EXT4_FEATURE_RO_COMPAT_BIGALLOC = 0x0200;
constexpr uint32_t EXT4_FEATURE_RO_COMPAT_METADATA_CSUM = 0x0400;
constexpr uint32_t EXT4_FEATURE_RO_COMPAT_REPLICA = 0x0800;
constexpr uint32_t EXT4_FEATURE_RO_COMPAT_READONLY = 0x1000;
constexpr uint32_t EXT4_FEATURE_RO_COMPAT_PROJECT = 0x2000;
constexpr uint32_t EXT4_FEATURE_RO_COMPAT_VERITY = 0x8000;

// Inode numbers
constexpr uint32_t EXT4_ROOT_INO = 2;
constexpr uint32_t EXT4_JOURNAL_INO = 8;
constexpr uint32_t EXT4_RESIZE_INO = 7;

// State flags
constexpr uint16_t EXT4_VALID_FS = 1;
constexpr uint16_t EXT4_ERROR_FS = 2;
constexpr uint16_t EXT4_ORPHAN_FS = 4;

// Inode mode types
constexpr uint16_t S_IFDIR = 0040000;  // Directory
constexpr uint16_t S_IFREG = 0100000;  // Regular file
constexpr uint16_t S_IFLNK = 0120000;  // Symbolic link

// Extent tree constants
constexpr uint32_t EXT4_EXTENT_MAGIC = 0xF30A;
constexpr uint32_t EXT4_EXTENT_HEADER_SIZE = 12;

// ============================================================================
// ext4 Superblock
// Total size: 1024 bytes (at offset 1024 from partition start)
// ============================================================================
struct __attribute__((packed)) EXT4Superblock {
    // 0x00-0x03
    uint32_t s_inodes_count;           // Total inode count
    uint32_t s_blocks_count_lo;        // Total block count (low 32 bits)
    uint32_t s_r_blocks_count_lo;      // Reserved block count (low 32 bits)
    uint32_t s_free_blocks_count_lo;    // Free block count (low 32 bits)
    
    // 0x10-0x13
    uint32_t s_free_inodes_count;      // Free inode count
    uint32_t s_first_data_block;         // First data block
    uint32_t s_log_block_size;           // Block size = 1024 << s_log_block_size
    uint32_t s_log_cluster_size;         // Cluster size = 1024 << s_log_cluster_size
    
    // 0x20-0x23
    uint32_t s_blocks_per_group;         // Blocks per group
    uint32_t s_clusters_per_group;       // Clusters per group
    uint32_t s_inodes_per_group;         // Inodes per group
    uint32_t s_mtime;                    // Mount time
    
    // 0x30-0x33
    uint32_t s_wtime;                    // Write time
    uint16_t s_mnt_count;                // Mount count
    uint16_t s_max_mnt_count;            // Max mount count before check
    uint16_t s_magic;                    // 0xEF53
    uint16_t s_state;                    // Filesystem state
    
    // 0x40-0x43
    uint16_t s_errors;                   // Error handling
    uint16_t s_minor_rev_level;          // Minor revision level
    uint32_t s_lastcheck;                // Last check time
    uint32_t s_checkinterval;            // Check interval
    uint32_t s_creator_os;               // Creator OS
    uint32_t s_rev_level;                // Revision level
    uint16_t s_def_resuid;               // Default reserved UID
    uint16_t s_def_resgid;               // Default reserved GID
    
    // ext2/3/4 specific (revision >= 1.0) - 0x54
    uint32_t s_first_ino;                // First non-reserved inode
    uint16_t s_inode_size;               // Inode size
    uint16_t s_block_group_nr;           // Block group number of this superblock
    uint32_t s_feature_compat;           // Compatible features
    uint32_t s_feature_incompat;           // Incompatible features
    uint32_t s_feature_ro_compat;        // Read-only compatible features
    uint8_t s_uuid[16];                  // Volume UUID
    char s_volume_name[16];              // Volume name
    char s_last_mounted[64];             // Last mounted directory
    uint32_t s_algorithm_usage_bitmap; // Compression algorithm usage
    
    // ext4 specific
    uint8_t s_prealloc_blocks;           // Preallocated blocks
    uint8_t s_prealloc_dir_blocks;       // Preallocated directory blocks
    uint16_t s_reserved_gdt_blocks;      // Reserved GDT blocks
    
    // Journal support
    uint8_t s_journal_uuid[16];          // Journal UUID
    uint32_t s_journal_inum;             // Journal inode
    uint32_t s_journal_dev;              // Journal device
    uint32_t s_last_orphan;              // Last orphan
    uint32_t s_hash_seed[4];             // HTREE hash seed
    uint8_t s_def_hash_version;          // Default hash version
    uint8_t s_reserved_char_pad;
    uint16_t s_desc_size;                // Size of group descriptor
    
    // 64-bit support
    uint32_t s_default_mount_opts;       // Default mount options
    uint32_t s_first_meta_bg;            // First metablock group
    uint32_t s_mkfs_time;                // Filesystem creation time
    uint32_t s_jnl_blocks[17];           // Journal blocks
    
    // 64-bit block counts
    uint32_t s_blocks_count_hi;          // High 32 bits of block count
    uint32_t s_r_blocks_count_hi;        // High 32 bits of reserved blocks
    uint32_t s_free_blocks_count_hi;     // High 32 bits of free blocks
    uint16_t s_min_extra_isize;          // Min extra inode size
    uint16_t s_want_extra_isize;         // Desired extra inode size
    uint32_t s_flags;                    // Misc flags
    uint16_t s_raid_stride;              // RAID stride
    uint16_t s_mmp_interval;             // MMP check interval
    uint64_t s_mmp_block;                // MMP block
    uint32_t s_raid_stripe_width;        // RAID stripe width
    uint8_t s_log_groups_per_flex;       // FLEX_BG group size
    uint8_t s_reserved_char_pad2;
    uint16_t s_reserved_pad;
    uint64_t s_kbytes_written;           // Lifetime KB written
    uint32_t s_snapshot_inum;            // Snapshot inode
    uint32_t s_snapshot_id;              // Snapshot ID
    uint64_t s_snapshot_r_blocks_count;  // Reserved blocks for snapshot
    uint32_t s_snapshot_list;          // Snapshot list head
    
    // Error count
    uint32_t s_error_count;              // Number of errors
    uint32_t s_first_error_time;         // First error time
    uint32_t s_first_error_ino;          // First error inode
    uint64_t s_first_error_block;        // First error block
    uint8_t s_first_error_func[32];      // First error function
    uint32_t s_last_error_time;          // Last error time
    uint32_t s_last_error_ino;           // Last error inode
    uint32_t s_last_error_line;          // Last error line
    uint64_t s_last_error_block;         // Last error block
    uint8_t s_last_error_func[32];       // Last error function
    
    // Mount options
    uint8_t s_mount_opts[64];
    uint32_t s_usr_quota_inum;           // User quota inode
    uint32_t s_grp_quota_inum;           // Group quota inode
    uint32_t s_overhead_clusters;        // Overhead clusters
    uint32_t s_backup_bgs[2];            // Block groups to back up
    uint8_t s_encrypt_algos[4];          // Encryption algorithms
    uint8_t s_encrypt_pw_salt[16];       // Salt for string2key
    uint32_t s_lpf_ino;                  // Orphan file inode
    uint32_t s_reserved[99];             // Padding
    uint32_t s_checksum;                 // Superblock checksum
    
    void init(uint64_t volume_size, uint32_t block_size = 4096);
};

// ============================================================================
// Block Group Descriptor
// Total size: 32 bytes (64 bytes with 64-bit support)
// ============================================================================
struct __attribute__((packed)) EXT4GroupDesc {
    uint32_t bg_block_bitmap_lo;         // Block bitmap block (low 32 bits)
    uint32_t bg_inode_bitmap_lo;         // Inode bitmap block (low 32 bits)
    uint32_t bg_inode_table_lo;          // Inode table block (low 32 bits)
    uint16_t bg_free_blocks_count_lo;    // Free blocks count (low 16 bits)
    uint16_t bg_free_inodes_count_lo;    // Free inodes count (low 16 bits)
    uint16_t bg_used_dirs_count_lo;      // Directories count (low 16 bits)
    uint16_t bg_flags;                   // Block group flags
    uint32_t bg_exclude_bitmap_lo;       // Snapshot exclusion bitmap
    uint16_t bg_block_bitmap_csum_lo;    // Block bitmap checksum
    uint16_t bg_inode_bitmap_csum_lo;    // Inode bitmap checksum
    uint16_t bg_itable_unused_lo;        // Unused inodes count
    uint16_t bg_checksum;                // Group descriptor checksum
    
    // 64-bit fields
    uint32_t bg_block_bitmap_hi;         // Block bitmap block (high 32 bits)
    uint32_t bg_inode_bitmap_hi;         // Inode bitmap block (high 32 bits)
    uint32_t bg_inode_table_hi;          // Inode table block (high 32 bits)
    uint16_t bg_free_blocks_count_hi;    // Free blocks count (high 16 bits)
    uint16_t bg_free_inodes_count_hi;    // Free inodes count (high 16 bits)
    uint16_t bg_used_dirs_count_hi;      // Directories count (high 16 bits)
    uint16_t bg_itable_unused_hi;        // Unused inodes count (high 16 bits)
    uint32_t bg_exclude_bitmap_hi;       // Snapshot exclusion bitmap (high)
    uint16_t bg_block_bitmap_csum_hi;    // Block bitmap checksum (high)
    uint16_t bg_inode_bitmap_csum_hi;    // Inode bitmap checksum (high)
    uint32_t bg_reserved;
    
    void init(uint32_t group_num, uint32_t block_size);
};

// ============================================================================
// ext4 Inode
// Total size: 128-256 bytes (typically 256)
// ============================================================================
struct __attribute__((packed)) EXT4Inode {
    uint16_t i_mode;                     // File mode
    uint16_t i_uid;                      // Low 16 bits of UID
    uint32_t i_size_lo;                  // File size (low 32 bits)
    uint32_t i_atime;                    // Access time
    uint32_t i_ctime;                    // Inode change time
    uint32_t i_mtime;                    // Modification time
    uint32_t i_dtime;                    // Deletion time
    uint16_t i_gid;                      // Low 16 bits of GID
    uint16_t i_links_count;              // Link count
    uint32_t i_blocks_lo;                // Block count (low 32 bits)
    uint32_t i_flags;                    // File flags
    
    union {
        uint32_t i_block[15];          // Block map or extent tree
        struct {
            uint32_t i_version_hi;     // High 32 bits of version
            uint32_t i_projid;         // Project ID
        } linux1;
    } i_block_union;
    
    uint32_t i_generation;               // File version
    uint32_t i_file_acl_lo;              // File ACL (low 32 bits)
    uint32_t i_size_high;                // File size (high 32 bits)
    uint32_t i_obso_faddr;               // Obsoleted fragment address
    
    // Linux 2 specific
    uint16_t i_blocks_high;
    uint16_t i_file_acl_high;
    uint16_t i_uid_high;
    uint16_t i_gid_high;
    uint16_t i_checksum_lo;
    uint16_t i_reserved;
    
    // Extra fields
    uint32_t i_extra_isize;
    uint32_t i_checksum_hi;
    uint32_t i_ctime_extra;
    uint32_t i_mtime_extra;
    uint32_t i_atime_extra;
    uint64_t i_crtime;
    uint32_t i_crtime_extra;
    uint32_t i_version_hi;
    
    void init_directory(uint32_t mode = 0755);
    void init_file(uint32_t mode = 0644);
};

// ============================================================================
// ext4 Directory Entry
// ============================================================================
struct __attribute__((packed)) EXT4DirEntry {
    uint32_t inode;                      // Inode number
    uint16_t rec_len;                    // Directory entry length
    uint8_t name_len;                    // Name length
    uint8_t file_type;                   // File type
    char name[255];                      // Name (variable length)
};

// Directory entry file types
constexpr uint8_t EXT4_FT_UNKNOWN = 0;
constexpr uint8_t EXT4_FT_REG_FILE = 1;
constexpr uint8_t EXT4_FT_DIR = 2;
constexpr uint8_t EXT4_FT_CHRDEV = 3;
constexpr uint8_t EXT4_FT_BLKDEV = 4;
constexpr uint8_t EXT4_FT_FIFO = 5;
constexpr uint8_t EXT4_FT_SOCK = 6;
constexpr uint8_t EXT4_FT_SYMLINK = 7;

// ============================================================================
// ext4 Extent Tree Structures
// ============================================================================
struct __attribute__((packed)) EXT4ExtentHeader {
    uint16_t eh_magic;                   // 0xF30A
    uint16_t eh_entries;                 // Number of valid entries
    uint16_t eh_max;                     // Maximum number of entries
    uint16_t eh_depth;                   // Depth of tree
    uint32_t eh_generation;              // Generation of tree
    
    bool isValid() const { return eh_magic == EXT4_EXTENT_MAGIC; }
};

struct __attribute__((packed)) EXT4Extent {
    uint32_t ee_block;                   // First logical block
    uint16_t ee_len;                     // Number of blocks
    uint16_t ee_start_hi;                // High 16 bits of physical block
    uint32_t ee_start_lo;                // Low 32 bits of physical block
    
    uint64_t getPhysicalBlock() const {
        return (static_cast<uint64_t>(ee_start_hi) << 32) | ee_start_lo;
    }
};

struct __attribute__((packed)) EXT4ExtentIdx {
    uint32_t ei_block;                   // Hash of blocks covered
    uint32_t ei_leaf_lo;                 // Physical block of child node (low)
    uint16_t ei_leaf_hi;                 // Physical block of child node (high)
    uint16_t ei_unused;
    
    uint64_t getLeafBlock() const {
        return (static_cast<uint64_t>(ei_leaf_hi) << 32) | ei_leaf_lo;
    }
};

// ============================================================================
// ext4 Layout Calculator
// ============================================================================
class EXT4Layout {
public:
    uint64_t total_size;                 // Total volume size in bytes
    uint32_t block_size;               // Block size (usually 4096)
    uint32_t blocks_per_group;         // Blocks per group
    uint32_t clusters_per_group;         // Clusters per group
    uint32_t inodes_per_group;         // Inodes per group
    uint32_t num_groups;                 // Number of block groups
    uint32_t inode_size;                 // Inode size (usually 256)
    uint32_t desc_size;                  // Group descriptor size (32 or 64)
    bool flex_bg;                      // Use flexible block groups
    uint32_t flex_bg_size;             // Flexible block group size
    uint64_t journal_size;               // Journal size
    
    void calculate(uint64_t volume_size_bytes, uint32_t block_size = 4096);
    
    uint64_t getBlockOffset(uint64_t block_num) const {
        return block_num * block_size;
    }
    
    uint64_t getGroupStartBlock(uint32_t group) const {
        return static_cast<uint64_t>(group) * blocks_per_group;
    }
    
    bool validate() const;
};

// ============================================================================
// ext4 Operations
// ============================================================================

// Initialize superblock
void initSuperblock(EXT4Superblock& sb, const EXT4Layout& layout,
                    uint32_t feature_incompat, uint32_t feature_compat,
                    uint32_t feature_ro_compat);

// Initialize group descriptor
void initGroupDesc(EXT4GroupDesc& gd, uint32_t group_num, const EXT4Layout& layout);

// Initialize inode
void initInode(EXT4Inode& inode, uint16_t mode, uint16_t uid = 0, 
               uint16_t gid = 0, uint32_t size = 0);

// Calculate block group from block number
uint32_t getBlockGroup(uint64_t block, uint32_t blocks_per_group);

// Calculate block group from inode number
uint32_t getInodeGroup(uint32_t inode, uint32_t inodes_per_group);

// Calculate offset of inode within its group
uint32_t getInodeOffset(uint32_t inode, uint32_t inodes_per_group);

// Calculate number of group descriptors
uint32_t getNumGroups(uint64_t total_blocks, uint32_t blocks_per_group);

// Generate UUID
void generateUUID(uint8_t uuid[16]);

// ============================================================================
// ext4 Format Operations
// ============================================================================

Result formatEXT4(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                  uint64_t size_bytes, const std::string& label);

Result createSuperblock(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                        const EXT4Layout& layout, const std::string& label);

Result createGroupDescriptors(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                               const EXT4Layout& layout);

Result createBlockBitmaps(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                           const EXT4Layout& layout);

Result createInodeBitmaps(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                           const EXT4Layout& layout);

Result createInodeTable(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                          const EXT4Layout& layout);

Result createRootDirectory(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                            const EXT4Layout& layout, const std::string& label);

Result createJournal(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                      const EXT4Layout& layout);

// ============================================================================
// ext4 Check Operations
// ============================================================================

Result checkEXT4(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                  bool repair = false, std::vector<std::string>* errors = nullptr);

Result checkSuperblock(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                        bool repair, std::vector<std::string>* errors);

Result checkGroupDescriptors(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                               const EXT4Layout& layout, bool repair,
                               std::vector<std::string>* errors);

Result checkBlockBitmaps(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                          const EXT4Layout& layout, bool repair,
                          std::vector<std::string>* errors);

Result checkInodeBitmaps(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                           const EXT4Layout& layout, bool repair,
                           std::vector<std::string>* errors);

Result checkInodeTable(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                         const EXT4Layout& layout, bool repair,
                         std::vector<std::string>* errors);

// ============================================================================
// ext4 Resize Operations
// ============================================================================

Result resizeEXT4(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                    uint64_t new_size_bytes);

Result extendBlockGroups(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                          const EXT4Layout& old_layout, const EXT4Layout& new_layout);

Result updateSuperblockForResize(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                                   const EXT4Layout& layout);

// ============================================================================
// ext4 Boot/Superblock Helper Functions
// ============================================================================

uint64_t calculateBackupSuperblockOffset(uint64_t start_sector,
                                            const EXT4Layout& layout,
                                            uint32_t group);

uint64_t calculateBackupGDTOffset(uint64_t start_sector,
                                   const EXT4Layout& layout,
                                   uint32_t group);

uint32_t calculateInodeTableSize(const EXT4Layout& layout);

// ============================================================================
// ext4 Root Directory Helper Functions
// ============================================================================

Result markBlockUsed(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                      const EXT4Layout& layout, uint32_t group, uint32_t block);

Result markInodeUsed(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                      const EXT4Layout& layout, uint32_t group, uint32_t inode);

Result updateGroupDescriptorUsed(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                                   const EXT4Layout& layout, uint32_t group,
                                   uint32_t blocks_used, uint32_t dirs_used, 
                                   uint32_t inodes_used);

// ============================================================================
// ext4 Check Functions
// ============================================================================

Result checkSuperblock(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                        bool repair, std::vector<std::string>* errors);

Result checkGroupDescriptors(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                               const EXT4Layout& layout, bool repair,
                               std::vector<std::string>* errors);

Result checkBlockBitmaps(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                          const EXT4Layout& layout, bool repair,
                          std::vector<std::string>* errors);

Result checkInodeBitmaps(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                          const EXT4Layout& layout, bool repair,
                          std::vector<std::string>* errors);

Result checkRootInode(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                        const EXT4Layout& layout, bool repair,
                        std::vector<std::string>* errors);

// ============================================================================
// ext4 Resize Functions
// ============================================================================

Result resizeEXT4(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                    uint64_t new_size_bytes);

Result extendBlockGroups(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                          const EXT4Layout& old_layout, const EXT4Layout& new_layout);

Result createGroupBitmaps(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                           const EXT4Layout& layout, uint32_t group);

Result updateSuperblockForResize(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                                   const EXT4Layout& layout);

} // namespace ext4
} // namespace opm
