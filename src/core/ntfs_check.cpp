#include "opm/ntfs_impl.hpp"
#include "opm/disk_io.hpp"
#include <algorithm>
#include <cstring>
#include <sstream>

namespace opm {
namespace ntfs {

namespace {

uint16_t rd16(const uint8_t* p) { return p[0] | (p[1] << 8); }
uint32_t rd32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (uint32_t(p[3]) << 24);
}
uint64_t rd64(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v |= uint64_t(p[i]) << (8 * i);
    return v;
}

// Parse a non-resident attribute's run list from a raw MFT record.
// Returns the first LCN and the total length in clusters when a run list of
// the given attribute type exists and is non-resident.
bool readFirstRun(const std::vector<uint8_t>& rec, uint32_t attr_type,
                  uint64_t& first_lcn, uint64_t& total_clusters) {
    if (rec.size() < 56) return false;
    if (rd32(rec.data()) != MFT_RECORD_MAGIC) return false;
    uint16_t attr_off = rd16(rec.data() + 20);
    if (attr_off == 0 || attr_off >= rec.size()) return false;
    size_t off = attr_off;
    while (off + 8 <= rec.size()) {
        uint32_t t = rd32(rec.data() + off);
        if (t == 0xFFFFFFFFu) break;
        uint32_t len = rd32(rec.data() + off + 4);
        if (len == 0 || off + len > rec.size()) break;
        const uint8_t* h = rec.data() + off;
        // candidate attribute name (for $Bitmap/$LogFile matching by attr id)
        if (t == attr_type && (h[8] & 1) && len > 64) {
            uint16_t runoff = rd16(h + 32);
            if (runoff + 1 <= len) {
                size_t p = off + runoff;
                int64_t prev = 0;
                if (p < rec.size()) {
                    uint8_t hdr = rec[p++];
                    if (hdr) {
                        int llen = hdr >> 4, olen = hdr & 0x0F;
                        if (llen > 0 && p + llen + olen <= rec.size()) {
                            uint64_t run_len = 0;
                            for (int i = 0; i < llen; i++)
                                run_len |= uint64_t(rec[p + i]) << (8 * i);
                            int64_t offs = 0;
                            for (int i = 0; i < olen; i++)
                                offs |= int64_t(rec[p + llen + i]) << (8 * i);
                            if (olen && (rec[p + llen + olen - 1] & 0x80))
                                for (int i = olen; i < 8; i++)
                                    offs |= int64_t(-1) << (8 * i);
                            prev = offs;
                            first_lcn = uint64_t(prev);
                            total_clusters = run_len;
                            return true;
                        }
                    }
                }
            }
        }
        off += len;
    }
    return false;
}

// Read one MFT record's raw bytes.
std::vector<uint8_t> resolveMFTRecord(std::shared_ptr<DiskIO> disk,
                                      uint64_t start_sector,
                                      const NTFSLayout& layout,
                                      uint64_t record_num) {
    uint64_t rec_bytes = (start_sector + layout.mft_lcn * layout.sectors_per_cluster) *
                         layout.bytes_per_sector + record_num * layout.mft_record_size;
    std::vector<uint8_t> rec(layout.mft_record_size, 0);
    if (disk->read(rec.data(), rec_bytes, layout.mft_record_size).failed()) {
        rec.clear();
    }
    return rec;
}

// Resolve where a system file lives: from its MFT record's $DATA run list
// when present (the real NTFS behaviour), else from the fixed-geometry
// helpers used by the format path (records have no run lists yet).
uint64_t resolveSystemLcn(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                          const NTFSLayout& layout, uint64_t record_num,
                          uint64_t fallback_cluster) {
    std::vector<uint8_t> rec =
        resolveMFTRecord(disk, start_sector, layout, record_num);
    uint64_t first_lcn = 0, total_cl = 0;
    if (readFirstRun(rec, ATTR_DATA, first_lcn, total_cl)) {
        return first_lcn;
    }
    return fallback_cluster;
}

} // anonymous namespace

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
    // Resolve $Bitmap location from the MFT record's $DATA run list (real
    // NTFS behaviour) when present; fall back to the fixed geometry used by
    // the format path (whose system records carry no run lists yet).
    uint64_t bitmap_cluster = resolveSystemLcn(disk, start_sector, layout,
                                               MFT_BITMAP, getBitmapCluster(layout));
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

    // Verify the clusters actually claimed by the system files are marked
    // used. This works for both layouts: the format path (fixed geometry,
    // contiguous prefix) and the conversion path (system files may sit in the
    // middle of the volume, leaving the old FAT region legitimately free).
    auto isSet = [&](uint64_t c) {
        uint64_t byte_idx = c / 8;
        uint64_t bit_idx = c % 8;
        if (byte_idx >= bitmap.size()) return true;  // beyond volume = n/a
        return (bitmap[byte_idx] & (1 << bit_idx)) != 0;
    };

    bool bitmap_ok = true;
    std::vector<uint64_t> must_be_used;

    // Boot region: first 16 sectors hold the boot sector + boot code.
    uint64_t boot_clusters = 16 / layout.sectors_per_cluster;
    if (boot_clusters < 1) boot_clusters = 1;
    for (uint64_t c = 0; c < boot_clusters && c < layout.total_clusters; c++) {
        must_be_used.push_back(c);
    }
    // MFT itself: from run list when present, else fixed prefix.
    {
        uint64_t mft_lcn = 0, mft_cl = 0;
        std::vector<uint8_t> rec =
            resolveMFTRecord(disk, start_sector, layout, MFT_MFT);
        if (readFirstRun(rec, ATTR_DATA, mft_lcn, mft_cl)) {
            for (uint64_t c = mft_lcn; c < mft_lcn + mft_cl && c < layout.total_clusters; c++) {
                must_be_used.push_back(c);
            }
        } else {
            uint64_t used = layout.mft_lcn +
                (16 * layout.mft_record_size / layout.bytes_per_cluster);
            for (uint64_t c = layout.mft_lcn; c < used && c < layout.total_clusters; c++) {
                must_be_used.push_back(c);
            }
        }
    }
    // $Bitmap itself
    {
        uint64_t bmp_bytes = (layout.total_clusters + 7) / 8;
        uint64_t bmp_cl = (bmp_bytes + layout.bytes_per_cluster - 1) / layout.bytes_per_cluster;
        if (bmp_cl < 1) bmp_cl = 1;
        for (uint64_t c = bitmap_cluster; c < bitmap_cluster + bmp_cl && c < layout.total_clusters; c++) {
            must_be_used.push_back(c);
        }
    }
    // $LogFile
    {
        uint64_t log_lcn = 0, log_cl = 0;
        std::vector<uint8_t> rec =
            resolveMFTRecord(disk, start_sector, layout, MFT_LOGFILE);
        if (readFirstRun(rec, ATTR_DATA, log_lcn, log_cl)) {
            for (uint64_t c = log_lcn; c < log_lcn + log_cl && c < layout.total_clusters; c++) {
                must_be_used.push_back(c);
            }
        }
    }
    // $UpCase
    {
        uint64_t uc_lcn = 0, uc_cl = 0;
        std::vector<uint8_t> rec =
            resolveMFTRecord(disk, start_sector, layout, MFT_UPCASE);
        if (readFirstRun(rec, ATTR_DATA, uc_lcn, uc_cl)) {
            for (uint64_t c = uc_lcn; c < uc_lcn + uc_cl && c < layout.total_clusters; c++) {
                must_be_used.push_back(c);
            }
        }
    }
    // MFT mirror
    {
        uint64_t mirr_cl = (4 * layout.mft_record_size + layout.bytes_per_cluster - 1) /
                           layout.bytes_per_cluster;
        if (mirr_cl < 1) mirr_cl = 1;
        for (uint64_t c = layout.mft_mirr_lcn; c < layout.mft_mirr_lcn + mirr_cl &&
             c < layout.total_clusters; c++) {
            must_be_used.push_back(c);
        }
    }

    // Deduplicate and verify.
    std::sort(must_be_used.begin(), must_be_used.end());
    must_be_used.erase(std::unique(must_be_used.begin(), must_be_used.end()),
                       must_be_used.end());
    for (uint64_t c : must_be_used) {
        if (!isSet(c)) {
            if (errors) {
                errors->push_back("Cluster " + std::to_string(c) +
                                " should be marked as used but is free");
            }
            bitmap_ok = false;
            if (repair) {
                uint64_t byte_idx = c / 8;
                uint64_t bit_idx = c % 8;
                if (byte_idx < bitmap.size()) {
                    bitmap[byte_idx] |= static_cast<uint8_t>(1 << bit_idx);
                }
            }
        }
    }

    // Verify bitmap is not entirely empty (would indicate corruption).
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
    // Log file is at MFT record 2. Resolve from the run list when present
    // (converted volumes), else the fixed position used by the format path.
    uint64_t log_cluster = resolveSystemLcn(disk, start_sector, layout,
                                            MFT_LOGFILE,
                                            getLogFileCluster(layout));
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
