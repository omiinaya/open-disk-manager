#include "opm/exfat_impl.hpp"
#include "opm/disk_io.hpp"
#include <cstring>

namespace opm {
namespace exfat {

Result checkExFAT(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                  bool repair, std::vector<std::string>* errors) {
    if (errors) errors->clear();
    
    // Step 1: Check boot sector
    Result result = checkExFATBootSector(disk, start_sector, repair, errors);
    if (result.failed()) return result;
    
    // Read boot sector to get layout
    ExFATBootSector boot_sector;
    uint64_t boot_offset = start_sector * 512;
    result = disk->read(&boot_sector, boot_offset, sizeof(ExFATBootSector));
    if (result.failed()) {
        if (errors) errors->push_back("Failed to read boot sector");
        return result;
    }
    
    // Calculate layout
    ExFATLayout layout;
    layout.bytes_per_sector_shift = boot_sector.bs_bytes_per_sector_shift;
    layout.bytes_per_sector = 1 << layout.bytes_per_sector_shift;
    layout.sectors_per_cluster_shift = boot_sector.bs_sectors_per_cluster_shift;
    layout.sectors_per_cluster = 1 << layout.sectors_per_cluster_shift;
    layout.bytes_per_cluster = layout.bytes_per_sector * layout.sectors_per_cluster;
    layout.volume_length = boot_sector.bs_volume_length;
    layout.fat_offset = boot_sector.bs_fat_offset;
    layout.fat_length = boot_sector.bs_fat_length;
    layout.cluster_heap_offset = boot_sector.bs_cluster_heap_offset;
    layout.cluster_count = boot_sector.bs_cluster_count;
    layout.root_cluster = boot_sector.bs_first_cluster_of_root;
    
    if (!layout.validate()) {
        if (errors) errors->push_back("Invalid exFAT layout");
        return Result::error("Invalid layout");
    }
    
    // Step 2: Check FAT
    result = checkExFATFAT(disk, start_sector, layout, repair, errors);
    if (result.failed()) return result;
    
    // Step 3: Check allocation bitmap
    result = checkExFATAllocationBitmap(disk, start_sector, layout, repair, errors);
    if (result.failed()) return result;
    
    // Step 4: Check root directory
    result = checkExFATRootDirectory(disk, start_sector, layout, repair, errors);
    if (result.failed()) return result;
    
    return Result::ok();
}

Result checkExFATBootSector(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                            bool repair, std::vector<std::string>* errors) {
    uint64_t boot_offset = start_sector * 512;
    
    ExFATBootSector boot_sector;
    Result result = disk->read(&boot_sector, boot_offset, sizeof(ExFATBootSector));
    if (result.failed()) {
        if (errors) errors->push_back("Failed to read boot sector");
        return Result::error("Cannot read boot sector");
    }
    
    // Check jump instruction
    if (boot_sector.bs_jmp[0] != EXFAT_BOOT_SIGNATURE) {
        if (errors) errors->push_back("Invalid boot signature");
        if (!repair) return Result::error("Invalid boot signature");
    }
    
    // Check file system name
    if (std::memcmp(boot_sector.bs_file_system_name, EXFAT_FILE_SYSTEM_NAME, 8) != 0) {
        if (errors) errors->push_back("Not an exFAT volume");
        return Result::error("Not exFAT");
    }
    
    // Check boot signature
    if (boot_sector.bs_boot_signature != EXFAT_BOOT_SECTOR_CHECKSUM) {
        if (errors) errors->push_back("Boot signature mismatch");
        if (!repair) return Result::error("Invalid boot signature");
    }
    
    // Validate volume length
    if (boot_sector.bs_volume_length < EXFAT_VOLUME_LENGTH_MIN) {
        if (errors) errors->push_back("Volume too small");
        return Result::error("Volume too small");
    }
    
    return Result::ok();
}

Result checkExFATFAT(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                    const ExFATLayout& layout, bool repair,
                    std::vector<std::string>* errors) {
    uint64_t fat_sector = start_sector + layout.fat_offset;
    uint64_t fat_offset = fat_sector * layout.bytes_per_sector;
    
    uint32_t fat_entries = layout.cluster_count + 2;
    uint64_t fat_size = fat_entries * sizeof(uint32_t);
    std::vector<uint32_t> fat(fat_entries);
    
    Result result = disk->read(fat.data(), fat_offset, fat_size);
    if (result.failed()) {
        if (errors) errors->push_back("Failed to read FAT");
        return Result::error("Cannot read FAT");
    }
    
    // Check cluster 0 (media type)
    if ((fat[0] & 0xFFFFFFF8) != 0xFFFFFFF8) {
        if (errors) errors->push_back("Invalid media type in FAT[0]");
    }
    
    // Check cluster 1 (reserved)
    if (fat[1] != EXFAT_CLUSTER_END) {
        if (errors) errors->push_back("FAT[1] should be end-of-chain marker");
    }
    
    // Check root directory cluster
    if (fat[layout.root_cluster] != EXFAT_CLUSTER_END) {
        if (errors) errors->push_back("Root directory cluster should be end-of-chain");
    }
    
    return Result::ok();
}

Result checkExFATAllocationBitmap(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                                  const ExFATLayout& layout, bool repair,
                                  std::vector<std::string>* errors) {
    uint32_t bitmap_cluster = 3;
    uint64_t bitmap_sector = start_sector + layout.clusterToSector(bitmap_cluster);
    uint64_t bitmap_offset = bitmap_sector * layout.bytes_per_sector;
    
    uint64_t bitmap_size = (layout.cluster_count + 7) / 8;
    std::vector<uint8_t> bitmap(bitmap_size);
    
    Result result = disk->read(bitmap.data(), bitmap_offset, bitmap_size);
    if (result.failed()) {
        if (errors) errors->push_back("Failed to read allocation bitmap");
        return Result::error("Cannot read bitmap");
    }
    
    // Check system clusters are marked as allocated
    bool bitmap_ok = true;
    for (uint32_t cluster = EXFAT_FIRST_DATA_CLUSTER; 
         cluster < EXFAT_FIRST_DATA_CLUSTER + 3 && cluster < layout.cluster_count; 
         cluster++) {
        uint64_t byte_idx = (cluster - EXFAT_FIRST_DATA_CLUSTER) / 8;
        uint64_t bit_idx = (cluster - EXFAT_FIRST_DATA_CLUSTER) % 8;
        
        if (byte_idx < bitmap_size) {
            if (!(bitmap[byte_idx] & (1 << bit_idx))) {
                if (errors) {
                    errors->push_back("Cluster " + std::to_string(cluster) + 
                                    " should be marked as allocated");
                }
                bitmap_ok = false;
                if (repair) bitmap[byte_idx] |= (1 << bit_idx);
            }
        }
    }
    
    // Verify bitmap is not all zeros
    bool all_zero = true;
    for (uint64_t i = 0; i < bitmap_size; i++) {
        if (bitmap[i] != 0) {
            all_zero = false;
            break;
        }
    }
    
    if (all_zero) {
        if (errors) errors->push_back("Allocation bitmap appears to be empty");
        return Result::error("Bitmap may be corrupted");
    }
    
    if (repair && !bitmap_ok) {
        result = disk->write(bitmap.data(), bitmap_offset, bitmap_size);
        if (result.failed()) {
            if (errors) errors->push_back("Failed to write repaired bitmap");
        }
    }
    
    return bitmap_ok ? Result::ok() : Result::error("Bitmap check failed");
}

Result checkExFATRootDirectory(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                               const ExFATLayout& layout, bool repair,
                               std::vector<std::string>* errors) {
    uint64_t root_sector = start_sector + layout.clusterToSector(layout.root_cluster);
    uint64_t root_offset = root_sector * layout.bytes_per_sector;
    
    std::vector<uint8_t> root_dir(layout.bytes_per_cluster);
    Result result = disk->read(root_dir.data(), root_offset, layout.bytes_per_cluster);
    if (result.failed()) {
        if (errors) errors->push_back("Failed to read root directory");
        return Result::error("Cannot read root directory");
    }
    
    bool root_ok = true;
    size_t offset = 0;
    
    while (offset + 32 <= layout.bytes_per_cluster) {
        uint8_t entry_type = root_dir[offset];
        
        if (entry_type == EXFAT_ENTRY_END) break;
        if (entry_type == 0x00) {
            offset += 32;
            continue;
        }
        
        bool valid_entry = false;
        switch (entry_type) {
            case EXFAT_ENTRY_VOLUME_LABEL:
            case EXFAT_ENTRY_ALLOCATION_BITMAP:
            case EXFAT_ENTRY_UPCASE_TABLE:
            case EXFAT_ENTRY_FILE:
                valid_entry = true;
                break;
            default:
                if ((entry_type & 0x80) || (entry_type & 0xC0)) {
                    valid_entry = true;
                }
                break;
        }
        
        if (!valid_entry) {
            if (errors) {
                errors->push_back("Invalid entry type at offset " + std::to_string(offset));
            }
            root_ok = false;
        }
        
        offset += 32;
    }
    
    return root_ok ? Result::ok() : Result::error("Root directory check failed");
}

} // namespace exfat
} // namespace opm
