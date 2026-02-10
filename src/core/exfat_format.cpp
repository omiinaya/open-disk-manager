#include "opm/exfat_impl.hpp"
#include "opm/disk_io.hpp"
#include <cstring>

namespace opm {
namespace exfat {

Result formatExFAT(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                   uint64_t size_bytes, const std::string& label) {
    // Calculate layout
    ExFATLayout layout;
    layout.calculate(size_bytes);
    
    if (!layout.validate()) {
        return Result::error("Invalid exFAT layout");
    }
    
    // Step 1: Create boot sector
    Result result = createExFATBootSector(disk, start_sector, layout);
    if (result.failed()) {
        return Result::error("Failed to create boot sector: " + result.message);
    }
    
    // Step 2: Create FAT
    result = createExFATFAT(disk, start_sector, layout);
    if (result.failed()) {
        return Result::error("Failed to create FAT: " + result.message);
    }
    
    // Step 3: Create allocation bitmap
    result = createExFATAllocationBitmap(disk, start_sector, layout);
    if (result.failed()) {
        return Result::error("Failed to create allocation bitmap: " + result.message);
    }
    
    // Step 4: Create up-case table
    result = createExFATUpcaseTable(disk, start_sector, layout);
    if (result.failed()) {
        return Result::error("Failed to create up-case table: " + result.message);
    }
    
    // Step 5: Create root directory
    result = createExFATRootDirectory(disk, start_sector, layout, label);
    if (result.failed()) {
        return Result::error("Failed to create root directory: " + result.message);
    }
    
    // Step 6: Flush changes
    result = disk->flush();
    if (result.failed()) {
        return Result::error("Failed to flush changes: " + result.message);
    }
    
    return Result::ok();
}

} // namespace exfat
} // namespace opm
