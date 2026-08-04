#include "opm/ext4_impl.hpp"
#include "opm/disk_io.hpp"
#include <cstring>

namespace opm {
namespace ext4 {

Result setLabel(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                const std::string& label) {
    if (!disk || !disk->isOpen()) {
        return Result::error("Disk not open");
    }
    if (disk->isReadOnly()) {
        return Result::error("Disk is read-only");
    }

    // Superblock lives at byte offset 1024 from the partition start
    // (matching getInfo). s_volume_name is 16 bytes at offset 120.
    const uint64_t sb_offset = start_sector * disk->sectorSize() + 1024;

    uint8_t sb[1024];
    Result r = disk->read(sb, sb_offset, 1024);
    if (r.failed()) {
        return Result::error("Failed to read ext4 superblock: " + r.message);
    }
    // Sanity: ext magic 0xEF53 at offset 56.
    if (sb[56] != 0x53 || sb[57] != 0xEF) {
        return Result::error("Not a valid ext4 superblock");
    }

    std::memset(sb + 120, 0, 16);
    size_t n = label.size() < 16 ? label.size() : 16;
    std::memcpy(sb + 120, label.data(), n);

    r = disk->write(sb, sb_offset, 1024);
    if (r.failed()) {
        return Result::error("Failed to write ext4 superblock: " + r.message);
    }
    r = disk->flush();
    if (r.failed()) {
        return Result::error("Failed to flush: " + r.message);
    }
    return Result::ok();
}

} // namespace ext4
} // namespace opm