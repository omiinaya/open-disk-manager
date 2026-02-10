#include "opm/partition.hpp"
#include "opm/utils.hpp"
#include <algorithm>
#include <sstream>

namespace opm {

Partition::Partition() = default;

bool Partition::isAligned(size_t alignment_sectors) const {
    return utils::isAligned(start_sector_, alignment_sectors);
}

bool Partition::isValid() const {
    // Basic validation
    if (start_sector_ > end_sector_) {
        return false;
    }
    
    if (sectorCount() == 0) {
        return false;
    }
    
    return true;
}

bool Partition::overlaps(const Partition& other) const {
    // Two partitions overlap if they share any sectors
    if (end_sector_ < other.start_sector_ || start_sector_ > other.end_sector_) {
        return false;
    }
    return true;
}

std::string Partition::formattedSize() const {
    return utils::formatBytes(sizeBytes());
}

bool Partition::isMounted() const {
    // Platform-specific implementation would check mount points
    // This is a placeholder
    #ifdef __linux__
    // Would parse /proc/mounts or use libmount
    return false;
    #else
    return false;
    #endif
}

std::string Partition::mountPoint() const {
    // Platform-specific implementation
    return "";
}

} // namespace opm
