#include "opm/exfat_impl.hpp"
#include "opm/disk_io.hpp"
#include <cstring>

namespace opm {
namespace exfat {

// ============================================================================
// Create exFAT Boot Sector
// ============================================================================

Result createExFATBootSector(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                               const ExFATLayout& layout) {
    // Create boot sector
    ExFATBootSector boot_sector;
    initExFATBootSector(boot_sector, layout);
    
    // Calculate boot sector offset
    uint64_t boot_offset = start_sector * layout.bytes_per_sector;
    
    // Write boot sector
    Result result = disk->write(&boot_sector, boot_offset, sizeof(ExFATBootSector));
    if (result.failed()) {
        return Result::error("Failed to write exFAT boot sector: " + result.message);
    }
    
    // Create extended boot sectors (sectors 1-8)
    ExFATExtendedBootSector extended_sectors[8];
    for (int i = 0; i < 8; i++) {
        extended_sectors[i].init();
    }
    
    uint64_t extended_offset = boot_offset + sizeof(ExFATBootSector);
    result = disk->write(extended_sectors, extended_offset, sizeof(extended_sectors));
    if (result.failed()) {
        return Result::error("Failed to write extended boot sectors: " + result.message);
    }
    
    // Create OEM parameters (sector 9)
    ExFATOemParameters oem_params;
    oem_params.init();
    
    uint64_t oem_offset = extended_offset + sizeof(extended_sectors);
    result = disk->write(&oem_params, oem_offset, sizeof(ExFATOemParameters));
    if (result.failed()) {
        return Result::error("Failed to write OEM parameters: " + result.message);
    }
    
    // Create checksum sector (sector 11)
    // First, calculate checksum over boot region (sectors 0-10)
    std::vector<uint8_t> boot_region(11 * layout.bytes_per_sector);
    result = disk->read(boot_region.data(), boot_offset, boot_region.size());
    if (result.failed()) {
        return Result::error("Failed to read boot region for checksum: " + result.message);
    }
    
    uint32_t checksum = calculateExFATBootChecksum(boot_region.data(), boot_region.size());
    
    // Write checksum sector (repeating the 32-bit checksum)
    uint64_t checksum_sector_offset = boot_offset + (11 * layout.bytes_per_sector);
    std::vector<uint32_t> checksum_data(layout.bytes_per_sector / sizeof(uint32_t));
    for (size_t i = 0; i < checksum_data.size(); i++) {
        checksum_data[i] = checksum;
    }
    
    result = disk->write(checksum_data.data(), checksum_sector_offset, layout.bytes_per_sector);
    if (result.failed()) {
        return Result::error("Failed to write checksum sector: " + result.message);
    }
    
    // Write backup checksum sector (sector 23)
    uint64_t backup_checksum_offset = boot_offset + (23 * layout.bytes_per_sector);
    result = disk->write(checksum_data.data(), backup_checksum_offset, layout.bytes_per_sector);
    if (result.failed()) {
        return Result::error("Failed to write backup checksum sector: " + result.message);
    }
    
    return Result::ok();
}

// ============================================================================
// Create exFAT FAT
// ============================================================================

Result createExFATFAT(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                       const ExFATLayout& layout) {
    // FAT starts at fat_offset
    uint64_t fat_sector = start_sector + layout.fat_offset;
    uint64_t fat_offset_bytes = fat_sector * layout.bytes_per_sector;
    
    // Create FAT table
    // Each entry is 32 bits (4 bytes)
    uint32_t fat_entries = layout.cluster_count + 2; // +2 for reserved clusters 0 and 1
    uint64_t fat_size = fat_entries * sizeof(uint32_t);
    
    std::vector<uint32_t> fat_table(fat_entries, EXFAT_CLUSTER_FREE);
    
    // Cluster 0: Media type descriptor (usually 0xFFFFFFF8)
    fat_table[0] = 0xFFFFFFF8;
    
    // Cluster 1: Reserved (0xFFFFFFFF)
    fat_table[1] = EXFAT_CLUSTER_END;
    
    // Cluster 2: Root directory (end of chain)
    fat_table[2] = EXFAT_CLUSTER_END;
    
    // Cluster 3: Allocation bitmap (end of chain)
    if (layout.cluster_count > 3) {
        fat_table[3] = EXFAT_CLUSTER_END;
    }
    
    // Cluster 4: Up-case table (end of chain)
    if (layout.cluster_count > 4) {
        fat_table[4] = EXFAT_CLUSTER_END;
    }
    
    // Write FAT
    Result result = disk->write(fat_table.data(), fat_offset_bytes, fat_size);
    if (result.failed()) {
        return Result::error("Failed to write exFAT FAT: " + result.message);
    }
    
    return Result::ok();
}

// ============================================================================
// Create exFAT Allocation Bitmap
// ============================================================================

Result createExFATAllocationBitmap(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                                      const ExFATLayout& layout) {
    // Bitmap is at cluster 3
    uint32_t bitmap_cluster = 3;
    uint64_t bitmap_sector = start_sector + layout.clusterToSector(bitmap_cluster);
    uint64_t bitmap_offset = bitmap_sector * layout.bytes_per_sector;
    
    // Calculate bitmap size (1 bit per cluster)
    uint64_t bitmap_bits = layout.cluster_count;
    uint64_t bitmap_bytes = (bitmap_bits + 7) / 8;
    uint64_t bitmap_sectors = (bitmap_bytes + layout.bytes_per_sector - 1) / layout.bytes_per_sector;
    
    // Ensure bitmap doesn't exceed one cluster
    uint64_t max_bitmap_size = layout.bytes_per_cluster;
    if (bitmap_bytes > max_bitmap_size) {
        bitmap_bytes = max_bitmap_size;
    }
    
    // Create bitmap
    std::vector<uint8_t> bitmap(bitmap_bytes, 0);
    
    // Mark clusters as allocated:
    // Cluster 0-1: Reserved (not in bitmap)
    // Cluster 2: Root directory
    // Cluster 3: Allocation bitmap
    // Cluster 4: Up-case table
    // Cluster 5+: Bitmap itself (if needed)
    
    // Mark root directory (cluster 2)
    bitmap[0] |= (1 << 2); // Cluster 2 -> bit 2
    
    // Mark allocation bitmap (cluster 3)
    bitmap[0] |= (1 << 3); // Cluster 3 -> bit 3
    
    // Mark up-case table (cluster 4)
    bitmap[0] |= (1 << 4); // Cluster 4 -> bit 4
    
    // Mark any additional clusters used by bitmap
    for (uint32_t cluster = 5; cluster < 5 + bitmap_sectors / layout.sectors_per_cluster; cluster++) {
        uint64_t byte_idx = cluster / 8;
        uint64_t bit_idx = cluster % 8;
        if (byte_idx < bitmap_bytes) {
            bitmap[byte_idx] |= (1 << bit_idx);
        }
    }
    
    // Write bitmap
    Result result = disk->write(bitmap.data(), bitmap_offset, bitmap_bytes);
    if (result.failed()) {
        return Result::error("Failed to write exFAT allocation bitmap: " + result.message);
    }
    
    // Clear remaining bytes in cluster
    if (bitmap_bytes < layout.bytes_per_cluster) {
        std::vector<uint8_t> zeros(layout.bytes_per_cluster - bitmap_bytes, 0);
        result = disk->write(zeros.data(), bitmap_offset + bitmap_bytes, zeros.size());
        if (result.failed()) {
            return result;
        }
    }
    
    return Result::ok();
}

// ============================================================================
// Create exFAT Up-Case Table
// ============================================================================

Result createExFATUpcaseTable(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                                const ExFATLayout& layout) {
    // Up-case table is at cluster 4
    uint32_t upcase_cluster = 4;
    uint64_t upcase_sector = start_sector + layout.clusterToSector(upcase_cluster);
    uint64_t upcase_offset = upcase_sector * layout.bytes_per_sector;
    
    // exFAT up-case table is 5836 bytes for the basic BMP range
    // We'll create a simplified version that just handles ASCII
    // Full table would require 65536 * 2 = 131072 bytes for Unicode BMP
    
    // For simplicity, create a minimal up-case table (just ASCII range)
    // Each entry is: lowercase -> uppercase
    uint32_t upcase_entries = 128; // ASCII range
    uint64_t upcase_size = upcase_entries * sizeof(uint16_t);
    
    std::vector<uint16_t> upcase_table(upcase_entries);
    
    // Fill table: identity for most characters, uppercase for lowercase
    for (uint32_t i = 0; i < upcase_entries; i++) {
        if (i >= 'a' && i <= 'z') {
            upcase_table[i] = i - 'a' + 'A'; // Convert to uppercase
        } else {
            upcase_table[i] = i; // Identity
        }
    }
    
    // Write up-case table
    Result result = disk->write(upcase_table.data(), upcase_offset, upcase_size);
    if (result.failed()) {
        return Result::error("Failed to write exFAT up-case table: " + result.message);
    }
    
    // Clear remaining bytes in cluster
    if (upcase_size < layout.bytes_per_cluster) {
        std::vector<uint8_t> zeros(layout.bytes_per_cluster - upcase_size, 0);
        result = disk->write(zeros.data(), upcase_offset + upcase_size, zeros.size());
        if (result.failed()) {
            return result;
        }
    }
    
    return Result::ok();
}

// ============================================================================
// Create exFAT Root Directory
// ============================================================================

Result createExFATRootDirectory(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                                  const ExFATLayout& layout, const std::string& label) {
    // Root directory is at cluster 2
    uint32_t root_cluster = layout.root_cluster;
    uint64_t root_sector = start_sector + layout.clusterToSector(root_cluster);
    uint64_t root_offset = root_sector * layout.bytes_per_sector;
    
    // Create directory entries
    // Each entry is 32 bytes
    // Required entries:
    // 1. Volume label (optional)
    // 2. Allocation bitmap entry
    // 3. Up-case table entry
    
    std::vector<uint8_t> root_entries(layout.bytes_per_cluster, 0);
    size_t entry_offset = 0;
    
    // Entry 1: Volume label (if provided)
    if (!label.empty()) {
        ExFATVolumeLabelEntry volume_label;
        std::memset(&volume_label, 0, sizeof(ExFATVolumeLabelEntry));
        volume_label.entry_type = EXFAT_ENTRY_VOLUME_LABEL;
        
        // Convert label to UTF-16LE (simplified - just ASCII)
        volume_label.character_count = static_cast<uint8_t>(std::min(label.length(), size_t(11)));
        for (size_t i = 0; i < volume_label.character_count; i++) {
            volume_label.volume_label[i] = static_cast<char16_t>(label[i]);
        }
        
        std::memcpy(root_entries.data() + entry_offset, &volume_label, sizeof(ExFATVolumeLabelEntry));
        entry_offset += sizeof(ExFATVolumeLabelEntry);
    }
    
    // Entry 2: Allocation bitmap
    ExFATAllocationBitmapEntry bitmap_entry;
    std::memset(&bitmap_entry, 0, sizeof(ExFATAllocationBitmapEntry));
    bitmap_entry.entry_type = EXFAT_ENTRY_ALLOCATION_BITMAP;
    bitmap_entry.bitmap_flags = 0x00; // First bitmap
    bitmap_entry.first_cluster = 3;   // Cluster 3
    bitmap_entry.data_length = (layout.cluster_count + 7) / 8; // 1 bit per cluster
    
    std::memcpy(root_entries.data() + entry_offset, &bitmap_entry, sizeof(ExFATAllocationBitmapEntry));
    entry_offset += sizeof(ExFATAllocationBitmapEntry);
    
    // Entry 3: Up-case table
    ExFATUpcaseTableEntry upcase_entry;
    std::memset(&upcase_entry, 0, sizeof(ExFATUpcaseTableEntry));
    upcase_entry.entry_type = EXFAT_ENTRY_UPCASE_TABLE;
    upcase_entry.checksum = 0; // Simplified - would need proper calculation
    upcase_entry.first_cluster = 4; // Cluster 4
    upcase_entry.data_length = 256; // Simplified size
    
    std::memcpy(root_entries.data() + entry_offset, &upcase_entry, sizeof(ExFATUpcaseTableEntry));
    entry_offset += sizeof(ExFATUpcaseTableEntry);
    
    // End of directory marker
    root_entries[entry_offset] = EXFAT_ENTRY_END;
    
    // Write root directory
    Result result = disk->write(root_entries.data(), root_offset, root_entries.size());
    if (result.failed()) {
        return Result::error("Failed to write exFAT root directory: " + result.message);
    }
    
    return Result::ok();
}

} // namespace exfat
} // namespace opm
