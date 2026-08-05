#pragma once

#include "types.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace opm {
class DiskIO;

namespace ntfs {

// A deleted file discovered by scanning the MFT. NTFS marks a record as
// deleted by clearing the IN_USE flag while (usually) leaving the attribute
// list intact — $FILE_NAME carries the name/size, $DATA carries the run list
// of the clusters the file used to occupy.
struct NtfsDeletedFile {
    std::string name;                       // from $FILE_NAME (UTF-16 -> ASCII)
    uint64_t mft_record = 0;                // MFT record number
    uint64_t parent_ref = 0;                // parent directory MFT reference
    uint64_t parent_record = 0;             // parent record number (low 48 bits)
    uint64_t data_size = 0;                 // from $DATA / $FILE_NAME
    uint64_t alloc_size = 0;
    uint32_t attrs = 0;                     // NTFS file attributes
    bool is_dir = false;
    std::vector<std::pair<uint64_t, uint64_t>> runs;  // (LCN, length) pairs
    uint16_t sequence = 0;                  // record sequence number
};

// Scan an NTFS volume's MFT for deleted records (magic FILE, IN_USE clear).
// Walks every record in the MFT (bounded by $MFT's own $DATA size) and
// extracts $FILE_NAME + non-resident $DATA run lists. Skips system records
// (0..15) and $-prefixed names. Sorted by parent record then name.
std::vector<NtfsDeletedFile> scanDeletedNTFS(std::shared_ptr<DiskIO> disk,
                                             uint64_t start_sector);

// In-place restore of a deleted NTFS file:
//   1. verifies the record is still marked deleted and all its clusters are
//      currently free in $Bitmap (refuses when the data was overwritten),
//   2. marks the run-list clusters used in $Bitmap,
//   3. sets the IN_USE flag back on the record,
//   4. re-inserts the $FILE_NAME index entry into the parent directory's
//      $INDEX_ROOT (resident index only; large/non-resident parent indexes
//      return an honest error — use exportDeletedNTFS for those).
Result restoreDeletedNTFS(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                          const NtfsDeletedFile& file);

// Export a deleted NTFS file's data to a host directory (name:
// <out_dir>/<mft_record>_<name>). Writes the reconstructed cluster runs to a
// regular file. This is the reliable recovery path for any volume.
Result exportDeletedNTFS(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                         const NtfsDeletedFile& file,
                         const std::string& out_dir);

} // namespace ntfs
} // namespace opm
