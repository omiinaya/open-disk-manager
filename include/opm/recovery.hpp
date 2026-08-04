#pragma once

#include "types.hpp"
#include <memory>
#include <string>
#include <vector>

namespace opm {
class DiskIO;

// A filesystem/partition candidate discovered during a recovery scan.
struct RecoveryCandidate {
    uint64_t start_sector = 0;   // Absolute start sector on the disk
    uint64_t size_sectors = 0;   // Estimated size (0 = unknown)
    FileSystemType fs = FileSystemType::Unknown;
    uint8_t mbr_type = 0x00;     // MBR type byte (0xEE = EFI, etc.)
    bool from_partition_table = false;  // True if a table entry gave exact bounds
    bool bootable = false;
    std::string description;
};

// Scan a disk for partition-table entries and orphan filesystem signatures.
//
// The scan probes every `probe_step` sectors for known filesystem signatures
// (FAT32/NTFS/exFAT/ext4/swap/LUKS/BitLocker). A valid MBR/GPT table, when
// present, contributes exact entries first; the signature scan then finds
// anything the table lost. Candidates are returned sorted by start sector.
std::vector<RecoveryCandidate> scanForPartitions(
    std::shared_ptr<DiskIO> disk,
    uint64_t probe_step = 2048,
    ProgressCallback progress = nullptr);

// Rebuild an MBR partition table from the given candidates. Partition data is
// NOT touched — only the table at sector 0 is rewritten. Refuses more than 4
// candidates or any candidate beyond the 2 TiB MBR limit.
Result rebuildPartitionTable(std::shared_ptr<DiskIO> disk,
                             const std::vector<RecoveryCandidate>& candidates);

} // namespace opm