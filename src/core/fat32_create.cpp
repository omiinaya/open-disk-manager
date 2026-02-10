#include "opm/fat32_impl.hpp"
#include "opm/disk_io.hpp"
#include "opm/utils.hpp"
#include <cstring>
#include <random>

namespace opm {
namespace fat32 {

// ============================================================================
// FAT32 Format - Phase 3.2.2: Boot Sector Creation
// ============================================================================

Result formatFAT32(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                   uint64_t size_bytes, const std::string& label) {
    
    // Step 1: Calculate filesystem layout
    FAT32Layout layout;
    layout.calculate(size_bytes);
    
    if (!layout.validate()) {
        return Result::error("Invalid FAT32 layout calculated");
    }
    
    // Step 2: Generate serial number (random)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis;
    uint32_t serial = dis(gen);
    
    // Step 3: Create boot sector
    FAT32BootSector boot_sector;
    initBootSector(boot_sector, layout, serial, label.c_str());
    
    // Step 4: Write boot sector (sector 0)
    Result result = disk->writeSector(&boot_sector, start_sector);
    if (result.failed()) {
        return Result::error("Failed to write boot sector: " + result.message);
    }
    
    // Step 5: Write backup boot sector (sector 6)
    result = disk->writeSector(&boot_sector, start_sector + layout.backup_boot_sector);
    if (result.failed()) {
        return Result::error("Failed to write backup boot sector: " + result.message);
    }
    
    return Result::ok();
}

// ============================================================================
// Write FAT32 Boot Sector
// ============================================================================

Result writeFAT32BootSector(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                             const FAT32Layout& layout, uint32_t serial,
                             const std::string& label) {
    
    // Create boot sector
    FAT32BootSector boot_sector;
    initBootSector(boot_sector, layout, serial, label.c_str());
    
    // Write to sector 0
    Result result = disk->writeSector(&boot_sector, start_sector);
    if (result.failed()) {
        return Result::error("Failed to write boot sector");
    }
    
    // Write backup to sector 6
    result = disk->writeSector(&boot_sector, start_sector + layout.backup_boot_sector);
    if (result.failed()) {
        return Result::error("Failed to write backup boot sector");
    }
    
    return Result::ok();
}

// ============================================================================
// Calculate FAT32 Serial Number
// ============================================================================

uint32_t generateFAT32Serial() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis;
    return dis(gen);
}

// ============================================================================
// Verify FAT32 Boot Sector
// ============================================================================

Result verifyFAT32BootSector(std::shared_ptr<DiskIO> disk, uint64_t start_sector) {
    FAT32BootSector boot_sector;
    
    // Read boot sector
    Result result = disk->readSector(&boot_sector, start_sector);
    if (result.failed()) {
        return Result::error("Failed to read boot sector");
    }
    
    // Verify signature at end of sector
    if (boot_sector.bs_boot_signature != BOOT_SIGNATURE) {
        return Result::error("Invalid boot sector signature");
    }
    
    // Verify FAT32 identifier
    if (std::memcmp(boot_sector.bs_file_system_type, "FAT32   ", 8) != 0) {
        return Result::error("Not a FAT32 filesystem");
    }
    
    // Verify jump instruction
    if (boot_sector.bs_jmp_boot[0] != 0xEB || 
        boot_sector.bs_jmp_boot[1] != 0x58 ||
        boot_sector.bs_jmp_boot[2] != 0x90) {
        return Result::error("Invalid jump instruction");
    }
    
    return Result::ok();
}

// ============================================================================
// FAT32 Info
// ============================================================================

Result getFAT32Info(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                    FAT32BootSector& boot_sector, FAT32Layout& layout) {
    
    // Read boot sector
    Result result = disk->readSector(&boot_sector, start_sector);
    if (result.failed()) {
        return Result::error("Failed to read boot sector");
    }
    
    // Populate layout from boot sector
    layout.bytes_per_sector = boot_sector.bpb_bytes_per_sector;
    layout.sectors_per_cluster = boot_sector.bpb_sectors_per_cluster;
    layout.reserved_sectors = boot_sector.bpb_reserved_sectors;
    layout.num_fats = boot_sector.bpb_num_fats;
    layout.sectors_per_fat = boot_sector.bpb_sectors_per_fat_32;
    layout.root_cluster = boot_sector.bpb_root_cluster;
    layout.fs_info_sector = boot_sector.bpb_fs_info_sector;
    layout.backup_boot_sector = boot_sector.bpb_backup_boot_sector;
    layout.total_sectors = boot_sector.bpb_total_sectors_32;
    
    // Calculate derived values
    layout.fat_start_sector = layout.reserved_sectors;
    layout.data_start_sector = layout.reserved_sectors + 
                                (layout.num_fats * layout.sectors_per_fat);
    
    uint64_t data_sectors = layout.total_sectors - layout.data_start_sector;
    layout.total_clusters = static_cast<uint32_t>(data_sectors / layout.sectors_per_cluster);
    
    return Result::ok();
}

} // namespace fat32
} // namespace opm
