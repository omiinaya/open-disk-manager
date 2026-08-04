#pragma once

#include "types.hpp"
#include <memory>

namespace opm {
class DiskIO;

// Format a partition as Linux swap (mkswap-compatible "SWAPSPACE2" v1 layout).
// Writes the version + magic signature at the end of the first 4096-byte page
// and clears FS signatures from the leading space. Optionally records a
// 16-byte volume label. Linux `swapon` accepts the result.
Result formatSwap(std::shared_ptr<DiskIO> disk,
                  uint64_t start_sector,
                  uint64_t size_bytes,
                  const std::string& label = "");

// Verify a swap signature at the given start sector.
bool isSwap(std::shared_ptr<DiskIO> disk, uint64_t start_sector);

} // namespace opm