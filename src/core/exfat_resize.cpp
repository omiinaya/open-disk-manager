#include "opm/exfat_impl.hpp"
#include "opm/disk_io.hpp"
#include <cstring>

namespace opm {
namespace exfat {

Result resizeExFAT(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                   uint64_t new_size_bytes) {
    // Read current boot sector
    ExFATBootSector boot_sector;
    uint64_t boot_offset = start_sector * 512;
    Result result = disk->read(&boot_sector, boot_offset, sizeof(ExFATBootSector));
    if (result.failed()) {
        return Result::error("Failed to read boot sector: " + result.message);
    }
    
    // Validate
    if (!boot_sector.isValid()) {
        return Result::error("Invalid exFAT boot sector");
    }
    
    // Calculate old layout
    ExFATLayout old_layout;
    old_layout.bytes_per_sector_shift = boot_sector.bs_bytes_per_sector_shift;
    old_layout.bytes_per_sector = 1 << old_layout.bytes_per_sector_shift;
    old_layout.sectors_per_cluster_shift = boot_sector.bs_sectors_per_cluster_shift;
    old_layout.sectors_per_cluster = 1 << old_layout.sectors_per_cluster_shift;
    old_layout.bytes_per_cluster = old_layout.bytes_per_sector * old_layout.sectors_per_cluster;
    old_layout.volume_length = boot_sector.bs_volume_length;
    old_layout.fat_offset = boot_sector.bs_fat_offset;
    old_layout.fat_length = boot_sector.bs_fat_length;
    old_layout.cluster_heap_offset = boot_sector.bs_cluster_heap_offset;
    old_layout.cluster_count = boot_sector.bs_cluster_count;
    old_layout.root_cluster = boot_sector.bs_first_cluster_of_root;
    
    // Calculate new layout
    ExFATLayout new_layout;
    new_layout.calculate(new_size_bytes);
    
    if (!new_layout.validate()) {
        return Result::error("Invalid new volume size");
    }
    
    // Check if shrinking
    if (new_size_bytes < old_layout.volume_length * old_layout.bytes_per_sector) {
        // Would need to check if data would be truncated
        return Result::error("Shrinking not implemented yet");
    }
    
    // Update boot sector
    result = updateExFATBootSectorForResize(disk, start_sector, new_layout);
    if (result.failed()) {
        return Result::error("Failed to update boot sector: " + result.message);
    }
    
    // Extend allocation bitmap if needed
    if (new_size_bytes > old_layout.volume_length * old_layout.bytes_per_sector) {
        result = extendExFATAllocationBitmap(disk, start_sector, old_layout, new_layout);
        if (result.failed()) {
            return result;
        }
    }
    
    // Flush changes
    result = disk->flush();
    if (result.failed()) {
        return Result::error("Failed to flush changes: " + result.message);
    }
    
    return Result::ok();
}

Result updateExFATBootSectorForResize(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                                      const ExFATLayout& layout) {
    // Read existing boot sector
    ExFATBootSector boot_sector;
    uint64_t boot_offset = start_sector * 512;
    Result result = disk->read(&boot_sector, boot_offset, sizeof(ExFATBootSector));
    if (result.failed()) {
        return result;
    }
    
    // Update fields
    boot_sector.bs_volume_length = layout.volume_length;
    boot_sector.bs_cluster_count = layout.cluster_count;
    
    // Recalculate checksum
    // (Simplified - would need full checksum calculation)
    
    // Write updated boot sector
    result = disk->write(&boot_sector, boot_offset, sizeof(ExFATBootSector));
    if (result.failed()) {
        return Result::error("Failed to write boot sector: " + result.message);
    }
    
    return Result::ok();
}

Result extendExFATAllocationBitmap(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                                   const ExFATLayout& old_layout, const ExFATLayout& new_layout) {
    // Calculate new bitmap size
    uint64_t old_bitmap_size = (old_layout.cluster_count + 7) / 8;
    uint64_t new_bitmap_size = (new_layout.cluster_count + 7) / 8;
    
    if (new_bitmap_size <= old_bitmap_size) {
        return Result::ok(); // Nothing to extend
    }
    
    // Read old bitmap
    uint32_t bitmap_cluster = 3;
    uint64_t bitmap_sector = start_sector + new_layout.clusterToSector(bitmap_cluster);
    uint64_t bitmap_offset = bitmap_sector * new_layout.bytes_per_sector;
    
    std::vector<uint8_t> bitmap(new_bitmap_size, 0);
    Result result = disk->read(bitmap.data(), bitmap_offset, old_bitmap_size);
    if (result.failed()) {
        return Result::error("Failed to read bitmap: " + result.message);
    }
    
    // Extended area should be marked as free (already 0)
    
    // Write extended bitmap
    result = disk->write(bitmap.data(), bitmap_offset, new_bitmap_size);
    if (result.failed()) {
        return Result::error("Failed to write extended bitmap: " + result.message);
    }
    
    return Result::ok();
}

} // namespace exfat
} // namespace opm
