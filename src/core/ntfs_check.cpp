#include "opm/ntfs_impl.hpp"
#include "opm/disk_io.hpp"
#include <cstring>
#include <sstream>

namespace opm {
namespace ntfs {

// ============================================================================
// NTFS Check Operations - Phase 3.4.7
// ============================================================================

Result checkNTFS(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                 bool repair, std::vector<std::string>* errors) {
    if (errors) {
        errors->clear();
    }

    // Step 1: Check boot sector
    Result result = checkBootSector(disk, start_sector, repair, errors);
    if (result.failed()) {
        return result;
    }

    // Read boot sector to get layout
    NTFSBootSector boot_sector;
    uint64_t boot_offset = start_sector * NTFS_SECTOR_SIZE;
    result = disk->read(&boot_sector, boot_offset, sizeof(NTFSBootSector));
    if (result.failed()) {
        if (errors) errors->push_back("Failed to read boot sector: " + result.message);
        return result;
    }

    // Calculate layout from boot sector
    NTFSLayout layout;
    layout.bytes_per_sector = boot_sector.bpb_bytes_per_sector;
    layout.sectors_per_cluster = boot_sector.bpb_sectors_per_cluster;
    layout.bytes_per_cluster = layout.bytes_per_sector * layout.sectors_per_cluster;
    layout.total_sectors = boot_sector.bpb_total_sectors;
    layout.total_clusters = layout.total_sectors / layout.sectors_per_cluster;
    layout.total_size = layout.total_sectors * layout.bytes_per_sector;
    layout.mft_lcn = boot_sector.bs_mft_lcn;
    layout.mft_mirr_lcn = boot_sector.bs_mft_mirr_lcn;
    layout.serial_number = boot_sector.bs_volume_serial;
    
    // Calculate MFT record size
    if (boot_sector.bs_clusters_per_mft_record < 0) {
        layout.mft_record_size = 1ULL << (-boot_sector.bs_clusters_per_mft_record);
    } else {
        layout.mft_record_size = boot_sector.bs_clusters_per_mft_record * layout.bytes_per_cluster;
    }
    
    // Calculate index record size
    if (boot_sector.bs_clusters_per_index_record < 0) {
        layout.index_record_size = 1ULL << (-boot_sector.bs_clusters_per_index_record);
    } else {
        layout.index_record_size = boot_sector.bs_clusters_per_index_record * layout.bytes_per_cluster;
    }

    if (!layout.validate()) {
        if (errors) errors->push_back("Invalid NTFS layout calculated from boot sector");
        return Result::error("Invalid NTFS layout");
    }

    // Step 2: Check MFT
    result = checkMFT(disk, start_sector, layout, repair, errors);
    if (result.failed()) {
        return result;
    }

    // Step 3: Check bitmap
    result = checkBitmap(disk, start_sector, layout, repair, errors);
    if (result.failed()) {
        return result;
    }

    // Step 4: Check log file
    result = checkLogFile(disk, start_sector, layout, repair, errors);
    if (result.failed()) {
        return result;
    }

    return Result::ok();
}

// ============================================================================
// Check Boot Sector
// ============================================================================

Result checkBootSector(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                       bool repair, std::vector<std::string>* errors) {
    uint64_t boot_offset = start_sector * NTFS_SECTOR_SIZE;
    
    // Read boot sector
    NTFSBootSector boot_sector;
    Result result = disk->read(&boot_sector, boot_offset, sizeof(NTFSBootSector));
    if (result.failed()) {
        if (errors) errors->push_back("Failed to read boot sector: " + result.message);
        return Result::error("Cannot read boot sector");
    }

    // Check boot signature
    if (boot_sector.bs_boot_signature != NTFS_BOOT_SIGNATURE) {
        if (errors) {
            std::stringstream ss;
            ss << "Boot signature mismatch: expected 0x" << std::hex << NTFS_BOOT_SIGNATURE
               << ", got 0x" << boot_sector.bs_boot_signature << std::dec;
            errors->push_back(ss.str());
        }
        if (!repair) {
            return Result::error("Invalid boot signature");
        }
    }

    // Check OEM ID
    if (std::memcmp(boot_sector.bs_oem, NTFS_OEM_ID, 8) != 0) {
        if (errors) {
            errors->push_back("OEM ID mismatch - not an NTFS volume");
        }
        return Result::error("Not an NTFS volume");
    }

    // Check sector size (must be 512)
    if (boot_sector.bpb_bytes_per_sector != 512) {
        if (errors) {
            errors->push_back("Invalid bytes per sector: " + 
                            std::to_string(boot_sector.bpb_bytes_per_sector));
        }
        return Result::error("Invalid sector size");
    }

    // Check sectors per cluster
    if (boot_sector.bpb_sectors_per_cluster == 0 ||
        boot_sector.bpb_sectors_per_cluster > 128) {
        if (errors) {
            errors->push_back("Invalid sectors per cluster: " + 
                            std::to_string(boot_sector.bpb_sectors_per_cluster));
        }
        return Result::error("Invalid cluster size");
    }

    // Check total sectors
    if (boot_sector.bpb_total_sectors == 0) {
        if (errors) errors->push_back("Total sectors is zero");
        return Result::error("Invalid volume size");
    }

    // Check MFT LCN
    if (boot_sector.bs_mft_lcn == 0) {
        if (errors) errors->push_back("MFT LCN is zero");
        return Result::error("Invalid MFT location");
    }

    // Check MFT mirror LCN
    if (boot_sector.bs_mft_mirr_lcn == 0 ||
        boot_sector.bs_mft_mirr_lcn >= boot_sector.bpb_total_sectors / boot_sector.bpb_sectors_per_cluster) {
        if (errors) errors->push_back("Invalid MFT mirror LCN");
        return Result::error("Invalid MFT mirror location");
    }

    return Result::ok();
}

// ============================================================================
// Check MFT (Master File Table)
// ============================================================================

Result checkMFT(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                const NTFSLayout& layout, bool repair, std::vector<std::string>* errors) {
    uint64_t mft_sector = start_sector + layout.mft_lcn * layout.sectors_per_cluster;
    uint64_t mft_offset = mft_sector * layout.bytes_per_sector;

    // Check first 16 system records
    bool system_records_ok = true;
    
    for (uint64_t i = 0; i < 16 && i < (4 * 1024 * 1024 / layout.mft_record_size); i++) {
        MFTRecordHeader record;
        uint64_t record_offset = mft_offset + (i * layout.mft_record_size);
        
        Result result = disk->read(&record, record_offset, sizeof(MFTRecordHeader));
        if (result.failed()) {
            if (errors) {
                errors->push_back("Failed to read MFT record " + std::to_string(i));
            }
            system_records_ok = false;
            continue;
        }

        // Check magic number
        if (record.mr_magic != MFT_RECORD_MAGIC && 
            record.mr_magic != EMPTY_RECORD_MAGIC) {
            // Empty record is okay for unused slots
            if (record.mr_magic == BAAD_RECORD_MAGIC) {
                if (errors) {
                    errors->push_back("MFT record " + std::to_string(i) + 
                                    " is corrupted (BAAD)");
                }
                system_records_ok = false;
                
                if (repair) {
                    // Mark as empty
                    MFTRecordHeader empty_record;
                    std::memset(&empty_record, 0, sizeof(MFTRecordHeader));
                    disk->write(&empty_record, record_offset, sizeof(MFTRecordHeader));
                }
            }
            continue;
        }

        // For system records (0-15), they should be in use
        if (i < 16) {
            if (!record.isInUse()) {
                if (errors) {
                    std::string name;
                    switch (i) {
                        case MFT_MFT: name = "$MFT"; break;
                        case MFT_MIRR: name = "$MFTMirr"; break;
                        case MFT_LOGFILE: name = "$LogFile"; break;
                        case MFT_VOLUME: name = "$Volume"; break;
                        case MFT_ATTRDEF: name = "$AttrDef"; break;
                        case MFT_ROOT: name = "$Root"; break;
                        case MFT_BITMAP: name = "$Bitmap"; break;
                        case MFT_BOOT: name = "$Boot"; break;
                        case MFT_BADCLUS: name = "$BadClus"; break;
                        case MFT_SECURE: name = "$Secure"; break;
                        case MFT_UPCASE: name = "$UpCase"; break;
                        case MFT_EXTEND: name = "$Extend"; break;
                        default: name = "System file"; break;
                    }
                    errors->push_back(name + " (record " + std::to_string(i) + 
                                    ") should be in use but is not marked");
                }
                system_records_ok = false;
            }

            // Check attribute offset
            if (record.mr_attr_offset < sizeof(MFTRecordHeader) ||
                record.mr_attr_offset >= layout.mft_record_size) {
                if (errors) {
                    errors->push_back("MFT record " + std::to_string(i) + 
                                    " has invalid attribute offset");
                }
                system_records_ok = false;
            }

            // Check USN (update sequence number)
            if (record.mr_usn_offset != 48) {
                if (errors) {
                    errors->push_back("MFT record " + std::to_string(i) + 
                                    " has unexpected USN offset");
                }
            }
        }
    }

    // Check MFT mirror
    uint64_t mirr_sector = start_sector + layout.mft_mirr_lcn * layout.sectors_per_cluster;
    uint64_t mirr_offset = mirr_sector * layout.bytes_per_sector;
    
    // Compare first 4 records
    bool mirror_matches = true;
    for (uint64_t i = 0; i < 4; i++) {
        std::vector<uint8_t> orig_record(layout.mft_record_size);
        std::vector<uint8_t> mirr_record(layout.mft_record_size);
        
        uint64_t orig_offset = mft_offset + (i * layout.mft_record_size);
        uint64_t mirror_record_offset = mirr_offset + (i * layout.mft_record_size);
        
        disk->read(orig_record.data(), orig_offset, layout.mft_record_size);
        disk->read(mirr_record.data(), mirror_record_offset, layout.mft_record_size);
        
        if (std::memcmp(orig_record.data(), mirr_record.data(), layout.mft_record_size) != 0) {
            mirror_matches = false;
            if (errors) {
                errors->push_back("MFT record " + std::to_string(i) + 
                                " doesn't match mirror");
            }
            
            if (repair) {
                // Copy original to mirror
                disk->write(orig_record.data(), mirror_record_offset, layout.mft_record_size);
            }
        }
    }

    if (!system_records_ok || !mirror_matches) {
        return Result::error("MFT check failed");
    }

    return Result::ok();
}

// ============================================================================
// Check Bitmap
// ============================================================================

Result checkBitmap(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                   const NTFSLayout& layout, bool repair, std::vector<std::string>* errors) {
    // Calculate expected bitmap location (shared with the format path)
    uint64_t bitmap_cluster = getBitmapCluster(layout);
    uint64_t bitmap_sector = start_sector + bitmap_cluster * layout.sectors_per_cluster;
    uint64_t bitmap_offset = bitmap_sector * layout.bytes_per_sector;
    
    uint64_t bitmap_size = (layout.total_clusters + 7) / 8;
    
    // Read bitmap
    std::vector<uint8_t> bitmap(bitmap_size);
    Result result = disk->read(bitmap.data(), bitmap_offset, bitmap_size);
    if (result.failed()) {
        if (errors) errors->push_back("Failed to read $Bitmap");
        return Result::error("Cannot read bitmap");
    }

    // Check that system clusters are marked as used
    uint64_t used_clusters = layout.mft_lcn + 
        (16 * layout.mft_record_size / layout.bytes_per_cluster);
    used_clusters = std::max(used_clusters, static_cast<uint64_t>(10));
    
    bool bitmap_ok = true;
    for (uint64_t i = 0; i < used_clusters && i < layout.total_clusters; i++) {
        uint64_t byte_idx = i / 8;
        uint64_t bit_idx = i % 8;
        
        if (byte_idx < bitmap_size) {
            if (!(bitmap[byte_idx] & (1 << bit_idx))) {
                if (errors) {
                    errors->push_back("Cluster " + std::to_string(i) + 
                                    " should be marked as used but is free");
                }
                bitmap_ok = false;
                
                if (repair) {
                    bitmap[byte_idx] |= (1 << bit_idx);
                }
            }
        }
    }

    // Verify bitmap is not all zeros (would indicate corruption)
    bool all_zero = true;
    for (uint64_t i = 0; i < bitmap_size && i < 1024; i++) {
        if (bitmap[i] != 0) {
            all_zero = false;
            break;
        }
    }
    
    if (all_zero) {
        if (errors) errors->push_back("Bitmap appears to be empty (all zeros)");
        return Result::error("Bitmap may be corrupted");
    }

    // Write repaired bitmap
    if (repair && !bitmap_ok) {
        result = disk->write(bitmap.data(), bitmap_offset, bitmap_size);
        if (result.failed()) {
            if (errors) errors->push_back("Failed to write repaired bitmap");
        }
    }

    return bitmap_ok ? Result::ok() : Result::error("Bitmap check failed");
}

// ============================================================================
// Check Log File
// ============================================================================

Result checkLogFile(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                    const NTFSLayout& layout, bool repair, std::vector<std::string>* errors) {
    // Log file is typically at MFT record 2
    // Calculate expected log file location
    uint64_t log_cluster = layout.mft_lcn + 
        (16 * layout.mft_record_size / layout.bytes_per_cluster) + 2;
    uint64_t log_sector = start_sector + log_cluster * layout.sectors_per_cluster;
    uint64_t log_offset = log_sector * layout.bytes_per_sector;
    
    // Read first few bytes to check signature
    char signature[4];
    Result result = disk->read(signature, log_offset, 4);
    if (result.failed()) {
        if (errors) errors->push_back("Failed to read $LogFile");
        return Result::error("Cannot read log file");
    }

    // Check for restart page signatures
    bool valid_signature = (std::memcmp(signature, "RSTR", 4) == 0 ||
                         std::memcmp(signature, "RCND", 4) == 0 ||
                         std::memcmp(signature, "CHKD", 4) == 0 ||
                         (signature[0] == 0 && signature[1] == 0 && 
                          signature[2] == 0 && signature[3] == 0));
    
    if (!valid_signature) {
        if (errors) {
            std::stringstream ss;
            ss << "Unexpected $LogFile signature: ";
            for (int i = 0; i < 4; i++) {
                ss << std::hex << (unsigned char)signature[i];
            }
            errors->push_back(ss.str());
        }
        
        if (repair) {
            // Initialize with empty restart page
            char init_signature[4] = {'R', 'S', 'T', 'R'};
            disk->write(init_signature, log_offset, 4);
        } else {
            return Result::error("Invalid log file signature");
        }
    }

    return Result::ok();
}

} // namespace ntfs
} // namespace opm
