#include "opm/fat32_impl.hpp"
#include "opm/disk_io.hpp"
#include <cstring>
#include <vector>

namespace opm {
namespace fat32 {

Result setLabel(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                const std::string& label) {
    if (!disk || !disk->isOpen()) {
        return Result::error("Disk not open");
    }
    if (disk->isReadOnly()) {
        return Result::error("Disk is read-only");
    }

    FAT32BootSector boot_sector;
    FAT32Layout layout;
    Result r = getFAT32Info(disk, start_sector, boot_sector, layout);
    if (r.failed()) {
        return Result::error("Not a valid FAT32 volume: " + r.message);
    }

    uint64_t root_sector = start_sector + layout.clusterToSector(layout.root_cluster);
    const uint32_t bps = layout.bytes_per_sector;
    std::vector<uint8_t> data(bps, 0);
    r = disk->readSector(data.data(), root_sector);
    if (r.failed()) {
        return Result::error("Failed to read root directory: " + r.message);
    }

    const size_t entry_count = bps / 32;
    FAT32DirEntry* entries = reinterpret_cast<FAT32DirEntry*>(data.data());

    // Prefer an existing volume-label entry; fall back to the first free slot.
    int target = -1;
    for (size_t i = 0; i < entry_count; i++) {
        if (entries[i].isVolumeLabel()) {
            target = static_cast<int>(i);
            break;
        }
        if (target < 0 && entries[i].dir_name[0] == 0x00) {
            target = static_cast<int>(i);
        }
    }
    if (target < 0) {
        return Result::error("No free root-directory entry for a volume label");
    }

    // If the slot is free, initialize a full volume-label entry; otherwise
    // patch the name in place (preserving timestamps).
    if (entries[target].dir_name[0] == 0x00 && !entries[target].isVolumeLabel()) {
        entries[target].initVolumeLabel(label.c_str());
    } else {
        std::memset(entries[target].dir_name, ' ', 11);
        size_t n = label.size() < 11 ? label.size() : 11;
        std::memcpy(entries[target].dir_name, label.data(), n);
        entries[target].dir_attr = ATTR_VOLUME_ID;
    }

    r = disk->writeSector(data.data(), root_sector);
    if (r.failed()) {
        return Result::error("Failed to write root directory: " + r.message);
    }
    r = disk->flush();
    if (r.failed()) {
        return Result::error("Failed to flush: " + r.message);
    }
    return Result::ok();
}

} // namespace fat32
} // namespace opm