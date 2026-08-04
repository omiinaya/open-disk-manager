#pragma once

#include "types.hpp"
#include <memory>
#include <string>
#include <vector>

namespace opm {
class DiskIO;

namespace fat32 {

// A deleted file discovered in a FAT32 directory scan.
struct DeletedFile {
    std::string name;           // 8.3 name with the first char replaced by '_'
    uint32_t start_cluster = 0; // First cluster from the directory entry
    uint32_t size_bytes = 0;    // Original file size
    uint32_t parent_cluster = 0;// 0 = root directory
    uint64_t entry_offset = 0;  // Absolute byte offset of the 0xE5 entry
    bool has_lfn = false;
};

// Scan a FAT32 volume for deleted files (short-name entries whose first byte
// is 0xE5). Walks the root directory and all subdirectories via the FAT
// cluster chains. Returns entries sorted by parent cluster then offset.
std::vector<DeletedFile> scanDeletedFiles(std::shared_ptr<DiskIO> disk,
                                          uint64_t start_sector);

// Restore a deleted file:
//  1. re-creates the directory entry (first char set to `replace_first`),
//  2. marks the contiguous cluster run [start_cluster, start_cluster+needed)
//     allocated in BOTH FATs (last entry = EOC),
//  3. decrements the FSInfo free-cluster count.
// Refuses when the needed clusters are not all free (file was overwritten).
Result restoreDeletedFile(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                          const DeletedFile& file,
                          char replace_first = '_');

} // namespace fat32
} // namespace opm