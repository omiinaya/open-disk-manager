#include "opm/fat32_impl.hpp"
#include "opm/disk_io.hpp"
#include <cstring>
#include <ctime>
#include <vector>

namespace opm {
namespace fat32 {

// ============================================================================
// FAT32 FSInfo Sector Operations - Phase 3.2.4
// ============================================================================

Result createFSInfoSector(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                          const FAT32Layout& layout, uint32_t free_clusters) {
    
    // Create FSInfo sector
    FAT32FSInfo fs_info;
    initFSInfoSector(fs_info, layout, free_clusters);
    
    // Write FSInfo sector (sector 1)
    Result result = disk->writeSector(&fs_info, start_sector + layout.fs_info_sector);
    if (result.failed()) {
        return Result::error("Failed to write FSInfo sector: " + result.message);
    }
    
    return Result::ok();
}

Result updateFSInfo(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                    const FAT32Layout& layout, uint32_t free_clusters, 
                    uint32_t next_free_hint) {
    
    // Read existing FSInfo
    FAT32FSInfo fs_info;
    Result result = disk->readSector(&fs_info, start_sector + layout.fs_info_sector);
    if (result.failed()) {
        return result;
    }
    
    // Update fields
    fs_info.fsi_free_count = free_clusters;
    fs_info.fsi_next_free = next_free_hint;
    
    // Write back
    result = disk->writeSector(&fs_info, start_sector + layout.fs_info_sector);
    if (result.failed()) {
        return result;
    }
    
    return Result::ok();
}

Result readFSInfo(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                  const FAT32Layout& layout, FAT32FSInfo& fs_info) {
    
    return disk->readSector(&fs_info, start_sector + layout.fs_info_sector);
}

// ============================================================================
// FAT32 Root Directory - Phase 3.2.5
// ============================================================================

Result createRootDirectory(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                          const FAT32Layout& layout, const std::string& label) {
    
    // Root directory is at cluster 2
    uint32_t root_cluster = layout.root_cluster;
    
    // Calculate sector for root cluster
    uint64_t root_sector = start_sector + layout.clusterToSector(root_cluster);
    
    // Create directory entries
    std::vector<uint8_t> sector_data(layout.bytes_per_sector, 0);
    
    // Add volume label entry if provided
    if (!label.empty()) {
        FAT32DirEntry* entries = reinterpret_cast<FAT32DirEntry*>(sector_data.data());
        
        // Volume label entry (first entry)
        entries[0].initVolumeLabel(label.c_str());
        
        // Set timestamps
        time_t now = std::time(nullptr);
        tm* timeinfo = std::localtime(&now);
        
        uint16_t fat_date = ((timeinfo->tm_year - 80) << 9) | 
                           ((timeinfo->tm_mon + 1) << 5) | 
                           timeinfo->tm_mday;
        uint16_t fat_time = (timeinfo->tm_hour << 11) | 
                           (timeinfo->tm_min << 5) | 
                           (timeinfo->tm_sec / 2);
        
        entries[0].dir_crt_date = fat_date;
        entries[0].dir_crt_time = fat_time;
        entries[0].dir_lst_acc_date = fat_date;
        entries[0].dir_wrt_date = fat_date;
        entries[0].dir_wrt_time = fat_time;
    }
    
    // Write root directory cluster
    Result result = disk->write(sector_data.data(), 
                                 root_sector * layout.bytes_per_sector, 
                                 layout.bytes_per_sector);
    if (result.failed()) {
        return Result::error("Failed to write root directory: " + result.message);
    }
    
    // Mark root cluster as used in FAT (already done in createFATTables)
    
    return Result::ok();
}

Result createDirectoryEntry(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                              const FAT32Layout& layout, uint32_t parent_cluster,
                              const std::string& name, uint8_t attr,
                              uint32_t cluster, uint32_t size) {
    
    // Find first available entry in parent directory
    uint64_t parent_sector = start_sector + layout.clusterToSector(parent_cluster);
    
    // Read directory sector
    std::vector<uint8_t> sector_data(layout.bytes_per_sector);
    Result result = disk->read(sector_data.data(), 
                               parent_sector * layout.bytes_per_sector,
                               layout.bytes_per_sector);
    if (result.failed()) {
        return result;
    }
    
    // Find free entry
    FAT32DirEntry* entries = reinterpret_cast<FAT32DirEntry*>(sector_data.data());
    int entry_index = -1;
    
    for (size_t i = 0; i < layout.bytes_per_sector / sizeof(FAT32DirEntry); i++) {
        if (entries[i].isUnused() || entries[i].isDeleted()) {
            entry_index = i;
            break;
        }
    }
    
    if (entry_index < 0) {
        return Result::error("No free directory entries");
    }
    
    // Create short name (8.3 format)
    FAT32DirEntry& entry = entries[entry_index];
    entry.clear();
    
    // Convert name to 8.3 format
    std::string short_name = createShortName(name);
    std::memcpy(entry.dir_name, short_name.c_str(), 11);
    
    // Set attributes
    entry.dir_attr = attr;
    entry.setCluster(cluster);
    entry.dir_file_size = size;
    
    // Set timestamps
    time_t now = std::time(nullptr);
    tm* timeinfo = std::localtime(&now);
    
    uint16_t fat_date = ((timeinfo->tm_year - 80) << 9) | 
                       ((timeinfo->tm_mon + 1) << 5) | 
                       timeinfo->tm_mday;
    uint16_t fat_time = (timeinfo->tm_hour << 11) | 
                       (timeinfo->tm_min << 5) | 
                       (timeinfo->tm_sec / 2);
    
    entry.dir_crt_date = fat_date;
    entry.dir_crt_time = fat_time;
    entry.dir_lst_acc_date = fat_date;
    entry.dir_wrt_date = fat_date;
    entry.dir_wrt_time = fat_time;
    
    // Write back
    result = disk->write(sector_data.data(), 
                         parent_sector * layout.bytes_per_sector,
                         layout.bytes_per_sector);
    if (result.failed()) {
        return result;
    }
    
    return Result::ok();
}

std::string createShortName(const std::string& long_name) {
    std::string short_name;
    short_name.resize(11, ' ');  // 8.3 format, padded with spaces
    
    // Find extension
    size_t dot_pos = long_name.find_last_of('.');
    std::string name_part = (dot_pos != std::string::npos) ? 
                             long_name.substr(0, dot_pos) : long_name;
    std::string ext_part = (dot_pos != std::string::npos) ? 
                            long_name.substr(dot_pos + 1) : "";
    
    // Convert name to uppercase and limit to 8 chars
    for (size_t i = 0; i < name_part.size() && i < 8; i++) {
        char c = std::toupper(name_part[i]);
        // Valid chars for short name
        if (std::isalnum(c) || c == '_' || c == '$' || c == '%' || 
            c == '@' || c == '~' || c == '`' || c == '!' || 
            c == '(' || c == ')' || c == '-' || c == '{' || 
            c == '}' || c == '^' || c == '#' || c == '&') {
            short_name[i] = c;
        }
    }
    
    // Convert extension to uppercase and limit to 3 chars
    for (size_t i = 0; i < ext_part.size() && i < 3; i++) {
        char c = std::toupper(ext_part[i]);
        if (std::isalnum(c)) {
            short_name[8 + i] = c;
        }
    }
    
    return short_name;
}

// ============================================================================
// Complete FAT32 Format - Phase 3.2.6
// ============================================================================

Result formatFAT32Complete(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                           uint64_t size_bytes, const std::string& label) {
    
    // Step 1: Calculate layout
    FAT32Layout layout;
    layout.calculate(size_bytes);
    
    if (!layout.validate()) {
        return Result::error("Invalid FAT32 layout");
    }
    
    // Step 2: Generate serial number
    uint32_t serial = generateFAT32Serial();
    
    // Step 3: Create boot sector
    Result result = writeFAT32BootSector(disk, start_sector, layout, serial, label);
    if (result.failed()) {
        return result;
    }
    
    // Step 4: Create FAT tables
    result = createFATTables(disk, start_sector, layout);
    if (result.failed()) {
        return result;
    }
    
    // Step 5: Create FSInfo sector
    uint32_t free_clusters = layout.total_clusters - 1;  // -1 for root cluster
    result = createFSInfoSector(disk, start_sector, layout, free_clusters);
    if (result.failed()) {
        return result;
    }
    
    // Step 6: Create root directory
    result = createRootDirectory(disk, start_sector, layout, label);
    if (result.failed()) {
        return result;
    }
    
    // Step 7: Flush changes
    result = disk->flush();
    if (result.failed()) {
        return result;
    }
    
    return Result::ok();
}

} // namespace fat32
} // namespace opm
