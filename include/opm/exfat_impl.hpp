#pragma once

#include "types.hpp"
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>
#include <string>

namespace opm {

class DiskIO;

namespace exfat {

using ::opm::DiskIO;

// ============================================================================
// exFAT Constants
// ============================================================================

// exFAT magic numbers
constexpr uint8_t EXFAT_BOOT_SIGNATURE = 0xEB;
constexpr uint8_t EXFAT_BOOT_SIGNATURE_2 = 0x76;
constexpr uint8_t EXFAT_BOOT_SIGNATURE_3 = 0x90;
constexpr char EXFAT_FILE_SYSTEM_NAME[8] = {'E', 'X', 'F', 'A', 'T', ' ', ' ', ' '};
constexpr uint16_t EXFAT_BOOT_SECTOR_CHECKSUM = 0xAA55;
constexpr uint32_t EXFAT_VOLUME_LENGTH_MIN = 0x100000; // 32MB minimum (in sectors)
constexpr uint32_t EXFAT_VOLUME_LENGTH_MAX = 0xFFFFFFFF; // 128TB maximum (in sectors)

// Cluster constants
constexpr uint32_t EXFAT_FIRST_DATA_CLUSTER = 2;
constexpr uint32_t EXFAT_CLUSTER_FREE = 0x00000000;
constexpr uint32_t EXFAT_CLUSTER_ALLOCATED_MIN = 0x00000002;
constexpr uint32_t EXFAT_CLUSTER_ALLOCATED_MAX = 0xFFFFFFF6;
constexpr uint32_t EXFAT_CLUSTER_BAD = 0xFFFFFFF7;
constexpr uint32_t EXFAT_CLUSTER_END = 0xFFFFFFFF;

// Directory entry types
constexpr uint8_t EXFAT_ENTRY_END = 0x00;
constexpr uint8_t EXFAT_ENTRY_ALLOCATION_BITMAP = 0x81;
constexpr uint8_t EXFAT_ENTRY_UPCASE_TABLE = 0x82;
constexpr uint8_t EXFAT_ENTRY_VOLUME_LABEL = 0x83;
constexpr uint8_t EXFAT_ENTRY_FILE = 0x85;
constexpr uint8_t EXFAT_ENTRY_STREAM_EXTENSION = 0xC0;
constexpr uint8_t EXFAT_ENTRY_FILE_NAME = 0xC1;

// File attributes
constexpr uint16_t EXFAT_ATTR_READ_ONLY = 0x0001;
constexpr uint16_t EXFAT_ATTR_HIDDEN = 0x0002;
constexpr uint16_t EXFAT_ATTR_SYSTEM = 0x0004;
constexpr uint16_t EXFAT_ATTR_VOLUME = 0x0008;
constexpr uint16_t EXFAT_ATTR_DIRECTORY = 0x0010;
constexpr uint16_t EXFAT_ATTR_ARCHIVE = 0x0020;

// ============================================================================
// exFAT Boot Sector (BPB)
// ============================================================================
struct __attribute__((packed)) ExFATBootSector {
    // Jump instruction (3 bytes)
    uint8_t bs_jmp[3];
    
    // File system name (8 bytes)
    char bs_file_system_name[8];
    
    // Must be zero (53 bytes)
    uint8_t bs_must_be_zero[53];
    
    // Partition offset (relative to beginning of volume)
    uint64_t bs_partition_offset;
    
    // Volume length (in sectors)
    uint64_t bs_volume_length;
    
    // FAT offset (relative to beginning of volume, in sectors)
    uint32_t bs_fat_offset;
    
    // FAT length (in sectors)
    uint32_t bs_fat_length;
    
    // Cluster heap offset (relative to beginning of volume, in sectors)
    uint32_t bs_cluster_heap_offset;
    
    // Cluster count
    uint32_t bs_cluster_count;
    
    // First cluster of root directory
    uint32_t bs_first_cluster_of_root;
    
    // Volume serial number
    uint32_t bs_volume_serial_number;
    
    // File system revision
    uint16_t bs_file_system_revision;
    
    // Volume flags
    uint16_t bs_volume_flags;
    
    // Bytes per sector shift (2^N = bytes per sector)
    uint8_t bs_bytes_per_sector_shift;
    
    // Sectors per cluster shift (2^N = sectors per cluster)
    uint8_t bs_sectors_per_cluster_shift;
    
    // Number of FATs
    uint8_t bs_number_of_fats;
    
    // Drive select (0x80 for first hard disk)
    uint8_t bs_drive_select;
    
    // Percent in use (0-100, 0xFF = not available)
    uint8_t bs_percent_in_use;
    
    // Reserved (7 bytes)
    uint8_t bs_reserved[7];
    
    // Boot code (390 bytes)
    uint8_t bs_boot_code[390];
    
    // Boot signature
    uint16_t bs_boot_signature;
    
    void init(uint64_t volume_length, uint8_t bytes_per_sector_shift,
              uint8_t sectors_per_cluster_shift, uint32_t fat_offset,
              uint32_t fat_length, uint32_t cluster_heap_offset,
              uint32_t cluster_count, uint32_t first_cluster_of_root,
              uint32_t volume_serial);
    
    bool isValid() const;
};

// ============================================================================
// Extended Boot Sectors (Checksum sector)
// ============================================================================
struct __attribute__((packed)) ExFATExtendedBootSector {
    // Extended boot code (508 bytes)
    uint8_t extended_boot_code[508];
    
    // Extended boot signature
    uint32_t extended_boot_signature;
    
    void init();
};

// ============================================================================
// OEM Parameters
// ============================================================================
struct __attribute__((packed)) ExFATOemParameters {
    // OEM defined parameters (512 bytes)
    uint8_t parameters[512];
    
    void init();
};

// ============================================================================
// Allocation Bitmap Entry
// ============================================================================
struct __attribute__((packed)) ExFATAllocationBitmapEntry {
    uint8_t entry_type;              // 0x81
    uint8_t bitmap_flags;
    uint8_t reserved[18];
    uint32_t first_cluster;
    uint64_t data_length;
};

// ============================================================================
// Up-Case Table Entry
// ============================================================================
struct __attribute__((packed)) ExFATUpcaseTableEntry {
    uint8_t entry_type;              // 0x82
    uint8_t reserved1[3];
    uint32_t checksum;
    uint8_t reserved2[12];
    uint32_t first_cluster;
    uint64_t data_length;
};

// ============================================================================
// Volume Label Entry
// ============================================================================
struct __attribute__((packed)) ExFATVolumeLabelEntry {
    uint8_t entry_type;              // 0x83
    uint8_t character_count;
    char16_t volume_label[11];
    uint8_t reserved[8];
};

// ============================================================================
// File Directory Entry
// ============================================================================
struct __attribute__((packed)) ExFATFileEntry {
    uint8_t entry_type;              // 0x85
    uint8_t secondary_count;
    uint16_t checksum;
    uint16_t file_attributes;
    uint8_t reserved1[2];
    uint32_t create_timestamp;
    uint32_t modify_timestamp;
    uint32_t access_timestamp;
    uint8_t reserved2[2];
};

// ============================================================================
// Stream Extension Entry
// ============================================================================
struct __attribute__((packed)) ExFATStreamExtensionEntry {
    uint8_t entry_type;              // 0xC0
    uint8_t flags;
    uint8_t reserved1;
    uint8_t name_length;
    uint16_t name_hash;
    uint8_t reserved2[2];
    uint64_t valid_data_length;
    uint32_t reserved3;
    uint32_t first_cluster;
    uint64_t data_length;
};

// ============================================================================
// File Name Entry
// ============================================================================
struct __attribute__((packed)) ExFATFileNameEntry {
    uint8_t entry_type;              // 0xC1
    uint8_t flags;
    char16_t file_name[15];
};

// ============================================================================
// exFAT Layout Calculator
// ============================================================================
class ExFATLayout {
public:
    uint64_t volume_length;          // Volume length in sectors
    uint8_t bytes_per_sector_shift;    // 2^N = bytes per sector
    uint8_t sectors_per_cluster_shift; // 2^N = sectors per cluster
    uint32_t bytes_per_sector;
    uint32_t sectors_per_cluster;
    uint32_t bytes_per_cluster;
    uint32_t fat_offset;             // FAT offset in sectors
    uint32_t fat_length;             // FAT length in sectors
    uint32_t cluster_heap_offset;    // Cluster heap offset in sectors
    uint32_t cluster_count;          // Total clusters
    uint32_t root_cluster;           // First cluster of root directory
    uint32_t serial_number;            // Volume serial
    
    void calculate(uint64_t volume_size_bytes);
    bool validate() const;
    
    uint64_t clusterToSector(uint32_t cluster) const {
        return cluster_heap_offset + (cluster - EXFAT_FIRST_DATA_CLUSTER) * sectors_per_cluster;
    }
    
    uint32_t sectorToCluster(uint64_t sector) const {
        if (sector < cluster_heap_offset) return 0;
        return EXFAT_FIRST_DATA_CLUSTER + (sector - cluster_heap_offset) / sectors_per_cluster;
    }
};

// ============================================================================
// exFAT Operations
// ============================================================================

// Generate serial number
uint32_t generateExFATSerial();

// Calculate checksum
uint32_t calculateExFATBootChecksum(const uint8_t* data, size_t length);
uint16_t calculateExFATNameChecksum(const char16_t* name, uint8_t name_length);

// Initialize boot sector
void initExFATBootSector(ExFATBootSector& bs, const ExFATLayout& layout);

// ============================================================================
// exFAT Format Operations
// ============================================================================

Result formatExFAT(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                   uint64_t size_bytes, const std::string& label);

Result createExFATBootSector(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                            const ExFATLayout& layout);

Result createExFATFAT(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                      const ExFATLayout& layout);

Result createExFATAllocationBitmap(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                                   const ExFATLayout& layout);

Result createExFATUpcaseTable(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                              const ExFATLayout& layout);

Result createExFATRootDirectory(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                                const ExFATLayout& layout, const std::string& label);

// ============================================================================
// exFAT Check Operations
// ============================================================================

Result checkExFAT(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                  bool repair = false, std::vector<std::string>* errors = nullptr);

Result checkExFATBootSector(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                            bool repair, std::vector<std::string>* errors);

Result checkExFATFAT(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                     const ExFATLayout& layout, bool repair,
                     std::vector<std::string>* errors);

Result checkExFATAllocationBitmap(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                                  const ExFATLayout& layout, bool repair,
                                  std::vector<std::string>* errors);

Result checkExFATRootDirectory(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                                const ExFATLayout& layout, bool repair,
                                std::vector<std::string>* errors);

// ============================================================================
// exFAT Resize Operations
// ============================================================================

Result resizeExFAT(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                   uint64_t new_size_bytes);

Result updateExFATBootSectorForResize(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                                      const ExFATLayout& layout);

Result extendExFATAllocationBitmap(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                                   const ExFATLayout& old_layout, const ExFATLayout& new_layout);

// Set the exFAT volume label (root-directory volume-label entry 0x83).
Result setLabel(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                const std::string& label);

} // namespace exfat
} // namespace opm
