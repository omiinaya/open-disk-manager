#include "opm/fat32_impl.hpp"
#include "opm/disk_io.hpp"
#include <cstring>
#include <vector>

namespace opm {
namespace fat32 {

// ============================================================================
// FAT32 FAT Table Operations - Phase 3.2.3
// ============================================================================

Result createFATTables(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                       const FAT32Layout& layout) {
    
    // Calculate number of FAT entries needed
    uint32_t num_entries = layout.total_clusters + 2;  // +2 for entries 0 and 1
    
    // Create FAT buffer
    std::vector<uint32_t> fat(num_entries);
    initFATTable(fat);
    
    // Mark system clusters as needed
    // Cluster 2 is root directory - mark as EOC (End of Chain)
    setFATEntry(fat, 2, FAT32_EOC);
    
    // Write FAT1
    Result result = writeFATTable(disk, start_sector, layout, 0, fat);
    if (result.failed()) {
        return Result::error("Failed to write FAT1: " + result.message);
    }
    
    // Write FAT2 (mirror)
    result = writeFATTable(disk, start_sector, layout, 1, fat);
    if (result.failed()) {
        return Result::error("Failed to write FAT2: " + result.message);
    }
    
    return Result::ok();
}

// ============================================================================
// Write FAT Table to Disk
// ============================================================================

Result writeFATTable(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                     const FAT32Layout& layout, uint32_t fat_num,
                     const std::vector<uint32_t>& fat) {
    
    // Calculate start sector for this FAT
    uint64_t fat_start = start_sector + layout.fat_start_sector + 
                         (static_cast<uint64_t>(fat_num) * layout.sectors_per_fat);
    
    // Calculate size in bytes
    uint64_t fat_size_bytes = fat.size() * sizeof(uint32_t);
    
    // Write FAT data
    const uint8_t* fat_data = reinterpret_cast<const uint8_t*>(fat.data());
    Result result = disk->write(fat_data, fat_start * layout.bytes_per_sector, fat_size_bytes);
    
    if (result.failed()) {
        return result;
    }
    
    return Result::ok();
}

// ============================================================================
// Read FAT Table from Disk
// ============================================================================

Result readFATTable(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                    const FAT32Layout& layout, uint32_t fat_num,
                    std::vector<uint32_t>& fat) {
    
    // Calculate start sector for this FAT
    uint64_t fat_start = start_sector + layout.fat_start_sector + 
                         (static_cast<uint64_t>(fat_num) * layout.sectors_per_fat);
    
    // Calculate number of entries
    uint32_t num_entries = layout.total_clusters + 2;
    
    // Resize vector
    fat.resize(num_entries);
    
    // Read FAT data
    uint64_t fat_size_bytes = num_entries * sizeof(uint32_t);
    uint8_t* fat_data = reinterpret_cast<uint8_t*>(fat.data());
    Result result = disk->read(fat_data, fat_start * layout.bytes_per_sector, fat_size_bytes);
    
    if (result.failed()) {
        return result;
    }
    
    // Convert from little-endian (already stored in native format on x86)
    // On big-endian systems, would need to swap bytes
    
    return Result::ok();
}

// ============================================================================
// Get Next Cluster in Chain
// ============================================================================

uint32_t getNextCluster(const std::vector<uint32_t>& fat, uint32_t cluster) {
    return getFATEntry(fat, cluster);
}

// ============================================================================
// Allocate Cluster
// ============================================================================

Result allocateCluster(std::vector<uint32_t>& fat, uint32_t& allocated_cluster) {
    // Find first free cluster (start from cluster 2)
    for (size_t i = 2; i < fat.size(); i++) {
        if (fat[i] == FAT32_FREE) {
            fat[i] = FAT32_EOC;  // Mark as allocated (end of chain)
            allocated_cluster = static_cast<uint32_t>(i);
            return Result::ok();
        }
    }
    
    return Result::error("No free clusters available");
}

// ============================================================================
// Free Cluster Chain
// ============================================================================

Result freeClusterChain(std::vector<uint32_t>& fat, uint32_t start_cluster) {
    uint32_t current = start_cluster;
    
    while (current < fat.size()) {
        uint32_t next = fat[current];
        fat[current] = FAT32_FREE;
        
        // Check if end of chain
        if (next >= FAT32_EOC_START && next <= FAT32_EOC) {
            break;
        }
        
        // Check if invalid
        if (next >= fat.size() && next < FAT32_EOC_START) {
            return Result::error("Invalid FAT chain");
        }
        
        current = next;
    }
    
    return Result::ok();
}

// ============================================================================
// Verify FAT Tables Match
// ============================================================================

Result verifyFATTables(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                       const FAT32Layout& layout) {
    
    std::vector<uint32_t> fat1, fat2;
    
    // Read FAT1
    Result result = readFATTable(disk, start_sector, layout, 0, fat1);
    if (result.failed()) {
        return Result::error("Failed to read FAT1");
    }
    
    // Read FAT2
    result = readFATTable(disk, start_sector, layout, 1, fat2);
    if (result.failed()) {
        return Result::error("Failed to read FAT2");
    }
    
    // Compare
    if (fat1.size() != fat2.size()) {
        return Result::error("FAT tables have different sizes");
    }
    
    for (size_t i = 0; i < fat1.size(); i++) {
        if (fat1[i] != fat2[i]) {
            return Result::error("FAT tables differ at entry " + std::to_string(i));
        }
    }
    
    return Result::ok();
}

// ============================================================================
// Get Free Cluster Count
// ============================================================================

uint32_t getFreeClusterCount(const std::vector<uint32_t>& fat) {
    uint32_t count = 0;
    for (size_t i = 2; i < fat.size(); i++) {
        if (fat[i] == FAT32_FREE) {
            count++;
        }
    }
    return count;
}

// ============================================================================
// Extend Chain
// ============================================================================

Result extendChain(std::vector<uint32_t>& fat, uint32_t end_cluster, 
                   uint32_t new_cluster) {
    if (end_cluster >= fat.size()) {
        return Result::error("Invalid cluster number");
    }
    
    // Set new cluster as end of chain
    fat[new_cluster] = FAT32_EOC;
    
    // Update previous end cluster to point to new cluster
    fat[end_cluster] = new_cluster;
    
    return Result::ok();
}

} // namespace fat32
} // namespace opm
