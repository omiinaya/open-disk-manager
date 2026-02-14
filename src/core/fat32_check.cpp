#include "opm/fat32_impl.hpp"
#include "opm/disk_io.hpp"
#include <cstring>
#include <vector>

namespace opm {
namespace fat32 {

// ============================================================================
// FAT32 Check - Phase 3.2.7
// ============================================================================

Result checkFAT32(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                  bool repair, std::vector<std::string>* errors) {
    
    if (errors) {
        errors->clear();
    }
    
    std::vector<std::string> local_errors;
    std::vector<std::string>* err = errors ? errors : &local_errors;
    
    // Step 1: Check boot sector
    Result result = checkBootSector(disk, start_sector, repair, err);
    if (result.failed() && !repair) {
        return result;
    }
    
    // Step 2: Get filesystem info
    FAT32BootSector boot_sector;
    FAT32Layout layout;
    result = getFAT32Info(disk, start_sector, boot_sector, layout);
    if (result.failed()) {
        err->push_back("Failed to read FAT32 info: " + result.message);
        return result;
    }
    
    // Step 3: Check FSInfo sector
    result = checkFSInfo(disk, start_sector, layout, repair, err);
    if (result.failed() && !repair) {
        return result;
    }
    
    // Step 4: Check FAT tables
    result = checkFATTables(disk, start_sector, layout, repair, err);
    if (result.failed() && !repair) {
        return result;
    }
    
    // Step 5: Check root directory
    result = checkRootDirectory(disk, start_sector, layout, repair, err);
    if (result.failed() && !repair) {
        return result;
    }
    
    // Return error if any issues found and not repairing
    if (!repair && !err->empty()) {
        return Result::error("FAT32 check found " + std::to_string(err->size()) + " errors");
    }
    
    return Result::ok();
}

// ============================================================================
// Check Boot Sector
// ============================================================================

Result checkBootSector(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                       bool repair, std::vector<std::string>* errors) {
    
    FAT32BootSector boot_sector;
    Result result = disk->readSector(&boot_sector, start_sector);
    if (result.failed()) {
        errors->push_back("Failed to read boot sector");
        return result;
    }
    
    // Check jump instruction
    if (boot_sector.bs_jmp_boot[0] != 0xEB || 
        boot_sector.bs_jmp_boot[2] != 0x90) {
        errors->push_back("Invalid jump instruction in boot sector");
    }
    
    // Check boot signature
    if (boot_sector.bs_boot_signature != BOOT_SIGNATURE) {
        errors->push_back("Invalid boot sector signature (expected 0xAA55)");
        if (repair) {
            boot_sector.bs_boot_signature = BOOT_SIGNATURE;
            disk->writeSector(&boot_sector, start_sector);
            errors->push_back("  -> Fixed: Set boot signature");
        }
    }
    
    // Check extended boot signature
    if (boot_sector.bs_ext_boot_signature != 0x29) {
        errors->push_back("Invalid extended boot signature (expected 0x29)");
    }
    
    // Check file system type
    if (std::memcmp(boot_sector.bs_file_system_type, "FAT32   ", 8) != 0) {
        errors->push_back("Invalid filesystem type (expected 'FAT32')");
    }
    
    // Check bytes per sector
    uint16_t bytes_per_sector = boot_sector.bpb_bytes_per_sector;
    if (bytes_per_sector != 512 && bytes_per_sector != 1024 && 
        bytes_per_sector != 2048 && bytes_per_sector != 4096) {
        errors->push_back("Invalid bytes per sector: " + std::to_string(bytes_per_sector));
    }
    
    // Check sectors per cluster
    uint8_t sectors_per_cluster = boot_sector.bpb_sectors_per_cluster;
    if (sectors_per_cluster == 0 || (sectors_per_cluster & (sectors_per_cluster - 1)) != 0) {
        errors->push_back("Invalid sectors per cluster: " + std::to_string(sectors_per_cluster));
    }
    
    // Check number of FATs
    if (boot_sector.bpb_num_fats != 2) {
        errors->push_back("Invalid number of FATs: " + std::to_string(boot_sector.bpb_num_fats));
    }
    
    // Check root cluster
    if (boot_sector.bpb_root_cluster < 2) {
        errors->push_back("Invalid root cluster: " + std::to_string(boot_sector.bpb_root_cluster));
    }
    
    return errors->empty() ? Result::ok() : Result::error("Boot sector has errors");
}

// ============================================================================
// Check FSInfo Sector
// ============================================================================

Result checkFSInfo(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                   const FAT32Layout& layout, bool repair,
                   std::vector<std::string>* errors) {
    
    FAT32FSInfo fs_info;
    Result result = disk->readSector(&fs_info, start_sector + layout.fs_info_sector);
    if (result.failed()) {
        errors->push_back("Failed to read FSInfo sector");
        return result;
    }
    
    // Check lead signature
    if (fs_info.fsi_lead_signature != FSINFO_LEAD_SIGNATURE) {
        errors->push_back("Invalid FSInfo lead signature");
        if (repair) {
            fs_info.fsi_lead_signature = FSINFO_LEAD_SIGNATURE;
        }
    }
    
    // Check structure signature
    if (fs_info.fsi_struc_signature != FSINFO_STRUC_SIGNATURE) {
        errors->push_back("Invalid FSInfo structure signature");
        if (repair) {
            fs_info.fsi_struc_signature = FSINFO_STRUC_SIGNATURE;
        }
    }
    
    // Check trail signature
    if (fs_info.fsi_trail_signature != FSINFO_TRAIL_SIGNATURE) {
        errors->push_back("Invalid FSInfo trail signature");
        if (repair) {
            fs_info.fsi_trail_signature = FSINFO_TRAIL_SIGNATURE;
        }
    }
    
    // If repairing and we made changes, write back
    if (repair) {
        disk->writeSector(&fs_info, start_sector + layout.fs_info_sector);
    }
    
    return errors->empty() ? Result::ok() : Result::error("FSInfo has errors");
}

// ============================================================================
// Check FAT Tables
// ============================================================================

Result checkFATTables(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                      const FAT32Layout& layout, bool repair,
                      std::vector<std::string>* errors) {
    
    std::vector<uint32_t> fat1, fat2;
    
    // Read FAT1
    Result result = readFATTable(disk, start_sector, layout, 0, fat1);
    if (result.failed()) {
        errors->push_back("Failed to read FAT1");
        return result;
    }
    
    // Read FAT2
    result = readFATTable(disk, start_sector, layout, 1, fat2);
    if (result.failed()) {
        errors->push_back("Failed to read FAT2");
        return result;
    }
    
    // Check FAT1 == FAT2
    if (fat1.size() != fat2.size()) {
        errors->push_back("FAT tables have different sizes");
    } else {
        size_t mismatch_count = 0;
        for (size_t i = 0; i < fat1.size(); i++) {
            if (fat1[i] != fat2[i]) {
                mismatch_count++;
                if (mismatch_count <= 3) {  // Report first 3 mismatches
                    errors->push_back("FAT mismatch at cluster " + std::to_string(i) + 
                                    ": FAT1=" + std::to_string(fat1[i]) + 
                                    ", FAT2=" + std::to_string(fat2[i]));
                }
            }
        }
        
        if (mismatch_count > 3) {
            errors->push_back("... and " + std::to_string(mismatch_count - 3) + " more mismatches");
        }
        
        if (mismatch_count > 0 && repair) {
            // Copy FAT1 to FAT2
            result = writeFATTable(disk, start_sector, layout, 1, fat1);
            if (result.success()) {
                errors->push_back("  -> Fixed: Synchronized FAT2 with FAT1");
            }
        }
    }
    
    // Check reserved entries
    if (fat1[0] != (0x0FFFFFF8 | (BOOT_MEDIA_DESCRIPTOR & 0xFF))) {
        errors->push_back("Invalid FAT entry 0 (media descriptor)");
    }
    
    if (fat1[1] != 0x0FFFFFFF) {
        errors->push_back("Invalid FAT entry 1 (reserved)");
    }
    
    // Check root cluster
    uint32_t root_entry = fat1[layout.root_cluster];
    if (root_entry != FAT32_EOC && 
        !(root_entry >= FAT32_EOC_START && root_entry <= FAT32_EOC)) {
        errors->push_back("Root cluster not marked as end of chain");
    }
    
    // Check for invalid entries
    uint32_t invalid_count = 0;
    for (size_t i = 2; i < fat1.size(); i++) {
        uint32_t entry = fat1[i];
        if (entry != FAT32_FREE && 
            !(entry >= 2 && entry < fat1.size()) &&
            !(entry >= FAT32_EOC_START && entry <= FAT32_EOC) &&
            entry != FAT32_BAD_CLUSTER) {
            invalid_count++;
        }
    }
    
    if (invalid_count > 0) {
        errors->push_back("Found " + std::to_string(invalid_count) + " invalid FAT entries");
    }
    
    return errors->empty() ? Result::ok() : Result::error("FAT tables have errors");
}

// ============================================================================
// Check Root Directory
// ============================================================================

Result checkRootDirectory(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                          const FAT32Layout& layout, [[maybe_unused]] bool repair,
                          std::vector<std::string>* errors) {
    
    // Read root directory cluster
    uint64_t root_sector = start_sector + layout.clusterToSector(layout.root_cluster);
    
    std::vector<uint8_t> sector_data(layout.bytes_per_sector);
    Result result = disk->read(sector_data.data(), 
                               root_sector * layout.bytes_per_sector,
                               layout.bytes_per_sector);
    if (result.failed()) {
        errors->push_back("Failed to read root directory");
        return result;
    }
    
    // Check first entry
    FAT32DirEntry* entries = reinterpret_cast<FAT32DirEntry*>(sector_data.data());
    
    // Volume label should be first if present
    if (!entries[0].isUnused()) {
        if (entries[0].isVolumeLabel()) {
            // Valid volume label
        } else if (entries[0].isLFN()) {
            // Long file name entry
        } else {
            // Regular file/directory
        }
    }
    
    // Scan for orphaned entries
    int used_count = 0;
    int deleted_count = 0;
    
    for (size_t i = 0; i < layout.bytes_per_sector / sizeof(FAT32DirEntry); i++) {
        if (!entries[i].isUnused()) {
            if (entries[i].isDeleted()) {
                deleted_count++;
            } else {
                used_count++;
                
                // Check cluster range
                if (!entries[i].isLFN() && !entries[i].isVolumeLabel()) {
                    uint32_t cluster = entries[i].getCluster();
                    if (cluster >= layout.total_clusters && cluster != 0) {
                        errors->push_back("Entry " + std::to_string(i) + 
                                        " has invalid cluster " + std::to_string(cluster));
                    }
                }
            }
        }
    }
    
    return Result::ok();
}

// ============================================================================
// Check Cluster Chain
// ============================================================================

Result checkClusterChain(const std::vector<uint32_t>& fat, uint32_t start_cluster,
                         std::vector<std::string>* errors) {
    
    std::vector<bool> visited(fat.size(), false);
    uint32_t current = start_cluster;
    uint32_t chain_length = 0;
    const uint32_t max_chain = 0x0FFFFFFF;  // Prevent infinite loops
    
    while (current < fat.size() && chain_length < max_chain) {
        if (visited[current]) {
            errors->push_back("Cluster chain loop detected at cluster " + 
                            std::to_string(current));
            return Result::error("Cluster chain loop");
        }
        
        visited[current] = true;
        chain_length++;
        
        uint32_t next = fat[current];
        
        // Check if end of chain
        if (next >= FAT32_EOC_START && next <= FAT32_EOC) {
            return Result::ok();
        }
        
        // Check if bad cluster
        if (next == FAT32_BAD_CLUSTER) {
            errors->push_back("Bad cluster in chain at cluster " + 
                            std::to_string(current));
            return Result::error("Bad cluster in chain");
        }
        
        // Check if valid next cluster
        if (next != FAT32_FREE && next >= 2 && next < fat.size()) {
            current = next;
        } else {
            errors->push_back("Invalid cluster reference: " + std::to_string(next));
            return Result::error("Invalid cluster reference");
        }
    }
    
    return Result::ok();
}

// ============================================================================
// Get Free Cluster Count (from actual FAT)
// ============================================================================

Result getActualFreeClusterCount(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                                  const FAT32Layout& layout, uint32_t& free_count) {
    
    std::vector<uint32_t> fat;
    Result result = readFATTable(disk, start_sector, layout, 0, fat);
    if (result.failed()) {
        return result;
    }
    
    free_count = 0;
    for (size_t i = 2; i < fat.size(); i++) {
        if (fat[i] == FAT32_FREE) {
            free_count++;
        }
    }
    
    return Result::ok();
}

// ============================================================================
// Repair FAT32
// ============================================================================

Result repairFAT32(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                   std::vector<std::string>* fixes) {
    
    if (fixes) {
        fixes->clear();
    }
    
    std::vector<std::string> local_fixes;
    std::vector<std::string>* fx = fixes ? fixes : &local_fixes;
    
    // Run check with repair enabled
    Result result = checkFAT32(disk, start_sector, true, fx);
    
    // Flush changes
    disk->flush();
    
    return result;
}

} // namespace fat32
} // namespace opm
