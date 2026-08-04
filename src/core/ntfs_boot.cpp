#include "opm/ntfs_impl.hpp"
#include "opm/disk_io.hpp"
#include <cstring>

namespace opm {
namespace ntfs {

// ============================================================================
// NTFS Boot Sector Creation - Phase 3.4.2
// ============================================================================

Result createNTFSBootSector(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                              const NTFSLayout& layout) {
    
    // Create boot sector
    NTFSBootSector boot_sector;
    initBootSector(boot_sector, layout);
    
    // Calculate boot sector offset
    uint64_t boot_offset = start_sector * layout.bytes_per_sector;
    
    // Write boot sector
    Result result = disk->write(&boot_sector, boot_offset, sizeof(NTFSBootSector));
    if (result.failed()) {
        return Result::error("Failed to write NTFS boot sector: " + result.message);
    }
    
    return Result::ok();
}

// ============================================================================
// Create MFT (Master File Table)
// ============================================================================

Result createMFT(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                  const NTFSLayout& layout) {
    
    // MFT starts at bs_mft_lcn
    uint64_t mft_sector = start_sector + layout.mft_lcn * layout.sectors_per_cluster;
    uint64_t mft_offset = mft_sector * layout.bytes_per_sector;
    
    // Create first 16 MFT records (system files)
    for (uint64_t i = 0; i < 16; i++) {
        MFTRecordHeader record;
        initMFTRecord(record, i, false);
        
        // Set directory flag for appropriate records
        if (i == MFT_ROOT || i == MFT_EXTEND) {
            record.mr_flags |= MFT_RECORD_FLAG_DIR;
            record.mr_hard_link_count = 2;
        }
        
        // Write record
        uint64_t record_offset = mft_offset + (i * layout.mft_record_size);
        Result result = disk->write(&record, record_offset, sizeof(MFTRecordHeader));
        if (result.failed()) {
            return Result::error("Failed to write MFT record " + 
                               std::to_string(i) + ": " + result.message);
        }
        
        // Write rest of record as zeros
        std::vector<uint8_t> zeros(layout.mft_record_size - sizeof(MFTRecordHeader), 0);
        result = disk->write(zeros.data(), 
                             record_offset + sizeof(MFTRecordHeader),
                             zeros.size());
        if (result.failed()) {
            return result;
        }
    }
    
    // Create MFT mirror at bs_mft_mirr_lcn
    uint64_t mirr_sector = start_sector + layout.mft_mirr_lcn * layout.sectors_per_cluster;
    uint64_t mirr_offset = mirr_sector * layout.bytes_per_sector;
    
    // Copy first 4 MFT records to mirror
    for (uint64_t i = 0; i < 4; i++) {
        uint64_t src_offset = mft_offset + (i * layout.mft_record_size);
        uint64_t dst_offset = mirr_offset + (i * layout.mft_record_size);
        
        std::vector<uint8_t> record_data(layout.mft_record_size);
        Result result = disk->read(record_data.data(), src_offset, layout.mft_record_size);
        if (result.failed()) {
            return Result::error("Failed to read MFT record for mirror: " + result.message);
        }
        
        result = disk->write(record_data.data(), dst_offset, layout.mft_record_size);
        if (result.failed()) {
            return Result::error("Failed to write MFT mirror: " + result.message);
        }
    }
    
    return Result::ok();
}

// ============================================================================
// Create System Files
// ============================================================================

Result createSystemFiles(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                          const NTFSLayout& layout) {
    
    // Create $LogFile (for journaling)
    Result result = createLogFile(disk, start_sector, layout);
    if (result.failed()) {
        return result;
    }
    
    // Create $Bitmap (cluster allocation)
    result = createBitmap(disk, start_sector, layout);
    if (result.failed()) {
        return result;
    }
    
    // Create $UpCase (uppercase conversion table)
    result = createUpCase(disk, start_sector, layout);
    if (result.failed()) {
        return result;
    }
    
    return Result::ok();
}

// ============================================================================
// System-file cluster allocation helpers
// ============================================================================

uint64_t getBitmapCluster(const NTFSLayout& layout) {
    // $Bitmap sits immediately after the MFT's 16 system records
    uint64_t mft_clusters = (16ULL * layout.mft_record_size +
                             layout.bytes_per_cluster - 1) /
                            layout.bytes_per_cluster;
    return layout.mft_lcn + mft_clusters;
}

uint64_t getLogFileClusters(const NTFSLayout& layout) {
    uint64_t log_size = 4ULL * 1024 * 1024;
    if (layout.total_size < 100ULL * 1024 * 1024) {
        log_size = 1ULL * 1024 * 1024;  // 1MB for small volumes
    }
    uint64_t clusters = log_size / layout.bytes_per_cluster;
    return clusters == 0 ? 1 : clusters;
}

uint64_t getLogFileCluster(const NTFSLayout& layout) {
    // $LogFile goes after $Bitmap
    uint64_t bitmap_bytes = (layout.total_clusters + 7) / 8;
    uint64_t bitmap_clusters = (bitmap_bytes + layout.bytes_per_cluster - 1) /
                               layout.bytes_per_cluster;
    return getBitmapCluster(layout) + bitmap_clusters;
}

uint64_t getUpCaseCluster(const NTFSLayout& layout) {
    // $UpCase goes after $LogFile
    return getLogFileCluster(layout) + getLogFileClusters(layout);
}

// ============================================================================
// Create Bitmap ($Bitmap)
// ============================================================================

Result createBitmap(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                     const NTFSLayout& layout) {
    
    // $Bitmap is at MFT record 6
    // Calculate bitmap size (1 bit per cluster)
    uint64_t bitmap_size = (layout.total_clusters + 7) / 8;  // Round up to bytes
    
    // Create bitmap buffer
    std::vector<uint8_t> bitmap(bitmap_size, 0);
    
    // Mark system clusters as used
    // Boot sector (cluster 0), MFT clusters, etc.
    uint64_t used_clusters = layout.mft_lcn + (16 * layout.mft_record_size / layout.bytes_per_cluster);
    used_clusters = std::max(used_clusters, static_cast<uint64_t>(10));
    
    for (uint64_t i = 0; i < used_clusters && i < layout.total_clusters; i++) {
        uint64_t byte_idx = i / 8;
        uint64_t bit_idx = i % 8;
        if (byte_idx < bitmap_size) {
            bitmap[byte_idx] |= (1 << bit_idx);
        }
    }
    
    // Allocate bitmap right after the MFT records
    uint64_t bitmap_cluster = getBitmapCluster(layout);
    uint64_t bitmap_sector = start_sector + bitmap_cluster * layout.sectors_per_cluster;
    uint64_t bitmap_offset = bitmap_sector * layout.bytes_per_sector;
    
    // Write bitmap
    Result result = disk->write(bitmap.data(), bitmap_offset, bitmap_size);
    if (result.failed()) {
        return Result::error("Failed to write $Bitmap: " + result.message);
    }
    
    return Result::ok();
}

// ============================================================================
// Create Log File ($LogFile)
// ============================================================================

Result createLogFile(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                      const NTFSLayout& layout) {
    
    // $LogFile is at MFT record 2
    // Journal size (typically 4MB minimum)
    uint64_t log_size = 4 * 1024 * 1024;  // 4MB
    if (layout.total_size < 100 * 1024 * 1024) {
        log_size = 1 * 1024 * 1024;  // 1MB for small volumes
    }
    
    // Create log file with restart pages
    uint64_t log_clusters = log_size / layout.bytes_per_cluster;
    if (log_clusters == 0) log_clusters = 1;
    
    // Allocate log file after the $Bitmap
    uint64_t log_cluster = getLogFileCluster(layout);
    uint64_t log_sector = start_sector + log_cluster * layout.sectors_per_cluster;
    uint64_t log_offset = log_sector * layout.bytes_per_sector;
    
    // Initialize log file with zeros (restart pages will be written on first mount)
    std::vector<uint8_t> log_data(log_size, 0);
    
    // Set up restart page signature at beginning
    // Restart page starts with 'RSTR' or 'RCND'
    log_data[0] = 'R';
    log_data[1] = 'S';
    log_data[2] = 'T';
    log_data[3] = 'R';
    
    Result result = disk->write(log_data.data(), log_offset, log_size);
    if (result.failed()) {
        return Result::error("Failed to write $LogFile: " + result.message);
    }
    
    return Result::ok();
}

// ============================================================================
// Create UpCase Table ($UpCase)
// ============================================================================

Result createUpCase(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                      const NTFSLayout& layout) {
    
    // $UpCase is at MFT record 10
    // Create uppercase conversion table (64KB for Unicode BMP)
    std::vector<uint16_t> upcase(65536);
    
    // Fill with simple ASCII uppercase conversion
    for (uint32_t i = 0; i < 65536; i++) {
        if (i >= 'a' && i <= 'z') {
            upcase[i] = i - 'a' + 'A';
        } else {
            upcase[i] = i;
        }
    }
    
    // Allocate space after $LogFile (no collision with $Bitmap)
    uint64_t upcase_cluster = getUpCaseCluster(layout);
    uint64_t upcase_sector = start_sector + upcase_cluster * layout.sectors_per_cluster;
    uint64_t upcase_offset = upcase_sector * layout.bytes_per_sector;
    
    // Write UpCase table
    Result result = disk->write(upcase.data(), upcase_offset, upcase.size() * sizeof(uint16_t));
    if (result.failed()) {
        return Result::error("Failed to write $UpCase: " + result.message);
    }
    
    return Result::ok();
}

// ============================================================================
// Create Root Directory
// ============================================================================

Result createRootDirectory(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                            const NTFSLayout& layout, const std::string& label) {
    
    // Root is at MFT record 5
    uint64_t mft_sector = start_sector + layout.mft_lcn * layout.sectors_per_cluster;
    uint64_t root_record_offset = mft_sector * layout.bytes_per_sector + 
                                   (MFT_ROOT * layout.mft_record_size);
    
    // Create root directory record
    MFTRecordHeader root_record;
    initMFTRecord(root_record, MFT_ROOT, true);  // true = is directory
    
    // Set attributes for root
    // Would add $STANDARD_INFORMATION, $FILE_NAME, $INDEX_ROOT, etc.
    
    Result result = disk->write(&root_record, root_record_offset, sizeof(MFTRecordHeader));
    if (result.failed()) {
        return Result::error("Failed to write root directory: " + result.message);
    }
    
    // Write volume label
    if (!label.empty()) {
        // Create $Volume record (MFT record 3) with volume name
        // This is simplified - real implementation would add $VOLUME_NAME attribute
    }
    
    return Result::ok();
}

} // namespace ntfs
} // namespace opm
