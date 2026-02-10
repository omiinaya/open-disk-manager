#include "opm/ntfs_impl.hpp"
#include "opm/disk_io.hpp"
#include <cstring>

namespace opm {
namespace ntfs {

// ============================================================================
// NTFS Complete Format - Phase 3.4.6
// ============================================================================

Result formatNTFS(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                   uint64_t size_bytes, const std::string& label) {
    
    // Step 1: Calculate layout
    NTFSLayout layout;
    layout.calculate(size_bytes);
    
    if (!layout.validate()) {
        return Result::error("Invalid NTFS layout");
    }
    
    // Step 2: Create boot sector
    Result result = createNTFSBootSector(disk, start_sector, layout);
    if (result.failed()) {
        return Result::error("Failed to create boot sector: " + result.message);
    }
    
    // Step 3: Create MFT (Master File Table)
    result = createMFT(disk, start_sector, layout);
    if (result.failed()) {
        return Result::error("Failed to create MFT: " + result.message);
    }
    
    // Step 4: Create system files
    result = createSystemFiles(disk, start_sector, layout);
    if (result.failed()) {
        return Result::error("Failed to create system files: " + result.message);
    }
    
    // Step 5: Create root directory
    result = createRootDirectory(disk, start_sector, layout, label);
    if (result.failed()) {
        return Result::error("Failed to create root directory: " + result.message);
    }
    
    // Step 6: Flush all changes
    result = disk->flush();
    if (result.failed()) {
        return Result::error("Failed to flush changes: " + result.message);
    }
    
    return Result::ok();
}

} // namespace ntfs
} // namespace opm
