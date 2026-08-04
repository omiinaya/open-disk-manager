#include "opm/swap.hpp"
#include "opm/disk_io.hpp"

#include <cstring>
#include <vector>

namespace opm {

namespace {

// Swap v1 header fields within the first 4096-byte page.
constexpr size_t SWAP_PAGE_SIZE   = 4096;
constexpr size_t SWAP_VERSION_OFF = 4084;  // uint32 LE: page layout version (1 = 4K)
constexpr size_t SWAP_MAGIC_OFF   = 4088;  // "SWAPSPACE2" (10 bytes)
constexpr size_t SWAP_LABEL_OFF   = 4068;  // 16-byte volume label field (v1)
constexpr uint32_t SWAP_VERSION_1 = 1;
const char SWAP_MAGIC[11] = "SWAPSPACE2";

} // anonymous namespace

Result formatSwap(std::shared_ptr<DiskIO> disk,
                  uint64_t start_sector,
                  uint64_t size_bytes,
                  const std::string& label) {
    if (!disk || !disk->isOpen()) {
        return Result::error("Disk not open");
    }
    if (disk->isReadOnly()) {
        return Result::error("Disk is read-only");
    }
    if (size_bytes < SWAP_PAGE_SIZE) {
        return Result::error("Swap area too small (need at least 4096 bytes)");
    }

    // Zero the leading space (a few pages is enough to clear prior FS
    // signatures on the boot sector/superblock region; swapon only inspects
    // the last 16 bytes of the first page for the signature).
    const uint64_t ss = disk->sectorSize();
    const uint64_t pages_to_clear = 4;  // 16 MiB max? keep small for speed
    uint64_t zero_sectors = (pages_to_clear * SWAP_PAGE_SIZE) / ss;
    if (zero_sectors == 0) zero_sectors = 1;
    std::vector<uint8_t> zeros(static_cast<size_t>(ss) * zero_sectors, 0);
    size_t written = 0;
    const size_t buf_sectors = zero_sectors;
    // Cap the clear at the area size.
    uint64_t available_sectors = size_bytes / ss;
    uint64_t to_clear = std::min<uint64_t>(buf_sectors, available_sectors);
    Result w = disk->writeSectors(zeros.data(), start_sector, static_cast<uint32_t>(to_clear));
    if (w.failed()) {
        return Result::error("Failed to clear swap area: " + w.message);
    }
    written = to_clear;
    (void)written;

    // Build the swap header page(s). The v1 "SWAPSPACE2" magic is 10 bytes at
    // byte 4088, which spans the 4096-byte page boundary — write two pages so
    // all 10 bytes land (kernel/mkswap do the same).
    std::vector<uint8_t> hdr(SWAP_PAGE_SIZE * 2, 0);

    // Version (LE32) = 1 => 4096-byte pages.
    hdr[SWAP_VERSION_OFF + 0] = SWAP_VERSION_1 & 0xFF;
    hdr[SWAP_VERSION_OFF + 1] = (SWAP_VERSION_1 >> 8) & 0xFF;
    hdr[SWAP_VERSION_OFF + 2] = (SWAP_VERSION_1 >> 16) & 0xFF;
    hdr[SWAP_VERSION_OFF + 3] = (SWAP_VERSION_1 >> 24) & 0xFF;

    // Magic "SWAPSPACE2" — all 10 bytes, spanning into the second page.
    std::memcpy(&hdr[SWAP_MAGIC_OFF], SWAP_MAGIC, 10);

    // Optional 16-byte label.
    if (!label.empty()) {
        size_t n = label.size() < 16 ? label.size() : 16;
        std::memcpy(&hdr[SWAP_LABEL_OFF], label.data(), n);
    }

    // Write the header (two pages) starting at the partition start.
    uint64_t hdr_sectors = (SWAP_PAGE_SIZE * 2) / ss;
    for (uint64_t i = 0; i < hdr_sectors; i++) {
        Result r = disk->writeSector(hdr.data() + static_cast<size_t>(i * ss),
                                     start_sector + i);
        if (r.failed()) {
            return Result::error("Failed to write swap header: " + r.message);
        }
    }

    Result r = disk->flush();
    if (r.failed()) {
        return Result::error("Failed to flush swap area: " + r.message);
    }

    return Result::ok();
}

bool isSwap(std::shared_ptr<DiskIO> disk, uint64_t start_sector) {
    if (!disk || !disk->isOpen()) {
        return false;
    }
    const uint64_t ss = disk->sectorSize();
    // Read two pages: the v1 magic (10 bytes at 4088) spans the first page.
    uint64_t read_bytes = SWAP_PAGE_SIZE * 2;
    uint64_t sectors = (read_bytes + ss - 1) / ss;
    std::vector<uint8_t> page(static_cast<size_t>(sectors) * ss, 0);
    Result r = disk->readSectors(page.data(), start_sector, static_cast<uint32_t>(sectors));
    if (r.failed()) {
        return false;
    }
    if (page.size() < SWAP_MAGIC_OFF + 10) {
        return false;
    }
    if (std::memcmp(page.data() + SWAP_MAGIC_OFF, SWAP_MAGIC, 10) == 0) {
        return true;
    }
    // v0 signature fits inside the first page.
    if (page.size() >= 4086 + 10 &&
        std::memcmp(page.data() + 4086, "SWAP-SPACE", 10) == 0) {
        return true;
    }
    return false;
}

} // namespace opm