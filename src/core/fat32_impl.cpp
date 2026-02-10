#include "opm/fat32_impl.hpp"
#include <algorithm>
#include <cstring>

namespace opm {
namespace fat32 {

// ============================================================================
// FAT32BootSector Implementation
// ============================================================================

void FAT32BootSector::init(uint32_t total_sectors, uint32_t sectors_per_fat,
                            uint32_t sectors_per_cluster_val, uint32_t serial_number,
                            const char* label) {
    // Clear the structure
    std::memset(this, 0, sizeof(*this));
    
    // Jump instruction to boot code
    bs_jmp_boot[0] = 0xEB;
    bs_jmp_boot[1] = 0x58;
    bs_jmp_boot[2] = 0x90;
    
    // OEM name
    std::memcpy(bs_oem_name, "MSDOS5.0", 8);
    
    // BPB
    bpb_bytes_per_sector = 512;  // Standard
    bpb_sectors_per_cluster = static_cast<uint8_t>(sectors_per_cluster_val);
    bpb_reserved_sectors = 32; // Standard for FAT32
    bpb_num_fats = 2;
    bpb_root_entries = 0;        // Not used in FAT32
    bpb_total_sectors_16 = 0;    // Use total_sectors_32
    bpb_media_descriptor = 0xF8; // Fixed disk
    bpb_sectors_per_fat_16 = 0;  // Use sectors_per_fat_32
    bpb_sectors_per_track = 63;  // Typical geometry
    bpb_num_heads = 255;         // Typical geometry
    bpb_hidden_sectors = 0;        // Partition start
    bpb_total_sectors_32 = total_sectors;
    
    // FAT32 specific
    bpb_sectors_per_fat_32 = sectors_per_fat;
    bpb_ext_flags = 0x0000;
    bpb_fs_version = 0x0000;
    bpb_root_cluster = ROOT_DIR_CLUSTER;  // Usually 2
    bpb_fs_info_sector = 1;              // FSInfo at sector 1
    bpb_backup_boot_sector = 6;          // Backup at sector 6
    
    // Extended BPB
    bs_drive_number = 0x80;
    bs_reserved1 = 0;
    bs_ext_boot_signature = 0x29;
    bs_volume_serial = serial_number;
    
    // Volume label (padded with spaces)
    std::memset(bs_volume_label, ' ', 11);
    if (label && label[0]) {
        size_t len = std::strlen(label);
        if (len > 11) len = 11;
        std::memcpy(bs_volume_label, label, len);
    }
    
    // File system type
    std::memcpy(bs_file_system_type, "FAT32   ", 8);
    
    // Boot code (minimal - just infinite loop)
    std::memset(bs_boot_code, 0, 420);
    // Simple boot code that just halts
    bs_boot_code[0] = 0xEB;  // jmp short
    bs_boot_code[1] = 0xFE;  // infinite loop
    bs_boot_code[2] = 0x90;  // nop
    
    // Boot signature
    bs_boot_signature = BOOT_SIGNATURE;
}

// ============================================================================
// FAT32FSInfo Implementation
// ============================================================================

void FAT32FSInfo::init(uint32_t free_clusters, uint32_t next_free_hint) {
    fsi_lead_signature = FSINFO_LEAD_SIGNATURE;
    std::memset(fsi_reserved1, 0, 480);
    fsi_struc_signature = FSINFO_STRUC_SIGNATURE;
    fsi_free_count = free_clusters;
    fsi_next_free = next_free_hint;
    std::memset(fsi_reserved2, 0, 12);
    fsi_trail_signature = FSINFO_TRAIL_SIGNATURE;
}

// ============================================================================
// FAT32DirEntry Implementation
// ============================================================================

void FAT32DirEntry::initVolumeLabel(const char* label) {
    clear();
    std::memset(dir_name, ' ', 11);
    if (label && label[0]) {
        size_t len = std::strlen(label);
        if (len > 11) len = 11;
        std::memcpy(dir_name, label, len);
    }
    dir_attr = ATTR_VOLUME_ID;
    dir_nt_reserved = 0;
    dir_crt_time_tenth = 0;
    dir_crt_time = 0;
    dir_crt_date = 0;
    dir_lst_acc_date = 0;
    dir_fst_clus_hi = 0;
    dir_wrt_time = 0;
    dir_wrt_date = 0;
    dir_fst_clus_lo = 0;
    dir_file_size = 0;
}

// ============================================================================
// FAT32LFNEntry Implementation
// ============================================================================

uint8_t FAT32LFNEntry::calculateChecksum(const char* short_name) {
    uint8_t sum = 0;
    for (int i = 0; i < 11; i++) {
        sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + short_name[i];
    }
    return sum;
}

// ============================================================================
// FAT32Layout Implementation
// ============================================================================

void FAT32Layout::calculate(uint64_t volume_size_bytes, uint32_t sector_size) {
    total_size = volume_size_bytes;
    bytes_per_sector = sector_size;
    total_sectors = volume_size_bytes / sector_size;
    
    // Select sectors per cluster based on volume size
    sectors_per_cluster = getSectorsPerCluster(volume_size_bytes, sector_size);
    
    // Reserved sectors (standard is 32 for FAT32)
    reserved_sectors = 32;
    
    // Number of FATs (always 2 for reliability)
    num_fats = 2;
    
    // Root cluster (usually 2)
    root_cluster = ROOT_DIR_CLUSTER;
    
    // FSInfo sector (usually 1, 0 is boot sector)
    fs_info_sector = 1;
    
    // Backup boot sector (usually 6)
    backup_boot_sector = 6;
    
    // Calculate total data sectors
    uint64_t data_sectors = total_sectors - reserved_sectors;
    
    // Calculate clusters
    total_clusters = data_sectors / sectors_per_cluster;
    
    // Ensure we have minimum clusters for FAT32
    if (total_clusters < FAT32_MIN_CLUSTERS) {
        total_clusters = FAT32_MIN_CLUSTERS;
    }
    
    // Ensure we don't exceed maximum
    if (total_clusters > FAT32_MAX_CLUSTERS) {
        total_clusters = FAT32_MAX_CLUSTERS;
    }
    
    // Calculate FAT size in sectors
    // Each FAT entry is 4 bytes, need enough for all clusters
    uint64_t fat_size_bytes = (total_clusters + 2) * 4;  // +2 for entries 0 and 1
    sectors_per_fat = static_cast<uint32_t>((fat_size_bytes + sector_size - 1) / sector_size);
    
    // Recalculate total clusters accounting for FAT space
    uint64_t fat_sectors_total = num_fats * sectors_per_fat;
    data_sectors = total_sectors - reserved_sectors - fat_sectors_total;
    total_clusters = static_cast<uint32_t>(data_sectors / sectors_per_cluster);
    
    // Recalculate FAT size with updated cluster count
    fat_size_bytes = (total_clusters + 2) * 4;
    sectors_per_fat = static_cast<uint32_t>((fat_size_bytes + sector_size - 1) / sector_size);
    
    // Calculate sector positions
    fat_start_sector = reserved_sectors;
    data_start_sector = reserved_sectors + (num_fats * sectors_per_fat);
}

bool FAT32Layout::validate() const {
    if (bytes_per_sector != 512 && bytes_per_sector != 1024 && 
        bytes_per_sector != 2048 && bytes_per_sector != 4096) {
        return false;
    }
    
    if (sectors_per_cluster == 0) {
        return false;
    }
    
    if (num_fats != 2) {
        return false;
    }
    
    if (total_clusters < FAT32_MIN_CLUSTERS) {
        return false;
    }
    
    if (total_clusters > FAT32_MAX_CLUSTERS) {
        return false;
    }
    
    if (data_start_sector <= fat_start_sector) {
        return false;
    }
    
    return true;
}

// ============================================================================
// FAT32 Operations
// ============================================================================

void initBootSector(FAT32BootSector& bs, const FAT32Layout& layout,
                    uint32_t serial, const char* label) {
    bs.init(static_cast<uint32_t>(layout.total_sectors), layout.sectors_per_fat,
            layout.sectors_per_cluster, serial, label);
}

void initFSInfoSector(FAT32FSInfo& fs_info, const FAT32Layout& layout) {
    fs_info.init(layout.total_clusters - 1, 3);  // All clusters free except cluster 2 (root)
}

void initFATTable(std::vector<uint32_t>& fat) {
    // Entry 0: Media descriptor (low byte) + reserved
    fat[0] = 0x0FFFFFF8 | (BOOT_MEDIA_DESCRIPTOR & 0xFF);
    
    // Entry 1: Reserved, dirty flag
    fat[1] = 0x0FFFFFFF;
    
    // Entry 2+: Free (will be set as needed)
    for (size_t i = 2; i < fat.size(); i++) {
        fat[i] = FAT32_FREE;
    }
}

void setFATEntry(std::vector<uint32_t>& fat, uint32_t cluster, uint32_t value) {
    if (cluster >= fat.size()) {
        return;
    }
    fat[cluster] = value & FAT32_EOC_MASK;
}

uint32_t getFATEntry(const std::vector<uint32_t>& fat, uint32_t cluster) {
    if (cluster >= fat.size()) {
        return FAT32_EOC;
    }
    return fat[cluster] & FAT32_EOC_MASK;
}

uint16_t calculateBootSectorChecksum(const FAT32BootSector& bs) {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(&bs);
    uint16_t checksum = 0;
    
    // Sum bytes 0-509 (exclude bytes 510-511 which contain signature)
    for (int i = 0; i < 510; i++) {
        checksum = ((checksum << 15) | (checksum >> 1)) + data[i];
    }
    
    return checksum;
}

// Standalone function to initialize FSInfo with layout
void initFSInfoSector(FAT32FSInfo& fs_info, const FAT32Layout& layout, 
                      uint32_t free_clusters) {
    (void)layout;  // Layout not needed for basic initialization
    fs_info.init(free_clusters, 3);  // Start looking from cluster 3
}

} // namespace fat32
} // namespace opm
