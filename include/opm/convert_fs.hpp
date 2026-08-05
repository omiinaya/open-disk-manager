#pragma once

#include "types.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace opm {

// Forward declarations
class DiskIO;

// ============================================================================
// On-disk filesystem conversion (data-preserving)
// ============================================================================
//
// convertFAT32ToNTFS() rewrites a FAT32 partition's metadata into a real NTFS
// volume WITHOUT moving any file data:
//
//   * The NTFS cluster size is chosen to equal the FAT32 cluster size, so
//     every FAT32 data cluster maps 1:1 to an NTFS logical cluster number
//     (LCN = data_start/spc + (fat_cluster - 2)). File contents stay at their
//     exact physical offsets.
//   * The old FAT32 reserved sectors + FAT tables become free; the NTFS
//     system structures ($MFT, $Bitmap, $LogFile, $UpCase, ...) are allocated
//     from free clusters, preferring that region.
//   * Each converted file/dir gets a real MFT record with
//     $STANDARD_INFORMATION, $FILE_NAME, and a non-resident $DATA run list
//     that reuses the original cluster chain (or $INDEX_ROOT /
//     $INDEX_ALLOCATION for directories).
//
// The result is a genuine NTFS volume: it passes opm's own checkNTFS (which
// resolves system files from the MFT run lists) and is readable by tools that
// parse NTFS ($Root index tree, fixup USNs on every record).
//
// Returns an error if the partition is not FAT32, the volume is too small, or
// a directory cannot be represented (record overflow) — never partially
// converts: all writes happen only after the plan fully validates.

// Convert a FAT32 partition (starting at start_sector, size_bytes long) to
// NTFS in place. label, if non-empty, becomes the NTFS volume label.
Result convertFAT32ToNTFS(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                          uint64_t size_bytes, const std::string& label = "");

// Convenience: run the whole conversion + validation on a partition, given a
// partition number (1-based) from the loaded table. Returns the resulting
// filesystem label on success (out_label).
Result convertPartitionToNTFS(std::shared_ptr<DiskIO> disk, int partition_number,
                              std::string* out_label = nullptr);

// Detect which on-disk filesystem a region uses (for the convert front-end).
// Returns FileSystemType::Unknown when nothing is recognized.
FileSystemType detectFilesystemAt(std::shared_ptr<DiskIO> disk,
                                  uint64_t start_sector);

} // namespace opm
