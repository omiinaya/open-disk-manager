#include "opm/ntfs_impl.hpp"
#include <cstring>
#include <ctime>
#include <cstdlib>

namespace opm {
namespace ntfs {

// ============================================================================
// NTFSLayout Implementation - Phase 3.4.1
// ============================================================================

void NTFSLayout::calculate(uint64_t volume_size_bytes, uint32_t sector_size) {
    bytes_per_sector = sector_size;
    bytes_per_sector = 512;  // NTFS always uses 512 bytes per sector
    
    // Select sectors per cluster based on volume size
    if (volume_size_bytes < (512 * 1024 * 1024)) {
        sectors_per_cluster = 1;  // 512 bytes for small volumes
    } else if (volume_size_bytes < (1ULL * 1024 * 1024 * 1024)) {
        sectors_per_cluster = 2;  // 1KB
    } else if (volume_size_bytes < (2ULL * 1024 * 1024 * 1024)) {
        sectors_per_cluster = 4;  // 2KB
    } else if (volume_size_bytes < (4ULL * 1024 * 1024 * 1024)) {
        sectors_per_cluster = 8;  // 4KB
    } else if (volume_size_bytes < (16ULL * 1024 * 1024 * 1024)) {
        sectors_per_cluster = 16; // 8KB
    } else if (volume_size_bytes < (32ULL * 1024 * 1024 * 1024)) {
        sectors_per_cluster = 32; // 16KB
    } else if (volume_size_bytes < (64ULL * 1024 * 1024 * 1024)) {
        sectors_per_cluster = 64; // 32KB
    } else {
        sectors_per_cluster = 128; // 64KB
    }
    
    bytes_per_cluster = sectors_per_cluster * bytes_per_sector;
    
    // Calculate total sectors
    total_sectors = volume_size_bytes / bytes_per_sector;
    total_size = volume_size_bytes;
    
    // Calculate total clusters
    total_clusters = total_sectors / sectors_per_cluster;
    
    // Calculate MFT position
    // MFT is typically at cluster 2 (after boot sector and any reserved)
    mft_lcn = 2;
    
    // Calculate MFT mirror position (middle of volume)
    mft_mirr_lcn = total_clusters / 2;
    
    // MFT record size (typically 1024 bytes = 2 sectors)
    // Stored as negative power of 2: -10 = 2^10 = 1024
    clusters_per_mft_record = -10;
    mft_record_size = 1024;
    
    // Index record size (typically 4096 bytes)
    clusters_per_index_record = -12;  // 2^12 = 4096
    index_record_size = 4096;
    
    // Generate serial number
    serial_number = generateNTFSSerial();
}

bool NTFSLayout::validate() const {
    if (bytes_per_sector != 512) {
        return false;
    }
    
    if (sectors_per_cluster == 0 || sectors_per_cluster > 128) {
        return false;
    }
    
    if (total_clusters == 0) {
        return false;
    }
    
    if (mft_lcn == 0 || mft_lcn >= total_clusters) {
        return false;
    }
    
    if (mft_mirr_lcn == 0 || mft_mirr_lcn >= total_clusters) {
        return false;
    }
    
    if (mft_record_size != 256 && mft_record_size != 512 && 
        mft_record_size != 1024 && mft_record_size != 4096) {
        return false;
    }
    
    return true;
}

// ============================================================================
// NTFSBootSector Implementation
// ============================================================================

void NTFSBootSector::init(uint64_t total_sectors_val, uint8_t sectors_per_cluster_val,
                            uint64_t mft_lcn_val, uint64_t mft_mirr_lcn_val, 
                            uint64_t serial) {
    // Clear
    std::memset(this, 0, sizeof(NTFSBootSector));
    
    // Jump instruction
    bs_jmp[0] = 0xEB;
    bs_jmp[1] = 0x52;
    bs_jmp[2] = 0x90;
    
    // OEM ID
    std::memcpy(bs_oem, NTFS_OEM_ID, 8);
    
    // BIOS Parameter Block
    bpb_bytes_per_sector = 512;
    bpb_sectors_per_cluster = sectors_per_cluster_val;
    bpb_reserved_sectors = 0;
    std::memset(bpb_always_zero_0, 0, 3);
    bpb_unused_0 = 0;
    bpb_media_descriptor = 0xF8;  // Fixed disk
    bpb_always_zero_1 = 0;
    bpb_sectors_per_track = 63;
    bpb_number_of_heads = 255;
    bpb_hidden_sectors = 0;
    bpb_unused_1 = 0;
    bpb_unused_2 = 0;
    bpb_total_sectors = total_sectors_val;
    
    // MFT information
    bs_mft_lcn = mft_lcn_val;
    bs_mft_mirr_lcn = mft_mirr_lcn_val;
    bs_clusters_per_mft_record = -10;  // 2^10 = 1024 bytes
    std::memset(bs_reserved_3, 0, 3);
    bs_clusters_per_index_record = -12;  // 2^12 = 4096 bytes
    std::memset(bs_reserved_4, 0, 3);
    
    // Serial number
    bs_volume_serial = serial;
    
    // Checksum (NTFS doesn't use it, but set to 0)
    bs_checksum = 0;
    
    // Boot code
    std::memset(bs_boot_code, 0, 426);
    // Simple boot code that just halts
    bs_boot_code[0] = 0xEB;  // jmp short
    bs_boot_code[1] = 0xFE;  // infinite loop
    bs_boot_code[2] = 0x90;  // nop
    
    // Boot signature
    bs_boot_signature = NTFS_BOOT_SIGNATURE;
}

// ============================================================================
// Utility Functions
// ============================================================================

uint64_t generateNTFSSerial() {
    // Simple random number generation using time and rand
    static bool seeded = false;
    if (!seeded) {
        std::srand(static_cast<unsigned int>(std::time(nullptr)));
        seeded = true;
    }
    uint64_t serial = static_cast<uint64_t>(std::rand());
    serial |= static_cast<uint64_t>(std::rand()) << 16;
    serial |= static_cast<uint64_t>(std::rand()) << 32;
    serial |= static_cast<uint64_t>(std::rand()) << 48;
    return serial;
}

void initBootSector(NTFSBootSector& bs, const NTFSLayout& layout) {
    bs.init(layout.total_sectors, layout.sectors_per_cluster,
            layout.mft_lcn, layout.mft_mirr_lcn, layout.serial_number);
}

uint32_t calculateBootChecksum(const uint8_t* data, uint64_t length) {
    // NTFS boot sector checksum is not actually used
    // but we calculate it anyway for completeness
    uint32_t sum = 0;
    for (size_t i = 0; i < length && i < 512; i++) {
        // Skip the checksum field at offset 0x150-0x153
        if (i >= 0x150 && i < 0x154) {
            continue;
        }
        sum = ((sum << 31) | (sum >> 1)) + data[i];
    }
    return sum;
}

// ============================================================================
// MFT Operations
// ============================================================================

void initMFTRecord(MFTRecordHeader& record, uint64_t record_num, bool is_dir) {
    std::memset(&record, 0, sizeof(MFTRecordHeader));
    
    record.mr_magic = MFT_RECORD_MAGIC;
    record.mr_usn_offset = 48;  // Standard offset for USN array
    record.mr_usn_size = 3;      // Standard size (3 entries: 1 USN + 2 fixups)
    record.mr_lsn = 0;
    record.mr_sequence_number = 1;
    record.mr_hard_link_count = is_dir ? 2 : 1;
    record.mr_attr_offset = 56;  // Standard offset to first attribute
    record.mr_flags = MFT_RECORD_FLAG_IN_USE;
    if (is_dir) {
        record.mr_flags |= MFT_RECORD_FLAG_DIR;
    }
    record.mr_used_size = record.mr_attr_offset + 8;  // Space for attribute header + end marker
    record.mr_alloc_size = 1024;  // Standard MFT record size
    record.mr_base_record = 0;    // Not an extension
    record.mr_next_attr_id = 0;
    record.mr_record_number = static_cast<uint16_t>(record_num);
    record.mr_usn = 0;  // Will be set by fixup
}

} // namespace ntfs
} // namespace opm
