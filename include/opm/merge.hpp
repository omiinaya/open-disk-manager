#pragma once

#include "types.hpp"
#include "disk_io.hpp"
#include <memory>
#include <string>

namespace opm {

// ============================================================================
// Merge adjacent partitions
//
// Semantics (matching commercial partition managers):
//   opm merge <device> <numA> <numB> [--into <folder>]
//   - A and B must be adjacent; the LEFT partition survives and grows right.
//   - If B is empty (no filesystem), merge = delete B + grow A (any FS).
//   - If both are FAT32, B's entire tree is copied into a folder on A first
//     (data-preserving merge), then B is removed and A grows.
//   - Any other combination with data in B returns an honest error listing the
//     supported paths — never a silent no-op.
// ============================================================================

struct MergeOptions {
    std::string folder_name;              // target folder for B's contents
};

Result mergePartitions(std::shared_ptr<DiskIO> disk,
                       int number_a, int number_b,
                       const MergeOptions& options = MergeOptions{});

} // namespace opm