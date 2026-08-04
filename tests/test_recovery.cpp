#include <gtest/gtest.h>
#include "opm/recovery.hpp"
#include "opm/partition_table.hpp"
#include "opm/disk_io.hpp"
#include "opm/fat32_impl.hpp"
#include "opm/ext4_impl.hpp"
#include <cstdio>
#include <vector>

using namespace opm;

namespace {

bool makeImage(uint64_t mb, std::string& path) {
    path = "/tmp/opm_rec_" + std::to_string(::getpid()) + "_" +
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

} // namespace

TEST(RecoveryTest, FindsOrphanFilesystemWithoutTable) {
    std::string path; ASSERT_TRUE(makeImage(96, path));
    auto disk = DiskIO::openReadWrite(path);
    ASSERT_TRUE(disk && disk->isOpen());

    // Write a raw FAT32 volume at sector 2048 — no partition table at all.
    Result r = fat32::formatFAT32Complete(disk, 2048, 64ULL * 1024 * 1024, "LOST");
    ASSERT_TRUE(r.success()) << r.message;

    auto candidates = scanForPartitions(disk, 2048);
    bool found = false;
    for (const auto& c : candidates) {
        if (c.start_sector == 2048 && c.fs == FileSystemType::FAT32) found = true;
    }
    EXPECT_TRUE(found) << "signature scan should find the orphan FAT32";

    // Rebuild an MBR table from what we found.
    r = rebuildPartitionTable(disk, candidates);
    ASSERT_TRUE(r.success()) << r.message;

    auto table = PartitionTable::load(disk);
    ASSERT_TRUE(table);
    EXPECT_EQ(table->type(), TableType::MBR);
    EXPECT_GE(table->getPartitionCount(), 1);

    // Data preserved: the filesystem must still be detectable in place.
    EXPECT_EQ(disk->detectFilesystem(2048), FileSystemType::FAT32);

    disk->close(); std::remove(path.c_str());
}

TEST(RecoveryTest, FindsMultipleOrphans) {
    std::string path; ASSERT_TRUE(makeImage(256, path));
    auto disk = DiskIO::openReadWrite(path);
    ASSERT_TRUE(disk && disk->isOpen());

    // FAT32 at 2048, ext4 at 100352 (both 1 MiB-aligned probes).
    Result r = fat32::formatFAT32Complete(disk, 2048, 64ULL * 1024 * 1024, "A");
    ASSERT_TRUE(r.success());
    r = ext4::formatEXT4(disk, 100352, 64ULL * 1024 * 1024, "B");
    ASSERT_TRUE(r.success());

    auto candidates = scanForPartitions(disk, 2048);
    int fat = 0, ext = 0;
    for (const auto& c : candidates) {
        if (c.fs == FileSystemType::FAT32) fat++;
        if (c.fs == FileSystemType::EXT4) ext++;
    }
    EXPECT_GE(fat, 1);
    EXPECT_GE(ext, 1);

    r = rebuildPartitionTable(disk, candidates);
    ASSERT_TRUE(r.success()) << r.message;
    auto table = PartitionTable::load(disk);
    ASSERT_TRUE(table);
    EXPECT_GE(table->getPartitionCount(), 2);
    EXPECT_EQ(disk->detectFilesystem(2048), FileSystemType::FAT32);
    EXPECT_EQ(disk->detectFilesystem(100352), FileSystemType::EXT4);

    disk->close(); std::remove(path.c_str());
}

TEST(RecoveryTest, EmptyDiskFindsNothing) {
    std::string path; ASSERT_TRUE(makeImage(32, path));
    auto disk = DiskIO::openReadWrite(path);
    ASSERT_TRUE(disk && disk->isOpen());

    auto candidates = scanForPartitions(disk, 2048);
    EXPECT_TRUE(candidates.empty());

    disk->close(); std::remove(path.c_str());
}

TEST(RecoveryTest, ExistingTableReportedAsCandidates) {
    std::string path; ASSERT_TRUE(makeImage(96, path));
    auto disk = DiskIO::openReadWrite(path);
    ASSERT_TRUE(disk && disk->isOpen());

    auto mbr = MBRTable::createNew(disk);
    Result r = mbr->createPartition(2048, 40ULL * 1024 * 1024,
                                    PartitionType::Linux, "root");
    ASSERT_TRUE(r.success());
    r = mbr->commit();
    ASSERT_TRUE(r.success());

    auto candidates = scanForPartitions(disk, 2048);
    bool found = false;
    for (const auto& c : candidates) {
        if (c.start_sector == 2048 && c.from_partition_table) found = true;
    }
    EXPECT_TRUE(found);

    disk->close(); std::remove(path.c_str());
}
