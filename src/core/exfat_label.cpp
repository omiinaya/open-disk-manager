#include "opm/exfat_impl.hpp"
#include "opm/disk_io.hpp"
#include <cstring>
#include <vector>

namespace opm {
namespace exfat {

Result setLabel(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                const std::string& label) {
    if (!disk || !disk->isOpen()) {
        return Result::error("Disk not open");
    }
    if (disk->isReadOnly()) {
        return Result::error("Disk is read-only");
    }

    ExFATBootSector boot;
    Result r = disk->readSector(&boot, start_sector);
    if (r.failed()) {
        return Result::error("Failed to read exFAT boot sector: " + r.message);
    }
    if (std::memcmp(boot.bs_file_system_name, "EXFAT   ", 8) != 0) {
        return Result::error("Not a valid exFAT boot sector");
    }

    const uint32_t bps = 1u << boot.bs_bytes_per_sector_shift;
    const uint32_t spc = 1u << boot.bs_sectors_per_cluster_shift;
    const uint32_t bpc = bps * spc;
    const uint32_t root_cluster = boot.bs_first_cluster_of_root;
    const uint64_t root_sector = start_sector +
        boot.bs_cluster_heap_offset +
        (root_cluster - EXFAT_FIRST_DATA_CLUSTER) * spc;

    // Read the first root-directory cluster (entries are 32 bytes).
    std::vector<uint8_t> data(bpc, 0);
    uint64_t root_byte = root_sector * bps;
    r = disk->read(data.data(), root_byte, bpc);
    if (r.failed()) {
        return Result::error("Failed to read root directory: " + r.message);
    }

    // Find the volume-label entry (0x83) or a free slot (0x00).
    int target = -1;
    for (size_t off = 0; off + 32 <= data.size(); off += 32) {
        uint8_t t = data[off];
        if (t == EXFAT_ENTRY_VOLUME_LABEL) {
            target = static_cast<int>(off);
            break;
        }
        if (target < 0 && t == EXFAT_ENTRY_END) {
            target = static_cast<int>(off);
        }
    }
    if (target < 0) {
        return Result::error("No free root-directory entry for a volume label");
    }

    size_t count = label.size() < 11 ? label.size() : 11;
    uint8_t* entry = data.data() + target;
    std::memset(entry, 0, 32);
    entry[0] = EXFAT_ENTRY_VOLUME_LABEL;
    entry[1] = static_cast<uint8_t>(count);
    for (size_t i = 0; i < count; i++) {
        // UTF-16LE from ASCII (matching the format path).
        uint16_t c = static_cast<unsigned char>(label[i]);
        entry[2 + i * 2] = static_cast<uint8_t>(c & 0xFF);
        entry[3 + i * 2] = static_cast<uint8_t>(c >> 8);
    }

    r = disk->write(data.data(), root_byte, bpc);
    if (r.failed()) {
        return Result::error("Failed to write root directory: " + r.message);
    }
    r = disk->flush();
    if (r.failed()) {
        return Result::error("Failed to flush: " + r.message);
    }
    return Result::ok();
}

} // namespace exfat
} // namespace opm