#include "opm/fat32_impl.hpp"
#include "opm/disk_io.hpp"
#include <cstring>
#include <vector>

namespace opm {
namespace fat32 {

// ============================================================================
// FAT32 Resize - Phase 3.2.8
// ============================================================================

Result resizeFAT32(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                     uint64_t new_size_bytes) {
    
    // Step 1: Read current boot sector
    FAT32BootSector boot_sector;
    Result result = disk->readSector(&boot_sector, start_sector);
    if (result.failed()) {
        return Result::error("Failed to read boot sector: " + result.message);
    }
    
    // Step 2: Get current layout
    FAT32Layout old_layout;
    old_layout.calculate(static_cast<uint64_t>(boot_sector.bpb_total_sectors_32) * 
                          boot_sector.bpb_bytes_per_sector);
    
    // Step 3: Calculate new layout
    FAT32Layout new_layout;
    new_layout.calculate(new_size_bytes);
    
    // Step 4: Validate resize
    if (new_size_bytes <= old_layout.total_size) {
        return Result::error("FAT32 shrink not supported");
    }
    
    // Step 5: Extend FAT tables
    result = extendFATTables(disk, start_sector, old_layout, new_layout);
    if (result.failed()) {
        return result;
    }
    
    // Step 6: Update boot sector
    result = updateBootSectorForResize(disk, start_sector, new_layout);
    if (result.failed()) {
        return result;
    }
    
    // Step 7: Update FSInfo
    uint32_t free_clusters = new_layout.total_clusters - old_layout.total_clusters;
    result = updateFSInfo(disk, start_sector, new_layout, 
                           old_layout.total_clusters + free_clusters, 3);
    if (result.failed()) {
        return result;
    }
    
    // Step 8: Flush
    return disk->flush();
}

// ============================================================================
// Extend FAT Tables
// ============================================================================

Result extendFATTables(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                       const FAT32Layout& old_layout, const FAT32Layout& new_layout) {
    
    // Read current FAT
    std::vector<uint32_t> fat;
    Result result = readFATTable(disk, start_sector, old_layout, 0, fat);
    if (result.failed()) {
        return result;
    }
    
    // Resize FAT to new size
    uint32_t new_entries = new_layout.total_clusters + 2;
    fat.resize(new_entries, FAT32_FREE);
    
    // Write extended FAT
    result = writeFATTable(disk, start_sector, new_layout, 0, fat);
    if (result.failed()) {
        return result;
    }
    
    result = writeFATTable(disk, start_sector, new_layout, 1, fat);
    if (result.failed()) {
        return result;
    }
    
    return Result::ok();
}

// ============================================================================
// Update Boot Sector for Resize
// ============================================================================

Result updateBootSectorForResize(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                                   const FAT32Layout& layout) {
    
    FAT32BootSector boot_sector;
    Result result = disk->readSector(&boot_sector, start_sector);
    if (result.failed()) {
        return result;
    }
    
    // Update total sectors
    boot_sector.bpb_total_sectors_32 = layout.total_sectors;
    
    // Update sectors per FAT
    boot_sector.bpb_sectors_per_fat_32 = layout.sectors_per_fat;
    
    // Write updated boot sector
    result = disk->writeSector(&boot_sector, start_sector);
    if (result.failed()) {
        return result;
    }
    
    // Write backup
    result = disk->writeSector(&boot_sector, start_sector + layout.backup_boot_sector);
    
    return result;
}

} // namespace fat32
} // namespace opm
