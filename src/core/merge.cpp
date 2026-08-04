#include "opm/merge.hpp"
#include "opm/partition_table.hpp"
#include "opm/partition.hpp"
#include "opm/fat32_impl.hpp"
#include <cstring>
#include <vector>
#include <string>

namespace opm {

namespace {

constexpr uint8_t ATTR_LFN_LOCAL = 0x0F;

// A file entry discovered in a FAT32 tree walk.
struct FatEntry {
    std::string name;           // short name (8.3)
    std::string parent_path;    // slash-joined folder path within the volume
    uint32_t start_cluster = 0;
    uint32_t size_bytes = 0;
    bool is_dir = false;
};

uint32_t fatCluster(const uint8_t e[32]) {
    uint32_t lo = (uint32_t)(e[26] | (uint16_t)(e[27] << 8));
    uint32_t hi = (uint32_t)(e[20] | (uint16_t)(e[21] << 8));
    return lo | (hi << 16);
}

// Decode an on-disk 8.3 name (11 bytes, space padded) into "NAME.EXT".
// The raw bytes already have the extension in bytes 8-10 — do NOT route them
// through createShortName (which expects a long name and would drop the ext).
std::string decodeShortName(const uint8_t n[11]) {
    std::string base, ext;
    for (int i = 0; i < 8; i++) {
        char c = static_cast<char>(n[i]);
        if (c == ' ' || c == 0) break;
        base += c;
    }
    for (int i = 8; i < 11; i++) {
        char c = static_cast<char>(n[i]);
        if (c == ' ' || c == 0) break;
        ext += c;
    }
    return ext.empty() ? base : base + "." + ext;
}

// Read one FAT entry value from the on-disk FAT (4-byte LE).
uint32_t readFatValue(std::shared_ptr<DiskIO> disk, uint64_t vol_start,
                      const fat32::FAT32Layout& layout, uint32_t cluster) {
    uint64_t fat_offset = (uint64_t)layout.fat_start_sector * layout.bytes_per_sector
                          + (uint64_t)cluster * 4;
    uint8_t b[4];
    if (disk->read(b, vol_start + fat_offset, 4).failed()) return 0;
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

bool isEoc(uint32_t v) { return (v & 0x0FFFFFFF) >= fat32::FAT32_EOC_START; }

// Walk a FAT32 volume tree, collecting regular files (recursive).
Result collectTree(std::shared_ptr<DiskIO> disk, uint64_t vol_start,
                   const fat32::FAT32Layout& layout, std::vector<FatEntry>& out,
                   const std::string& parent_path, uint32_t dir_cluster,
                   std::vector<uint32_t>& visited) {
    for (uint32_t c : visited) if (c == dir_cluster) return Result::ok();  // cylce guard
    visited.push_back(dir_cluster);

    const uint32_t bpc = layout.bytes_per_sector * layout.sectors_per_cluster;
    uint32_t cur = dir_cluster;
    uint64_t guard = 0;
    while (!isEoc(cur) && cur >= 2 && guard++ < 1000000) {
        uint64_t dir_sector = (uint64_t)layout.clusterToSector(cur);
        std::vector<uint8_t> data(bpc, 0);
        Result r = disk->read(data.data(), vol_start + dir_sector * layout.bytes_per_sector, bpc);
        if (r.failed()) return Result::error("read dir sector: " + r.message);
        for (size_t off = 0; off + 32 <= data.size(); off += 32) {
            const uint8_t* e = data.data() + off;
            uint8_t b0 = e[0];
            if (b0 == 0x00) return Result::ok();               // end of directory
            uint8_t attr = e[11];
            if (attr == ATTR_LFN_LOCAL) continue;
            if (b0 == fat32::DENTRY_DELETED) continue;
            if (attr & fat32::ATTR_VOLUME_ID) continue;
            // "." and ".." are space-padded 8.3 names starting with '.'.
            if (e[0] == '.') continue;
            std::string name = decodeShortName(e);
            FatEntry fe;
            fe.name = name;
            fe.parent_path = parent_path;
            fe.start_cluster = fatCluster(e);
            fe.size_bytes = (uint32_t)e[28] | ((uint32_t)e[29] << 8) |
                            ((uint32_t)e[30] << 16) | ((uint32_t)e[31] << 24);
            fe.is_dir = (attr & fat32::ATTR_DIRECTORY) != 0;
            if (fe.is_dir) {
                std::string child_path = parent_path.empty() ? name : parent_path + "/" + name;
                // recurse
                if (fe.start_cluster >= 2) {
                    Result rr = collectTree(disk, vol_start, layout, out, child_path,
                                            fe.start_cluster, visited);
                    if (rr.failed()) return rr;
                }
            } else {
                out.push_back(fe);
            }
        }
        cur = readFatValue(disk, vol_start, layout, cur);
    }
    return Result::ok();
}

// Read a file's bytes from a FAT32 volume by following its cluster chain.
Result readFileBytes(std::shared_ptr<DiskIO> disk, uint64_t vol_start,
                     const fat32::FAT32Layout& layout, uint32_t start_cluster,
                     uint32_t size, std::vector<uint8_t>& buf) {
    buf.clear();
    const uint32_t bpc = layout.bytes_per_sector * layout.sectors_per_cluster;
    uint32_t cur = start_cluster;
    uint32_t remaining = size;
    uint64_t guard = 0;
    while (remaining > 0 && cur >= 2 && !isEoc(cur) && guard++ < 10000000) {
        uint64_t data_sector = (uint64_t)layout.clusterToSector(cur);
        uint32_t take = remaining < bpc ? remaining : bpc;
        std::vector<uint8_t> chunk(bpc, 0);
        Result r = disk->read(chunk.data(), vol_start + data_sector * layout.bytes_per_sector, bpc);
        if (r.failed()) return r;
        buf.insert(buf.end(), chunk.begin(), chunk.begin() + take);
        remaining -= take;
        cur = readFatValue(disk, vol_start, layout, cur);
    }
    if (remaining > 0) return Result::error("cluster chain shorter than file size");
    return Result::ok();
}

// Ensure a folder path exists inside the FAT32 volume at vol_start, creating
// entries as needed. Returns the cluster of the final directory.
Result ensureFolder(std::shared_ptr<DiskIO> disk, uint64_t vol_start,
                    const fat32::FAT32Layout& layout,
                    const std::string& path, uint32_t& out_cluster) {
    if (path.empty()) { out_cluster = layout.root_cluster; return Result::ok(); }
    struct PathSegment { std::string name; uint32_t cluster; };
    // Split path, tracking existing clusters; create missing dirs.
    std::vector<std::string> parts;
    std::string cur;
    for (char c : path) { if (c == '/') { parts.push_back(cur); cur.clear(); } else cur += c; }
    parts.push_back(cur);

    std::vector<uint32_t> visited;
    // Find the deepest existing prefix by scanning the tree.
    // Simple approach: create dirs incrementally from root.
    uint32_t parent = layout.root_cluster;
    std::string built;
    std::vector<FatEntry> all;
    std::vector<uint32_t> vis;
    Result r = collectTree(disk, vol_start, layout, all, "", layout.root_cluster, vis);
    if (r.failed()) return r;

    for (const auto& p : parts) {
        if (p.empty()) continue;
        // Look for an existing dir named p under parent's path.
        std::string search_parent = built;
        uint32_t found_cluster = 0;
        for (const auto& fe : all) {
            if (fe.is_dir && fe.name == p && fe.parent_path == search_parent) {
                found_cluster = fe.start_cluster;
                break;
            }
        }
        if (found_cluster >= 2) {
            parent = found_cluster;
        } else {
            // Create the directory.
            uint32_t new_cluster = 0;
            // Allocate one cluster from the FAT.
            std::vector<uint32_t> fat;
            r = fat32::readFATTable(disk, vol_start, layout, 0, fat);
            if (r.failed()) return r;
            r = fat32::allocateCluster(fat, new_cluster);
            if (r.failed()) return r;
            for (uint32_t f = 0; f < layout.num_fats; f++) {
                r = fat32::writeFATTable(disk, vol_start, layout, f, fat);
                if (r.failed()) return r;
            }
            // Zero the data cluster.
            uint64_t data_sector = (uint64_t)layout.clusterToSector(new_cluster);
            std::vector<uint8_t> zero(layout.bytes_per_sector * layout.sectors_per_cluster, 0);
            r = disk->write(zero.data(), vol_start + data_sector * layout.bytes_per_sector, zero.size());
            if (r.failed()) return r;
            // . and . . entries
            {
                // Build the new dir's first two entries pointing up/down.
                uint8_t dot[32]; std::memset(dot, 0, sizeof(dot));
                std::string dn = ".          ";
                std::memcpy(dot, dn.data(), 11);
                dot[11] = fat32::ATTR_DIRECTORY;
                dot[20] = (uint8_t)((new_cluster >> 8) & 0xFF); dot[21] = (uint8_t)((new_cluster >> 16) & 0xFF);
                dot[26] = (uint8_t)(new_cluster & 0xFF); dot[27] = (uint8_t)((new_cluster >> 8) & 0xFF);
                uint8_t ddot[32]; std::memset(ddot, 0, sizeof(ddot));
                std::string ddn = "..         ";
                std::memcpy(ddot, ddn.data(), 11);
                ddot[11] = fat32::ATTR_DIRECTORY;
                ddot[20] = (uint8_t)((parent >> 8) & 0xFF); ddot[21] = (uint8_t)((parent >> 16) & 0xFF);
                ddot[26] = (uint8_t)(parent & 0xFF); ddot[27] = (uint8_t)((parent >> 8) & 0xFF);
                r = disk->write(dot, vol_start + data_sector * layout.bytes_per_sector, 32);
                if (r.failed()) return r;
                r = disk->write(ddot, vol_start + data_sector * layout.bytes_per_sector + 32, 32);
                if (r.failed()) return r;
            }
            // Create the entry in parent.
            r = fat32::createDirectoryEntry(disk, vol_start, layout, parent, p,
                                            fat32::ATTR_DIRECTORY, new_cluster, 0);
            if (r.failed()) return r;
            parent = new_cluster;
            // Refresh tree knowledge for the next segment.
            all.clear(); vis.clear();
            r = collectTree(disk, vol_start, layout, all, "", layout.root_cluster, vis);
            if (r.failed()) return r;
        }
        built = built.empty() ? p : built + "/" + p;
    }
    out_cluster = parent;
    return Result::ok();
}

// Write a file into the volume under the given parent dir, allocating clusters.
Result writeFile( std::shared_ptr<DiskIO> disk, uint64_t vol_start,
                  const fat32::FAT32Layout& layout, uint32_t parent_dir,
                  const std::string& name, const std::vector<uint8_t>& data) {
    const uint32_t bpc = layout.bytes_per_sector * layout.sectors_per_cluster;
    uint32_t clusters_needed = data.empty() ? 1 : (uint32_t)((data.size() + bpc - 1) / bpc);

    std::vector<uint32_t> fat;
    Result r = fat32::readFATTable(disk, vol_start, layout, 0, fat);
    if (r.failed()) return r;
    std::vector<uint32_t> alloc;
    for (uint32_t i = 0; i < clusters_needed; i++) {
        uint32_t c = 0;
        r = fat32::allocateCluster(fat, c);
        if (r.failed()) return Result::error("no free clusters in target volume");
        alloc.push_back(c);
    }
    // Chain them (EOC on the last).
    for (size_t i = 0; i < alloc.size(); i++) {
        uint32_t nxt = (i + 1 < alloc.size()) ? alloc[i + 1] : fat32::FAT32_EOC;
        fat[alloc[i]] = nxt & 0x0FFFFFFF;
    }
    for (uint32_t f = 0; f < layout.num_fats; f++) {
        r = fat32::writeFATTable(disk, vol_start, layout, f, fat);
        if (r.failed()) return r;
    }
    // Write data.
    size_t off = 0;
    for (uint32_t c : alloc) {
        uint64_t ds = (uint64_t)layout.clusterToSector(c);
        uint64_t abs = vol_start + ds * layout.bytes_per_sector;
        size_t take = data.size() - off;
        if (take > bpc) take = bpc;
        if (take > 0) {
            r = disk->write(data.data() + off, abs, take);
            if (r.failed()) return r;
        }
        off += take;
    }
    // Directory entry.
    r = fat32::createDirectoryEntry(disk, vol_start, layout, parent_dir, name,
                                    fat32::ATTR_ARCHIVE, alloc.empty() ? 0 : alloc[0],
                                    (uint32_t)data.size());
    if (r.failed()) return r;
    // FSInfo free-cluster refresh.
    uint32_t free_count = fat32::getFreeClusterCount(fat);
    fat32::updateFSInfo(disk, vol_start, layout, free_count, 0);
    return Result::ok();
}

} // namespace

Result mergePartitions(std::shared_ptr<DiskIO> disk,
                       int number_a, int number_b,
                       const MergeOptions& options) {
    if (!disk || !disk->isOpen()) return Result::error("device not open");
    if (number_a == number_b) return Result::error("partitions must be different");

    auto table = PartitionTable::load(disk);
    if (!table) return Result::error("no partition table found");
    auto pa = table->getPartition(number_a);
    auto pb = table->getPartition(number_b);
    if (!pa) return Result::error("partition " + std::to_string(number_a) + " not found");
    if (!pb) return Result::error("partition " + std::to_string(number_b) + " not found");

    // Determin the left/right order by start sector.
    bool a_left = pa->startSector() < pb->startSector();
    Partition left = a_left ? *pa : *pb;
    Partition right = a_left ? *pb : *pa;
    // Partition objects do not carry positional numbers; the caller's numbers
    // are the 1-based positional indices into the table.
    int left_number = a_left ? number_a : number_b;
    int right_number = a_left ? number_b : number_a;

    // Adjacency: right.start must equal left.start + left.size.
    if (right.startSector() != left.startSector() + left.sectorCount()) {
        return Result::error("partitions are not adjacent (right starts at " +
                             std::to_string(right.startSector()) + ", left+size = " +
                             std::to_string(left.startSector() + left.sectorCount()) + ")");
    }

    uint64_t combined_start = left.startSector();
    uint64_t combined_sectors = left.sectorCount() + right.sectorCount();
    // resizePartition takes size in BYTES (all partition APIs do).
    uint64_t combined_size = combined_sectors * disk->sectorSize();

    // Detect filesystems.
    FileSystemType fs_left = disk->detectFilesystem(left.startSector());
    FileSystemType fs_right = disk->detectFilesystem(right.startSector());

    bool right_empty = (fs_right == FileSystemType::Unknown);

    // Fast path: right is empty (no FS) — just delete right + grow left (any FS).
    if (right_empty) {
        // Delete the right partition, resize the left to cover both, commit.
        Result r = table->deletePartition(right_number);
        if (r.failed()) return r;
        r = table->resizePartition(left_number, combined_size);
        if (r.failed()) return r;
        r = table->commit();
        if (r.failed()) return r;
        return Result::ok();
    }

    // Data-move path: both FAT32. Copy right's tree into a folder on left.
    bool both_fat32 = (fs_left == FileSystemType::FAT32 && fs_right == FileSystemType::FAT32);
    if (!both_fat32) {
        std::string a = (fs_left == FileSystemType::FAT32) ? "FAT32" : "non-FAT32";
        std::string b = (fs_right == FileSystemType::FAT32) ? "FAT32" : "non-FAT32";
        return Result::error(
            "cannot merge: source and destination must both be FAT32 for a data-preserving "
            "merge, but left=" + a + " and right=" + b +
            ". Supported: empty source partition (any FS), or FAT32 -> FAT32.");
    }

    // FAT32 -> FAT32 data-preserving merge.
    fat32::FAT32Layout layout_left, layout_right;
    fat32::FAT32BootSector bsL, bsR;
    if (fat32::getFAT32Info(disk, left.startSector(), bsL, layout_left).failed())
        return Result::error("cannot read FAT32 on left partition");
    if (fat32::getFAT32Info(disk, right.startSector(), bsR, layout_right).failed())
        return Result::error("cannot read FAT32 on right partition");

    std::string folder = options.folder_name.empty() ? "merged_" + std::to_string(right.number()) : options.folder_name;
    // Validate folder name is a safe 8.3-ish short name.
    std::string safe = fat32::createShortName(folder);
    uint32_t folder_cluster = 0;

    // Gather all files from the right volume.
    std::vector<FatEntry> files;
    std::vector<uint32_t> visited;
    Result r = collectTree(disk, right.startSector(), layout_right, files, "",
                           layout_right.root_cluster, visited);
    if (r.failed()) return r;

    // Create the target folder on the left volume first.
    r = ensureFolder(disk, left.startSector(), layout_left, safe, folder_cluster);
    if (r.failed()) return r;

    // Copy each file under its original sub-path.
    for (const auto& fe : files) {
        // Read from right.
        std::vector<uint8_t> data;
        r = readFileBytes(disk, right.startSector(), layout_right, fe.start_cluster,
                          fe.size_bytes, data);
        if (r.failed()) return r;
        // Determine destination folder (mirror sub-paths).
        std::string dest_path = safe;
        if (!fe.parent_path.empty()) dest_path += "/" + fe.parent_path;
        uint32_t dest_cluster = 0;
        r = ensureFolder(disk, left.startSector(), layout_left, dest_path, dest_cluster);
        if (r.failed()) return r;
        r = writeFile(disk, left.startSector(), layout_left, dest_cluster,
                      fe.name, data);
        if (r.failed()) return r;
    }

    // Now remove the right partition and grow the left.
    r = table->deletePartition(right_number);
    if (r.failed()) return r;
    r = table->resizePartition(left_number, combined_size);
    if (r.failed()) return r;
    r = table->commit();
    if (r.failed()) return r;

    return Result::ok();
}

} // namespace opm