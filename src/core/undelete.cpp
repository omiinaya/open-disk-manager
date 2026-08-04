#include "opm/undelete.hpp"
#include "opm/fat32_impl.hpp"
#include "opm/disk_io.hpp"
#include <algorithm>
#include <cstring>
#include <unordered_set>
#include <vector>

namespace opm {
namespace fat32 {

namespace {

constexpr uint32_t FAT32_EOC_MARK = 0x0FFFFFF8;  // end-of-chain marker
constexpr uint32_t FAT32_BAD = 0x0FFFFFF7;
constexpr uint8_t  ATTR_DIR = 0x10;
constexpr uint8_t  ATTR_LFN = 0x0F;
constexpr uint8_t  DELETED  = 0xE5;

// Read a FAT32 FAT entry (4 bytes, LE) for a cluster.
uint32_t readFATEntry(std::shared_ptr<DiskIO> disk, const FAT32Layout& layout,
                      uint32_t cluster) {
    uint64_t byte_off = layout.fat_start_sector * (uint64_t)layout.bytes_per_sector
                        + (uint64_t)cluster * 4;
    uint8_t b[4];
    if (disk->read(b, byte_off, 4).failed()) return 0;
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
           ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

Result writeFATEntry(std::shared_ptr<DiskIO> disk, const FAT32Layout& layout,
                     uint32_t cluster, uint32_t value) {
    uint8_t b[4] = { static_cast<uint8_t>(value & 0xFF),
                     static_cast<uint8_t>((value >> 8) & 0xFF),
                     static_cast<uint8_t>((value >> 16) & 0xFF),
                     static_cast<uint8_t>((value >> 24) & 0xFF) };
    uint64_t base = layout.fat_start_sector * (uint64_t)layout.bytes_per_sector
                    + (uint64_t)cluster * 4;
    for (uint32_t f = 0; f < layout.num_fats; f++) {
        uint64_t off = base + (uint64_t)f * layout.sectors_per_fat * layout.bytes_per_sector;
        Result r = disk->write(b, off, 4);
        if (r.failed()) return r;
    }
    return Result::ok();
}

bool isEOC(uint32_t v) { return v >= FAT32_EOC_MARK; }
bool isFree(uint32_t v) { return v == 0x00000000; }

// 11-byte 8.3 name -> string with first byte replaced by a placeholder.
std::string nameForDisplay(const uint8_t dname[11]) {
    std::string out;
    out.reserve(13);
    char base[12];
    for (int i = 0; i < 11; i++) {
        base[i] = (dname[i] == 0x20 || dname[i] == 0x00) ? '\0' : static_cast<char>(dname[i]);
    }
    base[11] = '\0';
    std::string n(base);
    if (n.empty()) return "_";
    size_t dot = 0;
    // name is 8.3: bytes 0-7 name, 8-10 ext
    std::string name_part;
    for (int i = 0; i < 8; i++) {
        char c = static_cast<char>(dname[i]);
        if (c == 0x20 || c == 0x00) break;
        name_part += c;
    }
    std::string ext_part;
    for (int i = 8; i < 11; i++) {
        char c = static_cast<char>(dname[i]);
        if (c == 0x20 || c == 0x00) break;
        ext_part += c;
    }
    name_part[0] = '_';
    (void)dot;
    return ext_part.empty() ? name_part : name_part + "." + ext_part;
}

} // anonymous namespace

std::vector<DeletedFile> scanDeletedFiles(std::shared_ptr<DiskIO> disk,
                                          uint64_t start_sector) {
    std::vector<DeletedFile> out;
    if (!disk || !disk->isOpen()) return out;

    FAT32BootSector bs;
    FAT32Layout layout;
    if (getFAT32Info(disk, start_sector, bs, layout).failed()) return out;

    const uint32_t bpc = layout.bytes_per_sector * layout.sectors_per_cluster;
    std::unordered_set<uint32_t> visited;
    std::vector<std::pair<uint32_t, uint32_t> /*cluster, depth*/> work;
    work.push_back({layout.root_cluster, 0});

    while (!work.empty()) {
        auto [cluster, depth] = work.back();
        work.pop_back();
        if (depth > 32) continue;
        if (visited.count(cluster)) continue;
        visited.insert(cluster);

        uint32_t cur = cluster;
        bool chain_done = false;
        while (!chain_done) {
            uint64_t dir_sector = layout.clusterToSector(cur);
            std::vector<uint8_t> data(bpc, 0);
            if (disk->read(data.data(), (start_sector + dir_sector) * layout.bytes_per_sector, bpc).failed()) {
                break;
            }
            bool end_of_dir = false;
            for (size_t off = 0; off + 32 <= data.size(); off += 32) {
                const uint8_t* e = data.data() + off;
                uint8_t b0 = e[0];
                if (b0 == 0x00) { end_of_dir = true; break; }
                uint8_t attr = e[11];
                if (attr == ATTR_LFN) continue;
                if (b0 == DELETED) {
                    DeletedFile f;
                    f.entry_offset = (start_sector + dir_sector) * layout.bytes_per_sector + off;
                    f.name = nameForDisplay(e);
                    f.parent_cluster = cur;
                    f.size_bytes = (uint32_t)e[28] | ((uint32_t)e[29] << 8) |
                                   ((uint32_t)e[30] << 16) | ((uint32_t)e[31] << 24);
                    uint32_t cl_lo = (uint32_t)(e[26] | (e[27] << 8));
                    uint32_t cl_hi = (uint32_t)(e[20] | (e[21] << 8));
                    uint32_t cl = cl_lo | (cl_hi << 16);
                    f.start_cluster = cl;
                    out.push_back(f);
                    continue;
                }
                if (attr & ATTR_DIR) {
                    // Recurse into a non-deleted subdirectory (skip . and ..).
                    if (e[0] == '.' ) continue;
                    uint32_t cl_lo = (uint32_t)(e[26] | (e[27] << 8));
                    uint32_t cl_hi = (uint32_t)(e[20] | (e[21] << 8));
                    uint32_t cl = cl_lo | (cl_hi << 16);
                    if (cl >= 2 && !visited.count(cl)) {
                        work.push_back({cl, depth + 1});
                    }
                }
            }
            if (end_of_dir) break;
            uint32_t next = readFATEntry(disk, layout, cur);
            if (isEOC(next) || next == 0 || isFree(next) || next == FAT32_BAD) break;
            cur = next;
        }
    }

    std::sort(out.begin(), out.end(), [](const DeletedFile& a, const DeletedFile& b) {
        if (a.parent_cluster != b.parent_cluster) return a.parent_cluster < b.parent_cluster;
        return a.entry_offset < b.entry_offset;
    });
    return out;
}

Result restoreDeletedFile(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                          const DeletedFile& file, char replace_first) {
    if (!disk || !disk->isOpen()) return Result::error("Disk not open");
    if (disk->isReadOnly()) return Result::error("Disk is read-only");

    FAT32BootSector bs;
    FAT32Layout layout;
    Result r = getFAT32Info(disk, start_sector, bs, layout);
    if (r.failed()) return Result::error("Not a FAT32 volume: " + r.message);

    // Read the directory entry; it must still be marked deleted.
    uint8_t entry[32];
    r = disk->read(entry, file.entry_offset, 32);
    if (r.failed()) return Result::error("Failed to read directory entry: " + r.message);
    if (entry[0] != DELETED) return Result::error("Entry is no longer marked deleted");

    const uint32_t cluster_size = layout.bytes_per_sector * layout.sectors_per_cluster;
    uint64_t needed = (uint64_t)file.size_bytes / cluster_size;
    if (file.size_bytes % cluster_size != 0) needed++;
    if (needed == 0) needed = 1;

    // Validate the cluster run is within the volume and all clusters are free.
    uint32_t max_cluster = layout.root_cluster + layout.total_clusters; // approximate upper
    if (file.start_cluster < 2) return Result::error("Invalid starting cluster");
    for (uint64_t i = 0; i < needed; i++) {
        uint32_t cl = file.start_cluster + (uint32_t)i;
        if (cl >= layout.total_clusters + 2) {
            return Result::error("Cluster run exceeds the volume");
        }
        uint32_t v = readFATEntry(disk, layout, cl);
        if (!isFree(v)) {
            return Result::error("Cluster " + std::to_string(cl) +
                                 " is no longer free; the file data may have been overwritten");
        }
    }

    // Mark the contiguous run allocated in both FATs.
    for (uint64_t i = 0; i < needed; i++) {
        uint32_t cl = file.start_cluster + (uint32_t)i;
        uint32_t nxt = (i + 1 < needed) ? cl + 1 : FAT32_EOC_MARK;
        r = writeFATEntry(disk, layout, cl, nxt);
        if (r.failed()) return Result::error("Failed to update FAT: " + r.message);
    }

    // Re-create the directory entry (undelete).
    entry[0] = static_cast<uint8_t>(replace_first);
    r = disk->write(entry, file.entry_offset, 32);
    if (r.failed()) return Result::error("Failed to write directory entry: " + r.message);

    // Decrement FSInfo free count.
    FAT32FSInfo fs_info;
    if (readFSInfo(disk, start_sector, layout, fs_info).success()) {
        uint32_t free_count = fs_info.fsi_free_count >= static_cast<uint32_t>(needed)
            ? fs_info.fsi_free_count - static_cast<uint32_t>(needed)
            : fs_info.fsi_free_count;
        updateFSInfo(disk, start_sector, layout, free_count, fs_info.fsi_next_free);
    }

    r = disk->flush();
    if (r.failed()) return Result::error("Failed to flush: " + r.message);
    return Result::ok();
}

} // namespace fat32
} // namespace opm