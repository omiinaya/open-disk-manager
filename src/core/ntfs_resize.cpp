#include "opm/ntfs_impl.hpp"
#include "opm/disk_io.hpp"
#include <cstring>

namespace opm {
namespace ntfs {

// ============================================================================
// NTFS Resize Operations - Phase 3.4.8
// ============================================================================

Result resizeNTFS(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                  uint64_t new_size_bytes) {
    // Step 1: Read current boot sector to get layout
    NTFSBootSector boot_sector;
    uint64_t boot_offset = start_sector * NTFS_SECTOR_SIZE;
    Result result = disk->read(&boot_sector, boot_offset, sizeof(NTFSBootSector));
    if (result.failed()) {
        return Result::error("Failed to read boot sector: " + result.message);
    }

    // Validate current filesystem
    if (boot_sector.bs_boot_signature != NTFS_BOOT_SIGNATURE) {
        return Result::error("Invalid boot signature - not an NTFS volume");
    }

    if (std::memcmp(boot_sector.bs_oem, NTFS_OEM_ID, 8) != 0) {
        return Result::error("Not an NTFS volume");
    }

    // Calculate old layout
    NTFSLayout old_layout;
    old_layout.bytes_per_sector = boot_sector.bpb_bytes_per_sector;
    old_layout.sectors_per_cluster = boot_sector.bpb_sectors_per_cluster;
    old_layout.bytes_per_cluster = old_layout.bytes_per_sector * old_layout.sectors_per_cluster;
    old_layout.total_sectors = boot_sector.bpb_total_sectors;
    old_layout.total_clusters = old_layout.total_sectors / old_layout.sectors_per_cluster;
    old_layout.total_size = old_layout.total_sectors * old_layout.bytes_per_sector;
    old_layout.mft_lcn = boot_sector.bs_mft_lcn;
    old_layout.mft_mirr_lcn = boot_sector.bs_mft_mirr_lcn;
    old_layout.serial_number = boot_sector.bs_volume_serial;
    
    if (boot_sector.bs_clusters_per_mft_record < 0) {
        old_layout.mft_record_size = 1ULL << (-boot_sector.bs_clusters_per_mft_record);
    } else {
        old_layout.mft_record_size = boot_sector.bs_clusters_per_mft_record * old_layout.bytes_per_cluster;
    }
    
    if (boot_sector.bs_clusters_per_index_record < 0) {
        old_layout.index_record_size = 1ULL << (-boot_sector.bs_clusters_per_index_record);
    } else {
        old_layout.index_record_size = boot_sector.bs_clusters_per_index_record * old_layout.bytes_per_cluster;
    }

    // Calculate new layout
    NTFSLayout new_layout;
    new_layout.calculate(new_size_bytes);
    
    if (!new_layout.validate()) {
        return Result::error("Invalid new volume size");
    }

    // Check if we're shrinking
    if (new_size_bytes < old_layout.total_size) {
        // Check if MFT would be truncated
        uint64_t old_mft_end = old_layout.mft_lcn * old_layout.bytes_per_cluster + 
                               (16 * old_layout.mft_record_size);
        uint64_t new_mft_end = new_layout.mft_lcn * new_layout.bytes_per_cluster + 
                               (16 * new_layout.mft_record_size);
        
        if (new_mft_end < old_mft_end) {
            return Result::error("Cannot shrink volume: MFT would be truncated");
        }
        
        // Check bitmap
        uint64_t bitmap_clusters = (new_layout.total_clusters + 7) / 8 / new_layout.bytes_per_cluster;
        uint64_t new_bitmap_end = new_layout.mft_lcn * new_layout.bytes_per_cluster + 
                                 (16 * new_layout.mft_record_size) + 
                                 (10 * new_layout.bytes_per_cluster) + 
                                 (bitmap_clusters * new_layout.bytes_per_cluster);
        
        if (new_bitmap_end > new_size_bytes) {
            return Result::error("Cannot shrink volume: bitmap would exceed volume");
        }
    }

    // Update boot sector with new size
    result = updateBootSectorForResize(disk, start_sector, new_layout);
    if (result.failed()) {
        return Result::error("Failed to update boot sector: " + result.message);
    }

    // If growing, extend MFT and bitmap
    if (new_size_bytes > old_layout.total_size) {
        // Extend MFT if needed
        result = extendMFT(disk, start_sector, old_layout, new_layout);
        if (result.failed()) {
            return Result::error("Failed to extend MFT: " + result.message);
        }

        // Update bitmap
        uint64_t new_bitmap_size = (new_layout.total_clusters + 7) / 8;
        uint64_t bitmap_cluster = new_layout.mft_lcn + 
            (16 * new_layout.mft_record_size / new_layout.bytes_per_cluster) + 10;
        uint64_t bitmap_sector = start_sector + bitmap_cluster * new_layout.sectors_per_cluster;
        uint64_t bitmap_offset = bitmap_sector * new_layout.bytes_per_sector;
        
        // Create extended bitmap
        std::vector<uint8_t> new_bitmap(new_bitmap_size, 0);
        
        // Mark system clusters as used
        uint64_t used_clusters = new_layout.mft_lcn + 
            (16 * new_layout.mft_record_size / new_layout.bytes_per_cluster);
        used_clusters = std::max(used_clusters, static_cast<uint64_t>(10));
        
        for (uint64_t i = 0; i < used_clusters && i < new_layout.total_clusters; i++) {
            uint64_t byte_idx = i / 8;
            uint64_t bit_idx = i % 8;
            if (byte_idx < new_bitmap_size) {
                new_bitmap[byte_idx] |= (1 << bit_idx);
            }
        }
        
        result = disk->write(new_bitmap.data(), bitmap_offset, new_bitmap_size);
        if (result.failed()) {
            return Result::error("Failed to update bitmap: " + result.message);
        }
    }

    // Flush changes
    result = disk->flush();
    if (result.failed()) {
        return Result::error("Failed to flush changes: " + result.message);
    }

    return Result::ok();
}

// ============================================================================
// Extend MFT
// ============================================================================

Result extendMFT(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                 const NTFSLayout& old_layout, const NTFSLayout& new_layout) {
    uint64_t old_mft_end = old_layout.mft_lcn * old_layout.bytes_per_cluster + 
                          (16 * old_layout.mft_record_size);
    uint64_t new_mft_end = new_layout.mft_lcn * new_layout.bytes_per_cluster + 
                          (16 * new_layout.mft_record_size);
    
    // If MFT size hasn't changed, nothing to do
    if (new_mft_end <= old_mft_end) {
        return Result::ok();
    }

    uint64_t mft_sector = start_sector + new_layout.mft_lcn * new_layout.sectors_per_cluster;
    uint64_t mft_offset = mft_sector * new_layout.bytes_per_sector;

    // Clear new space in MFT (records 16+)
    std::vector<uint8_t> zeros(new_layout.mft_record_size, 0);
    
    for (uint64_t i = 16; i < 256 && i < (4 * 1024 * 1024 / new_layout.mft_record_size); i++) {
        uint64_t record_offset = mft_offset + (i * new_layout.mft_record_size);
        
        // Check if record already exists
        MFTRecordHeader existing;
        Result result = disk->read(&existing, record_offset, sizeof(MFTRecordHeader));
        
        if (result.success() && existing.mr_magic == MFT_RECORD_MAGIC) {
            // Record exists, skip
            continue;
        }
        
        // Write empty record
        result = disk->write(zeros.data(), record_offset, new_layout.mft_record_size);
        if (result.failed()) {
            return Result::error("Failed to extend MFT at record " + std::to_string(i));
        }
    }

    return Result::ok();
}

// ============================================================================
// Update Boot Sector for Resize
// ============================================================================

Result updateBootSectorForResize(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                                 const NTFSLayout& layout) {
    // Read existing boot sector
    NTFSBootSector boot_sector;
    uint64_t boot_offset = start_sector * NTFS_SECTOR_SIZE;
    Result result = disk->read(&boot_sector, boot_offset, sizeof(NTFSBootSector));
    if (result.failed()) {
        return result;
    }

    // Update total sectors
    boot_sector.bpb_total_sectors = layout.total_sectors;
    
    // Update MFT mirror position (middle of new volume)
    boot_sector.bs_mft_mirr_lcn = layout.mft_mirr_lcn;
    
    // Recalculate checksum (though NTFS doesn't use it)
    // For now, leave it as 0
    boot_sector.bs_checksum = 0;

    // Write updated boot sector
    result = disk->write(&boot_sector, boot_offset, sizeof(NTFSBootSector));
    if (result.failed()) {
        return Result::error("Failed to write boot sector: " + result.message);
    }

    return Result::ok();
}

} // namespace ntfs
} // namespace opm
