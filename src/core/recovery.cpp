#include "opm/recovery.hpp"
#include "opm/partition_table.hpp"
#include "opm/disk_io.hpp"
#include "opm/utils.hpp"
#include <algorithm>
#include <cstring>
#include <vector>

namespace opm {

namespace {

constexpr uint64_t TWO_TIB_SECTORS = 1ULL << 32;

PartitionType partitionTypeForCandidate(const RecoveryCandidate& c) {
    switch (c.fs) {
        case FileSystemType::FAT12: return PartitionType::FAT12;
        case FileSystemType::FAT16: return PartitionType::FAT16;
        case FileSystemType::FAT32: return PartitionType::FAT32LBA;
        case FileSystemType::NTFS:  return PartitionType::NTFS;
        case FileSystemType::Swap:  return PartitionType::LinuxSwap;
        case FileSystemType::LVM2:  return PartitionType::LinuxLVM;
        case FileSystemType::RAID:  return PartitionType::LinuxRAID;
        case FileSystemType::EFI:   return PartitionType::EFI;
        default:                    return PartitionType::Linux;
    }
}

// Identify a filesystem signature at the given probe sector. Returns the
// filesystem type, or Unknown when nothing is there.
FileSystemType detectSignatureAt(std::shared_ptr<DiskIO> disk, uint64_t sector) {
    const uint64_t ss = disk->sectorSize();
    uint8_t buf[8192];

    if (disk->readSector(buf, sector).success()) {
        if (std::memcmp(buf + 3, "NTFS    ", 8) == 0) return FileSystemType::NTFS;
        if (std::memcmp(buf + 82, "FAT32   ", 8) == 0) return FileSystemType::FAT32;
        if (std::memcmp(buf + 3, "EXFAT   ", 8) == 0) return FileSystemType::exFAT;
        if (std::memcmp(buf + 3, "-FVE-FS-", 8) == 0) return FileSystemType::NTFS;  // BitLocker
        if (std::memcmp(buf, "LUKS", 4) == 0) return FileSystemType::LVM2;          // LUKS
    }
    // ext4 superblock at byte offset 1024.
    if (disk->read(buf, sector * ss + 1024, 512).success()) {
        if (buf[56] == 0x53 && buf[57] == 0xEF) return FileSystemType::EXT4;
    }
    // Swap v1 signature spans the 4096-byte page boundary (10 bytes at 4088).
    if (disk->read(buf, sector * ss, 8192).success()) {
        if (std::memcmp(buf + 4088, "SWAPSPACE2", 10) == 0 ||
            std::memcmp(buf + 4086, "SWAP-SPACE", 10) == 0) {
            return FileSystemType::Swap;
        }
    }
    return FileSystemType::Unknown;
}

const char* fsLabel(FileSystemType fs) {
    switch (fs) {
        case FileSystemType::FAT12: return "FAT12";
        case FileSystemType::FAT16: return "FAT16";
        case FileSystemType::FAT32: return "FAT32";
        case FileSystemType::exFAT: return "exFAT";
        case FileSystemType::NTFS:  return "NTFS";
        case FileSystemType::EXT2:  return "ext2";
        case FileSystemType::EXT3:  return "ext3";
        case FileSystemType::EXT4:  return "ext4";
        case FileSystemType::Swap:  return "swap";
        case FileSystemType::LVM2:  return "LUKS/LVM";
        case FileSystemType::EFI:   return "EFI";
        default:                    return "unknown";
    }
}

bool overlaps(const RecoveryCandidate& a, uint64_t start, uint64_t end) {
    if (a.size_sectors == 0) return false;
    uint64_t a_end = a.start_sector + a.size_sectors - 1;
    return !(end < a.start_sector || start > a_end);
}

} // anonymous namespace

std::vector<RecoveryCandidate> scanForPartitions(
    std::shared_ptr<DiskIO> disk,
    uint64_t probe_step,
    ProgressCallback progress) {
    std::vector<RecoveryCandidate> out;
    if (!disk || !disk->isOpen()) {
        return out;
    }
    const uint64_t total = disk->sectorCount();

    // 1) Exact candidates from an existing partition table.
    try {
        auto table = PartitionTable::load(disk);
        if (table && table->isValid()) {
            for (const auto& p : table->getPartitions()) {
                RecoveryCandidate c;
                c.start_sector = p.startSector();
                c.size_sectors = p.sectorCount();
                c.fs = p.filesystem();
                c.mbr_type = static_cast<uint8_t>(p.type());
                c.from_partition_table = true;
                c.bootable = p.isBootable();
                c.description = "from " + table->typeName() + " table";
                if (c.fs == FileSystemType::Unknown) {
                    // Derive from the partition type byte where possible.
                    switch (static_cast<int>(c.mbr_type)) {
                        case 0x07: c.fs = FileSystemType::NTFS; break;
                        case 0x0B:
                        case 0x0C: c.fs = FileSystemType::FAT32; break;
                        case 0x82: c.fs = FileSystemType::Swap; break;
                        case 0x83: c.fs = FileSystemType::EXT4; break;
                        case 0xEF: c.fs = FileSystemType::EFI; break;
                        default: break;
                    }
                }
                out.push_back(c);
            }
        }
    } catch (...) {
        // Corrupt table: fall through to the signature scan.
    }

    // 2) Signature scan for anything the table missed.
    if (probe_step == 0) probe_step = 2048;
    uint64_t total_probes = total / probe_step;
    uint64_t probe = 0;
    for (uint64_t s = probe_step; s < total; s += probe_step) {
        probe++;
        FileSystemType fs = detectSignatureAt(disk, s);
        if (fs != FileSystemType::Unknown) {
            // Skip if this signature lies inside an exact table entry.
            bool inside = false;
            for (const auto& c : out) {
                if (c.from_partition_table && c.size_sectors > 0) {
                    uint64_t end = c.start_sector + c.size_sectors - 1;
                    if (s >= c.start_sector && s <= end) { inside = true; break; }
                }
            }
            if (!inside) {
                RecoveryCandidate c;
                c.start_sector = s;
                c.fs = fs;
                c.description = std::string("signature scan (") + fsLabel(fs) + ")";
                out.push_back(c);
            }
        }
        if (progress && (probe % 16 == 0)) {
            progress(probe, total_probes, "scanning for partitions");
        }
    }

    // 3) Estimate sizes for signature-only candidates (up to the next
    //    candidate start or the end of the disk).
    std::sort(out.begin(), out.end(),
              [](const RecoveryCandidate& a, const RecoveryCandidate& b) {
                  return a.start_sector < b.start_sector;
              });
    for (size_t i = 0; i < out.size(); i++) {
        if (out[i].size_sectors == 0) {
            uint64_t end = total;
            for (size_t j = i + 1; j < out.size(); j++) {
                if (out[j].start_sector > out[i].start_sector) {
                    end = out[j].start_sector;
                    break;
                }
            }
            out[i].size_sectors = end - out[i].start_sector;
        }
    }

    if (progress) {
        progress(total_probes, total_probes, "scan complete");
    }
    return out;
}

Result rebuildPartitionTable(std::shared_ptr<DiskIO> disk,
                             const std::vector<RecoveryCandidate>& candidates) {
    if (!disk || !disk->isOpen()) {
        return Result::error("Disk not open");
    }
    if (disk->isReadOnly()) {
        return Result::error("Disk is read-only");
    }

    // Only rebuild MBR: at most 4 primary partitions, all below 2 TiB.
    int usable = 0;
    for (const auto& c : candidates) {
        if (c.start_sector < 2048) continue;      // don't clobber the boot area
        if (c.start_sector >= TWO_TIB_SECTORS) continue;
        usable++;
    }
    if (usable == 0) {
        return Result::error("No recoverable partitions found");
    }
    if (usable > 4) {
        return Result::error("Too many candidates (" + std::to_string(usable) +
                             ") for an MBR table (max 4)");
    }

    auto mbr = MBRTable::createNew(disk);
    int added = 0;
    for (const auto& c : candidates) {
        if (c.start_sector < 2048 || c.start_sector >= TWO_TIB_SECTORS) continue;
        uint64_t size_bytes = c.size_sectors > 0
            ? c.size_sectors * disk->sectorSize()
            : (disk->sectorCount() - c.start_sector) * disk->sectorSize();
        if (c.start_sector + size_bytes / disk->sectorSize() > TWO_TIB_SECTORS) {
            size_bytes = (TWO_TIB_SECTORS - c.start_sector) * disk->sectorSize();
        }
        PartitionType pt = partitionTypeForCandidate(c);
        Result r = mbr->createPartition(c.start_sector, size_bytes, pt, "");
        if (r.failed()) {
            return Result::error("Failed to add candidate at sector " +
                                 std::to_string(c.start_sector) + ": " + r.message);
        }
        if (c.bootable) {
            mbr->setPartitionBootable(added + 1, true);
        }
        added++;
    }

    Result r = mbr->commit();
    if (r.failed()) {
        return Result::error("Commit failed: " + r.message);
    }
    return Result::ok();
}

} // namespace opm