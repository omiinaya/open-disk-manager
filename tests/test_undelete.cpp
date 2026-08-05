#include <gtest/gtest.h>
#include "opm/undelete.hpp"
#include "opm/fat32_impl.hpp"
#include "opm/disk_io.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
#include "test_util.hpp"

using namespace opm;

namespace {

bool makeImage(uint64_t mb, std::string& path) {
    path = test_tmp_dir() + "/opm_undel_" + std::to_string(::getpid()) + "_" +
           std::to_string(std::rand()) + ".img";
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    std::vector<uint8_t> zero(1024 * 1024, 0);
    for (uint64_t i = 0; i < mb; i++) {
        if (std::fwrite(zero.data(), 1, zero.size(), f) != zero.size()) {
            std::fclose(f);
            return false;
        }
    }
    std::fclose(f);
    return true;
}

// Raw FAT helpers for the fixture (mirror the layout math).
uint32_t rawFATEntry(std::shared_ptr<DiskIO> disk, const fat32::FAT32Layout& l,
                     uint32_t cluster, bool* ok = nullptr) {
    uint64_t off = l.fat_start_sector * (uint64_t)l.bytes_per_sector + cluster * 4;
    uint8_t b[4];
    if (disk->read(b, off, 4).failed()) { if (ok) *ok = false; return 0; }
    if (ok) *ok = true;
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

void rawSetFATEntry(std::shared_ptr<DiskIO> disk, const fat32::FAT32Layout& l,
                    uint32_t cluster, uint32_t value) {
    uint64_t off = l.fat_start_sector * (uint64_t)l.bytes_per_sector + cluster * 4;
    uint8_t b[4] = { (uint8_t)(value & 0xFF), (uint8_t)((value >> 8) & 0xFF),
                     (uint8_t)((value >> 16) & 0xFF), (uint8_t)((value >> 24) & 0xFF) };
    disk->write(b, off, 4);
}

// Mark a directory entry deleted by name (set first byte to 0xE5).
bool deleteEntryByName(std::shared_ptr<DiskIO> disk, const fat32::FAT32Layout& l,
                       uint64_t start, const char* name) {
    uint64_t dir_sector = l.clusterToSector(l.root_cluster);
    uint32_t bps = l.bytes_per_sector;
    std::vector<uint8_t> data(bps, 0);
    if (disk->read(data.data(), (start + dir_sector) * bps, bps).failed()) return false;
    for (size_t off = 0; off + 32 <= data.size(); off += 32) {
        if (std::memcmp(data.data() + off, name, std::strlen(name)) == 0) {
            data[off] = 0xE5;
            return disk->write(data.data(), (start + dir_sector) * bps, bps).success();
        }
    }
    return false;
}

} // namespace

TEST(UndeleteTest, ScanAndRestoreDeletedFile) {
    std::string path; ASSERT_TRUE(makeImage(96, path));
    auto disk = DiskIO::openReadWrite(path);
    ASSERT_TRUE(disk && disk->isOpen());

    Result r = fat32::formatFAT32Complete(disk, 2048, 64ULL * 1024 * 1024, "VOL");
    ASSERT_TRUE(r.success()) << r.message;

    fat32::FAT32BootSector bs; fat32::FAT32Layout layout;
    r = fat32::getFAT32Info(disk, 2048, bs, layout);
    ASSERT_TRUE(r.success()) << r.message;

    // Create a file entry (cluster 3) with data.
    const uint32_t cluster = 3;
    const uint32_t size = 4096;
    r = fat32::createDirectoryEntry(disk, 2048, layout, layout.root_cluster,
                                    "TEST.TXT", 0x20, cluster, size);
    ASSERT_TRUE(r.success()) << r.message;

    // Write data at the cluster.
    uint64_t data_sector = layout.clusterToSector(cluster);
    std::vector<uint8_t> payload(4096, 0xAB);
    payload[0] = 'H'; payload[1] = 'I';
    r = disk->write(payload.data(), (2048 + data_sector) * layout.bytes_per_sector, 4096);
    ASSERT_TRUE(r.success());
    rawSetFATEntry(disk, layout, cluster, 0x0FFFFFFF);  // allocate
    disk->flush();

    // Simulate deletion: 0xE5 marker + free the FAT entry.
    ASSERT_TRUE(deleteEntryByName(disk, layout, 2048, "TEST"));
    rawSetFATEntry(disk, layout, cluster, 0x00000000);
    disk->flush();

    // Scan: the file must be found.
    auto files = fat32::scanDeletedFiles(disk, 2048);
    ASSERT_EQ(files.size(), 1u);
    EXPECT_EQ(files[0].start_cluster, cluster);
    EXPECT_EQ(files[0].size_bytes, size);
    EXPECT_EQ(files[0].name, "_EST.TXT");

    // Restore.
    r = fat32::restoreDeletedFile(disk, 2048, files[0], 'T');
    ASSERT_TRUE(r.success()) << r.message;

    // Entry restored with the replacement first char; FAT chain re-allocated.
    // Cluster size here is 512 B, so a 4096 B file spans 8 clusters; walk the
    // chain from the start cluster and expect it to end with an EOC marker.
    bool ok = false;
    uint32_t cl = cluster;
    uint32_t steps = 0;
    bool eoc = false;
    while (steps < 64) {
        uint32_t v = rawFATEntry(disk, layout, cl, &ok);
        ASSERT_TRUE(ok);
        if (v >= 0x0FFFFFF8) { eoc = true; break; }
        EXPECT_NE(v, 0u) << "chain broken at cluster " << cl;
        cl = v;
        steps++;
    }
    EXPECT_TRUE(eoc);

    // Data still intact.
    std::vector<uint8_t> back(4096, 0);
    ASSERT_TRUE(disk->read(back.data(), (2048 + data_sector) * layout.bytes_per_sector, 4096).success());
    EXPECT_EQ(back[0], 'H');
    EXPECT_EQ(back[1], 'I');

    // A second scan must now find nothing deleted.
    EXPECT_TRUE(fat32::scanDeletedFiles(disk, 2048).empty());

    disk->close(); std::remove(path.c_str());
}

TEST(UndeleteTest, RestoreRefusedWhenClusterOccupied) {
    std::string path; ASSERT_TRUE(makeImage(96, path));
    auto disk = DiskIO::openReadWrite(path);
    ASSERT_TRUE(disk && disk->isOpen());

    Result r = fat32::formatFAT32Complete(disk, 2048, 64ULL * 1024 * 1024, "VOL");
    ASSERT_TRUE(r.success());
    fat32::FAT32BootSector bs; fat32::FAT32Layout layout;
    r = fat32::getFAT32Info(disk, 2048, bs, layout);
    ASSERT_TRUE(r.success());

    const uint32_t cluster = 3;
    r = fat32::createDirectoryEntry(disk, 2048, layout, layout.root_cluster,
                                    "GONE.BIN", 0x20, cluster, 2048);
    ASSERT_TRUE(r.success());
    // Keep the cluster allocated (file was overwritten) and delete the entry.
    rawSetFATEntry(disk, layout, cluster, 0x0FFFFFFF);
    ASSERT_TRUE(deleteEntryByName(disk, layout, 2048, "GONE"));

    auto files = fat32::scanDeletedFiles(disk, 2048);
    ASSERT_EQ(files.size(), 1u);
    r = fat32::restoreDeletedFile(disk, 2048, files[0], 'G');
    EXPECT_TRUE(r.failed());
    EXPECT_NE(r.message.find("no longer free"), std::string::npos);

    disk->close(); std::remove(path.c_str());
}
