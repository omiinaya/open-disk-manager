#include "opm/exfat_impl.hpp"
#include <cstring>
#include <ctime>
#include <cstdlib>

namespace opm {
namespace exfat {

// ============================================================================
// ExFATBootSector Implementation
// ============================================================================

void ExFATBootSector::init(uint64_t volume_length_val, uint8_t bytes_per_sector_shift_val,
                          uint8_t sectors_per_cluster_shift_val, uint32_t fat_offset_val,
                          uint32_t fat_length_val, uint32_t cluster_heap_offset_val,
                          uint32_t cluster_count_val, uint32_t first_cluster_of_root_val,
                          uint32_t volume_serial_val) {
    // Clear boot sector
    std::memset(this, 0, sizeof(ExFATBootSector));
    
    // Jump instruction
    bs_jmp[0] = EXFAT_BOOT_SIGNATURE;
    bs_jmp[1] = EXFAT_BOOT_SIGNATURE_2;
    bs_jmp[2] = EXFAT_BOOT_SIGNATURE_3;
    
    // File system name
    std::memcpy(bs_file_system_name, EXFAT_FILE_SYSTEM_NAME, 8);
    
    // Must be zero (already zeroed)
    
    // Partition offset
    bs_partition_offset = 0;
    
    // Volume length
    bs_volume_length = volume_length_val;
    
    // FAT offset
    bs_fat_offset = fat_offset_val;
    
    // FAT length
    bs_fat_length = fat_length_val;
    
    // Cluster heap offset
    bs_cluster_heap_offset = cluster_heap_offset_val;
    
    // Cluster count
    bs_cluster_count = cluster_count_val;
    
    // First cluster of root directory
    bs_first_cluster_of_root = first_cluster_of_root_val;
    
    // Volume serial number
    bs_volume_serial_number = volume_serial_val;
    
    // File system revision (1.0)
    bs_file_system_revision = 0x0100;
    
    // Volume flags
    bs_volume_flags = 0x0000;
    
    // Bytes per sector shift
    bs_bytes_per_sector_shift = bytes_per_sector_shift_val;
    
    // Sectors per cluster shift
    bs_sectors_per_cluster_shift = sectors_per_cluster_shift_val;
    
    // Number of FATs (1 for exFAT, 2 for TexFAT)
    bs_number_of_fats = 1;
    
    // Drive select
    bs_drive_select = 0x80;
    
    // Percent in use (0xFF = not available)
    bs_percent_in_use = 0xFF;
    
    // Reserved (already zeroed)
    
    // Boot code - simple infinite loop
    bs_boot_code[0] = 0xEB; // jmp short
    bs_boot_code[1] = 0xFE; // infinite loop
    bs_boot_code[2] = 0x90; // nop
    
    // Boot signature
    bs_boot_signature = EXFAT_BOOT_SECTOR_CHECKSUM;
}

bool ExFATBootSector::isValid() const {
    // Check jump instruction
    if (bs_jmp[0] != EXFAT_BOOT_SIGNATURE || bs_jmp[1] != EXFAT_BOOT_SIGNATURE_2) {
        return false;
    }
    
    // Check file system name
    if (std::memcmp(bs_file_system_name, EXFAT_FILE_SYSTEM_NAME, 8) != 0) {
        return false;
    }
    
    // Check boot signature
    if (bs_boot_signature != EXFAT_BOOT_SECTOR_CHECKSUM) {
        return false;
    }
    
    // Check volume length
    if (bs_volume_length < EXFAT_VOLUME_LENGTH_MIN || bs_volume_length > EXFAT_VOLUME_LENGTH_MAX) {
        return false;
    }
    
    // Check shifts
    if (bs_bytes_per_sector_shift < 9 || bs_bytes_per_sector_shift > 12) { // 512-4096 bytes
        return false;
    }
    
    if (bs_sectors_per_cluster_shift > 25 - bs_bytes_per_sector_shift) { // Max 32MB clusters
        return false;
    }
    
    return true;
}

// ============================================================================
// ExFATExtendedBootSector Implementation
// ============================================================================

void ExFATExtendedBootSector::init() {
    std::memset(this, 0, sizeof(ExFATExtendedBootSector));
    extended_boot_signature = 0xAA550000;
}

// ============================================================================
// ExFATOemParameters Implementation
// ============================================================================

void ExFATOemParameters::init() {
    std::memset(this, 0, sizeof(ExFATOemParameters));
}

// ============================================================================
// ExFATLayout Implementation
// ============================================================================

void ExFATLayout::calculate(uint64_t volume_size_bytes) {
    // Calculate bytes per sector (512 default)
    bytes_per_sector_shift = 9; // 2^9 = 512
    bytes_per_sector = 512;
    
    // Calculate sectors per cluster based on volume size
    // exFAT uses power-of-2 cluster sizes
    uint64_t total_sectors = volume_size_bytes / bytes_per_sector;
    
    if (total_sectors < 0x40000) { // < 128MB
        sectors_per_cluster_shift = 0; // 512 bytes
    } else if (total_sectors < 0x80000) { // < 256MB
        sectors_per_cluster_shift = 1; // 1KB
    } else if (total_sectors < 0x100000) { // < 512MB
        sectors_per_cluster_shift = 2; // 2KB
    } else if (total_sectors < 0x2000000) { // < 32GB
        sectors_per_cluster_shift = 3; // 4KB
    } else if (total_sectors < 0x4000000) { // < 64GB
        sectors_per_cluster_shift = 4; // 8KB
    } else if (total_sectors < 0x8000000) { // < 128GB
        sectors_per_cluster_shift = 5; // 16KB
    } else if (total_sectors < 0x10000000) { // < 256GB
        sectors_per_cluster_shift = 6; // 32KB
    } else {
        sectors_per_cluster_shift = 7; // 64KB
    }
    
    sectors_per_cluster = 1 << sectors_per_cluster_shift;
    bytes_per_cluster = bytes_per_sector * sectors_per_cluster;
    
    // Volume length in sectors
    volume_length = total_sectors;
    
    // FAT offset (after boot sector, extended boot sectors, OEM parameters, and checksum)
    // 24 sectors: 1 boot + 8 extended + 1 OEM + 1 checksum + 13 reserved
    fat_offset = 24;
    
    // Calculate FAT length
    // Need 4 bytes per cluster entry
    uint64_t fat_entries = total_sectors / sectors_per_cluster;
    fat_length = static_cast<uint32_t>((fat_entries * 4 + bytes_per_sector - 1) / bytes_per_sector);
    
    // Ensure minimum FAT size
    if (fat_length < 1) fat_length = 1;
    
    // Cluster heap offset (after FAT)
    cluster_heap_offset = fat_offset + fat_length;
    
    // Cluster count (not including clusters 0 and 1 which are reserved)
    uint64_t cluster_heap_sectors = total_sectors - cluster_heap_offset;
    cluster_count = static_cast<uint32_t>(cluster_heap_sectors / sectors_per_cluster);
    
    // Ensure minimum cluster count
    if (cluster_count < 2) cluster_count = 2;
    
    // First cluster of root directory (cluster 2)
    root_cluster = EXFAT_FIRST_DATA_CLUSTER;
    
    // Generate serial number
    serial_number = generateExFATSerial();
}

bool ExFATLayout::validate() const {
    if (volume_length < EXFAT_VOLUME_LENGTH_MIN) {
        return false;
    }
    
    if (bytes_per_sector_shift < 9 || bytes_per_sector_shift > 12) {
        return false;
    }
    
    if (sectors_per_cluster_shift > 25 - bytes_per_sector_shift) {
        return false;
    }
    
    if (fat_offset == 0 || fat_length == 0) {
        return false;
    }
    
    // Cluster heap must start at or after FAT (not before)
    if (cluster_heap_offset < fat_offset + fat_length) {
        return false;
    }
    
    if (cluster_count < 2) {
        return false;
    }
    
    return true;
}

// ============================================================================
// Utility Functions
// ============================================================================

uint32_t generateExFATSerial() {
    static bool seeded = false;
    if (!seeded) {
        std::srand(static_cast<unsigned int>(std::time(nullptr)));
        seeded = true;
    }
    return static_cast<uint32_t>(std::rand());
}

uint32_t calculateExFATBootChecksum(const uint8_t* data, size_t length) {
    uint32_t checksum = 0;
    for (size_t i = 0; i < length; i++) {
        // Skip bytes 106-107 (volume flags) and 112 (percent in use)
        if ((i >= 106 && i <= 107) || i == 112) {
            continue;
        }
        
        uint32_t byte = data[i];
        checksum = ((checksum << 31) | (checksum >> 1)) + byte;
    }
    return checksum;
}

uint16_t calculateExFATNameChecksum(const char16_t* name, uint8_t name_length) {
    uint16_t checksum = 0;
    for (uint8_t i = 0; i < name_length; i++) {
        checksum = ((checksum << 15) | (checksum >> 1)) + (name[i] & 0xFF);
        checksum = ((checksum << 15) | (checksum >> 1)) + (name[i] >> 8);
    }
    return checksum;
}

void initExFATBootSector(ExFATBootSector& bs, const ExFATLayout& layout) {
    bs.init(layout.volume_length, layout.bytes_per_sector_shift,
            layout.sectors_per_cluster_shift, layout.fat_offset,
            layout.fat_length, layout.cluster_heap_offset,
            layout.cluster_count, layout.root_cluster,
            layout.serial_number);
}

} // namespace exfat
} // namespace opm
