#pragma once

#include "types.hpp"
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>
#include <string>

namespace opm {
namespace fat32 {

// ============================================================================
// FAT32 Constants
// ============================================================================

// Boot sector constants
constexpr uint16_t BOOT_SIGNATURE = 0xAA55;
constexpr uint8_t BOOT_MEDIA_DESCRIPTOR = 0xF8;  // Fixed disk

// FAT32 special cluster values
constexpr uint32_t FAT32_FREE = 0x00000000;
constexpr uint32_t FAT32_RESERVED_START = 0x0FFFFFF0;
constexpr uint32_t FAT32_RESERVED_END = 0x0FFFFFF6;
constexpr uint32_t FAT32_BAD_CLUSTER = 0x0FFFFFF7;
constexpr uint32_t FAT32_EOC_START = 0x0FFFFFF8;
constexpr uint32_t FAT32_EOC = 0x0FFFFFFF;
constexpr uint32_t FAT32_EOC_MASK = 0x0FFFFFFF;

// FSInfo sector signatures
constexpr uint32_t FSINFO_LEAD_SIGNATURE = 0x41615252;   // "RRaA"
constexpr uint32_t FSINFO_STRUC_SIGNATURE = 0x61417272;  // "rrAa"
constexpr uint32_t FSINFO_TRAIL_SIGNATURE = 0xAA550000;

// Directory entry attributes
constexpr uint8_t ATTR_READ_ONLY = 0x01;
constexpr uint8_t ATTR_HIDDEN = 0x02;
constexpr uint8_t ATTR_SYSTEM = 0x04;
constexpr uint8_t ATTR_VOLUME_ID = 0x08;
constexpr uint8_t ATTR_DIRECTORY = 0x10;
constexpr uint8_t ATTR_ARCHIVE = 0x20;
constexpr uint8_t ATTR_LONG_NAME = 0x0F;
constexpr uint8_t ATTR_LONG_NAME_MASK = 0x3F;

// Directory entry constants
constexpr uint8_t DENTRY_DELETED = 0xE5;
constexpr uint8_t DENTRY_UNUSED = 0x00;
constexpr uint8_t DENTRY_LFN_LAST = 0x40;

// FAT32 limits
constexpr uint32_t FAT32_MAX_CLUSTERS = 0x0FFFFFF6;
constexpr uint32_t FAT32_MIN_CLUSTERS = 65525;  // Minimum for FAT32
constexpr uint32_t ROOT_DIR_CLUSTER = 2;

// Cluster size selection (sectors per cluster)
// Based on volume size according to Microsoft FAT32 spec:
//   <260MB -> 512B clusters (1 sector)
//   260MB-8GB -> 4KB (8 sectors)
//   8-16GB -> 8KB (16 sectors)
//   16-32GB -> 16KB (32 sectors)
//   >32GB -> 32KB (64 sectors)
constexpr uint32_t getSectorsPerCluster(uint64_t volume_size_bytes, uint32_t sector_size) {
    uint64_t mb = volume_size_bytes / (1024 * 1024);
    if (mb < 260) {
        return 1;    // 512B clusters
    } else if (mb < 8192) {
        return 8;    // 4KB clusters
    } else if (mb < 16384) {
        return 16;   // 8KB clusters
    } else if (mb < 32768) {
        return 32;   // 16KB clusters
    } else {
        return 64;   // 32KB clusters
    }
}

// ============================================================================
// FAT32 Boot Sector (BPB - BIOS Parameter Block)
// Total size: 512 bytes
// ============================================================================
struct __attribute__((packed)) FAT32BootSector {
    // Jump instruction (3 bytes)
    uint8_t bs_jmp_boot[3];
    
    // OEM name (8 bytes)
    char bs_oem_name[8];
    
    // BPB (BIOS Parameter Block) - 25 bytes
    uint16_t bpb_bytes_per_sector;      // Always 512, 1024, 2048, or 4096
    uint8_t bpb_sectors_per_cluster;     // Must be power of 2
    uint16_t bpb_reserved_sectors;       // Usually 32 for FAT32
    uint8_t bpb_num_fats;                // Usually 2
    uint16_t bpb_root_entries;           // 0 for FAT32
    uint16_t bpb_total_sectors_16;       // 0 for FAT32 (use total_sectors_32)
    uint8_t bpb_media_descriptor;        // 0xF8 for fixed, 0xF0 for removable
    uint16_t bpb_sectors_per_fat_16;     // 0 for FAT32
    uint16_t bpb_sectors_per_track;      // Geometry
    uint16_t bpb_num_heads;              // Geometry
    uint32_t bpb_hidden_sectors;         // Partition start
    uint32_t bpb_total_sectors_32;       // Total sectors
    
    // FAT32 specific - 28 bytes
    uint32_t bpb_sectors_per_fat_32;     // Sectors per FAT
    uint16_t bpb_ext_flags;              // Flags
    uint16_t bpb_fs_version;             // 0x0000
    uint32_t bpb_root_cluster;           // Root directory cluster (usually 2)
    uint16_t bpb_fs_info_sector;         // FSInfo sector number (usually 1)
    uint16_t bpb_backup_boot_sector;     // Backup boot sector (usually 6)
    uint8_t bpb_reserved[12];            // Reserved, set to 0
    
    // Extended BPB - 26 bytes
    uint8_t bs_drive_number;             // 0x80 for fixed, 0x00 for removable
    uint8_t bs_reserved1;                // 0
    uint8_t bs_ext_boot_signature;       // 0x29 (extended BPB signature)
    uint32_t bs_volume_serial;           // Volume serial number
    char bs_volume_label[11];            // Volume label (padded with spaces)
    char bs_file_system_type[8];         // "FAT32   "
    
    // Boot code (420 bytes)
    uint8_t bs_boot_code[420];
    
    // Boot sector signature (2 bytes)
    uint16_t bs_boot_signature;          // 0xAA55
    
    // Helper to initialize with defaults
    void init(uint32_t total_sectors, uint32_t sectors_per_fat,
              uint32_t sectors_per_cluster, uint32_t serial_number,
              const char* label = nullptr);
};

// ============================================================================
// FAT32 FSInfo Sector
// Total size: 512 bytes
// ============================================================================
struct __attribute__((packed)) FAT32FSInfo {
    // Lead signature (4 bytes)
    uint32_t fsi_lead_signature;         // 0x41615252 ("RRaA")
    
    // Reserved (480 bytes)
    uint8_t fsi_reserved1[480];
    
    // Structure signature (4 bytes)
    uint32_t fsi_struc_signature;        // 0x61417272 ("rrAa")
    
    // Free count (4 bytes) - 0xFFFFFFFF if unknown
    uint32_t fsi_free_count;             // Last known free cluster count
    
    // Next free (4 bytes) - hint for next free cluster
    uint32_t fsi_next_free;              // Hint for next free cluster
    
    // Reserved (12 bytes)
    uint8_t fsi_reserved2[12];
    
    // Trail signature (4 bytes)
    uint32_t fsi_trail_signature;        // 0xAA550000
    
    // Helper to initialize
    void init(uint32_t free_clusters, uint32_t next_free_hint = 0xFFFFFFFF);
};

// ============================================================================
// FAT32 Directory Entry (32 bytes)
// ============================================================================
struct __attribute__((packed)) FAT32DirEntry {
    // Short file name (11 bytes)
    char dir_name[11];
    
    // Attributes (1 byte)
    uint8_t dir_attr;
    
    // Reserved for Windows NT (1 byte)
    uint8_t dir_nt_reserved;
    
    // Creation time tenths of second (1 byte)
    uint8_t dir_crt_time_tenth;
    
    // Creation time (2 bytes)
    uint16_t dir_crt_time;
    
    // Creation date (2 bytes)
    uint16_t dir_crt_date;
    
    // Last access date (2 bytes)
    uint16_t dir_lst_acc_date;
    
    // High 16 bits of first cluster (2 bytes)
    uint16_t dir_fst_clus_hi;
    
    // Last write time (2 bytes)
    uint16_t dir_wrt_time;
    
    // Last write date (2 bytes)
    uint16_t dir_wrt_date;
    
    // Low 16 bits of first cluster (2 bytes)
    uint16_t dir_fst_clus_lo;
    
    // File size (4 bytes)
    uint32_t dir_file_size;
    
    // Helper to check if entry is deleted
    bool isDeleted() const { return static_cast<uint8_t>(dir_name[0]) == DENTRY_DELETED; }
    
    // Helper to check if entry is unused
    bool isUnused() const { return dir_name[0] == DENTRY_UNUSED; }
    
    // Helper to check if this is a long file name entry
    bool isLFN() const { return dir_attr == ATTR_LONG_NAME; }
    
    // Helper to check if this is a volume label
    bool isVolumeLabel() const { return (dir_attr & ATTR_VOLUME_ID) && !(dir_attr & ATTR_DIRECTORY); }
    
    // Get full cluster number
    uint32_t getCluster() const { return (static_cast<uint32_t>(dir_fst_clus_hi) << 16) | dir_fst_clus_lo; }
    
    // Set cluster number
    void setCluster(uint32_t cluster) {
        dir_fst_clus_hi = static_cast<uint16_t>(cluster >> 16);
        dir_fst_clus_lo = static_cast<uint16_t>(cluster & 0xFFFF);
    }
    
    // Initialize volume label entry
    void initVolumeLabel(const char* label);
    
    // Clear entry
    void clear() { std::memset(this, 0, sizeof(*this)); }
};

// ============================================================================
// Long File Name (LFN) Directory Entry (32 bytes)
// ============================================================================
struct __attribute__((packed)) FAT32LFNEntry {
    // Sequence number (1 byte) - 0x40 | sequence for last entry
    uint8_t lfn_ord;
    
    // Characters 1-5 (10 bytes)
    uint16_t lfn_name1[5];
    
    // Attributes - always ATTR_LONG_NAME (1 byte)
    uint8_t lfn_attr;
    
    // Type - always 0 (1 byte)
    uint8_t lfn_type;
    
    // Checksum of short name (1 byte)
    uint8_t lfn_chk_sum;
    
    // Characters 6-11 (12 bytes)
    uint16_t lfn_name2[6];
    
    // Always 0 (2 bytes)
    uint16_t lfn_fst_clus_lo;
    
    // Characters 12-13 (4 bytes)
    uint16_t lfn_name3[2];
    
    // Calculate checksum of short name
    static uint8_t calculateChecksum(const char* short_name);
};

// ============================================================================
// FAT32 Layout Calculator
// ============================================================================
class FAT32Layout {
public:
    uint64_t total_sectors;
    uint32_t sectors_per_cluster;
    uint32_t bytes_per_sector;
    uint32_t reserved_sectors;
    uint32_t num_fats;
    uint32_t sectors_per_fat;
    uint32_t root_cluster;
    uint32_t fs_info_sector;
    uint32_t backup_boot_sector;
    uint32_t total_clusters;
    uint32_t data_start_sector;
    uint32_t fat_start_sector;
    uint64_t total_size;  // Total size in bytes
    
    // Calculate layout from volume size
    void calculate(uint64_t volume_size_bytes, uint32_t sector_size = 512);
    
    // Calculate sector from cluster number
    uint32_t clusterToSector(uint32_t cluster) const {
        return data_start_sector + (cluster - 2) * sectors_per_cluster;
    }
    
    // Calculate cluster from sector
    uint32_t sectorToCluster(uint32_t sector) const {
        if (sector < data_start_sector) return 0;
        return ((sector - data_start_sector) / sectors_per_cluster) + 2;
    }
    
    // Validate layout
    bool validate() const;
};

// ============================================================================
// FAT32 Operations
// ============================================================================

// Initialize boot sector
void initBootSector(FAT32BootSector& bs, const FAT32Layout& layout,
                    uint32_t serial, const char* label);

// Initialize FSInfo sector
void initFSInfoSector(FAT32FSInfo& fs_info, const FAT32Layout& layout);

// Initialize FAT table
void initFATTable(std::vector<uint32_t>& fat);

// Set FAT entry
void setFATEntry(std::vector<uint32_t>& fat, uint32_t cluster, uint32_t value);

// Get FAT entry
uint32_t getFATEntry(const std::vector<uint32_t>& fat, uint32_t cluster);

// Calculate FAT32 sector checksum (for backup)
uint16_t calculateBootSectorChecksum(const FAT32BootSector& bs);

} // namespace fat32

// Forward declarations for disk operations
class DiskIO;

namespace fat32 {

// Set the volume label in the FAT32 root directory (updates the volume-label
// entry, creating it in a free slot if absent).
Result setLabel(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                const std::string& label);

// Bring DiskIO into fat32 namespace
using ::opm::DiskIO;

// Format FAT32 filesystem
Result formatFAT32(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                   uint64_t size_bytes, const std::string& label);

// Write boot sector
Result writeFAT32BootSector(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                             const FAT32Layout& layout, uint32_t serial,
                             const std::string& label);

// Verify boot sector
Result verifyFAT32BootSector(std::shared_ptr<DiskIO> disk, uint64_t start_sector);

// Get FAT32 info
Result getFAT32Info(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                    FAT32BootSector& boot_sector, FAT32Layout& layout);

// Generate serial number
uint32_t generateFAT32Serial();

// Create FAT tables
Result createFATTables(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                       const FAT32Layout& layout);

// Write FAT table
Result writeFATTable(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                     const FAT32Layout& layout, uint32_t fat_num,
                     const std::vector<uint32_t>& fat);

// Read FAT table
Result readFATTable(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                    const FAT32Layout& layout, uint32_t fat_num,
                    std::vector<uint32_t>& fat);

// Get next cluster
uint32_t getNextCluster(const std::vector<uint32_t>& fat, uint32_t cluster);

// Allocate cluster
Result allocateCluster(std::vector<uint32_t>& fat, uint32_t& allocated_cluster);

// Free cluster chain
Result freeClusterChain(std::vector<uint32_t>& fat, uint32_t start_cluster);

// Verify FAT tables match
Result verifyFATTables(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                       const FAT32Layout& layout);

// Get free cluster count
uint32_t getFreeClusterCount(const std::vector<uint32_t>& fat);

// Extend cluster chain
Result extendChain(std::vector<uint32_t>& fat, uint32_t end_cluster, 
                   uint32_t new_cluster);

// ============================================================================
// FAT32 FSInfo Operations
// ============================================================================

Result createFSInfoSector(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                          const FAT32Layout& layout, uint32_t free_clusters);

Result updateFSInfo(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                    const FAT32Layout& layout, uint32_t free_clusters, 
                    uint32_t next_free_hint);

Result readFSInfo(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                  const FAT32Layout& layout, FAT32FSInfo& fs_info);

// ============================================================================
// FAT32 Directory Operations
// ============================================================================

Result createRootDirectory(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                          const FAT32Layout& layout, const std::string& label);

Result createDirectoryEntry(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                              const FAT32Layout& layout, uint32_t parent_cluster,
                              const std::string& name, uint8_t attr,
                              uint32_t cluster, uint32_t size);

std::string createShortName(const std::string& long_name);

// ============================================================================
// Complete FAT32 Format
// ============================================================================

Result formatFAT32Complete(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                           uint64_t size_bytes, const std::string& label);

// ============================================================================
// FSInfo initialization with free cluster count
// ============================================================================

void initFSInfoSector(FAT32FSInfo& fs_info, const FAT32Layout& layout, 
                      uint32_t free_clusters);

// ============================================================================
// FAT32 Check Operations
// ============================================================================

Result checkFAT32(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                    bool repair = false, std::vector<std::string>* errors = nullptr);

Result checkBootSector(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                       bool repair, std::vector<std::string>* errors);

Result checkFSInfo(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                   const FAT32Layout& layout, bool repair,
                   std::vector<std::string>* errors);

Result checkFATTables(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                        const FAT32Layout& layout, bool repair,
                        std::vector<std::string>* errors);

Result checkRootDirectory(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                          const FAT32Layout& layout, bool repair,
                          std::vector<std::string>* errors);

Result checkClusterChain(const std::vector<uint32_t>& fat, uint32_t start_cluster,
                         std::vector<std::string>* errors);

Result getActualFreeClusterCount(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                                  const FAT32Layout& layout, uint32_t& free_count);

Result repairFAT32(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                   std::vector<std::string>* fixes);

// ============================================================================
// FAT32 Resize Operations
// ============================================================================

Result resizeFAT32(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                     uint64_t new_size_bytes);

Result extendFATTables(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                     const FAT32Layout& old_layout, const FAT32Layout& new_layout);

Result updateBootSectorForResize(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                                   const FAT32Layout& layout);

} // namespace fat32
} // namespace opm
