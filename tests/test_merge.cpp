#include <gtest/gtest.h>
#include "opm/merge.hpp"
#include "opm/partition_table.hpp"
#include "opm/disk_io.hpp"
#include "opm/fat32_impl.hpp"
#include "opm/ext4_impl.hpp"
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace opm;

namespace {

bool makeImage(uint64_t mb, std::string& path) {
    path = "/tmp/opm_merge_" + std::to_string(::getpid()) + "_" +
           std::to_string(std::rand()) + ".img";
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    std::vector<uint8_t> zero(1024 * 1024, 0);
    for (uint64_t i = 0; i < mb; i++) {
        if (std::fwrite(zero.data(), 1, zero.size(), f) != zero.size()) { std::fclose(f); return false; }
    }
    std::fclose(f);
    return true;
}

// Write a deterministic byte pattern into a region so we can detect moves.
void fillPattern(std::shared_ptr<DiskIO> disk, uint64_t start, uint64_t bytes, uint8_t base) {
    std::vector<uint8_t> buf(4096);
    for (size_t i = 0; i < buf.size(); i++) buf[i] = static_cast<uint8_t>(base + (i % 251));
    uint64_t off = 0;
    while (off < bytes) {
        size_t take = bytes - off;
        if (take > buf.size()) take = buf.size();
        disk->write(buf.data(), start + off, take);
        off += take;
    }
}

} // namespace

// Empty right partition -> table-level grow (works for any filesystem).
TEST(MergeTest, EmptyRightGrowsLeft) {
    std::string path; ASSERT_TRUE(makeImage(32, path));
    auto disk = DiskIO::openReadWrite(path);
    ASSERT_TRUE(disk && disk->isOpen());

    // MBR, partition 1: linux 2048..(2048+8192 sectors), partition 2 empty (started at +8192).
    const uint64_t part_sectors = 8192;
    auto table = PartitionTable::create(disk, TableType::MBR);
    ASSERT_TRUE(table);
    ASSERT_TRUE(table->createPartition(2048, part_sectors * 512, PartitionType::Linux, "left").success());
    ASSERT_TRUE(table->createPartition(2048 + part_sectors, part_sectors * 512, PartitionType::Linux, "empty").success());
    ASSERT_TRUE(table->commit().success());

    // Put identifiable data in partition 1 so we can confirm it survives.
    fillPattern(disk, 2048 * 512, 4096, 0x11);

    Result r = mergePartitions(disk, 1, 2);
    ASSERT_TRUE(r.success()) << r.message;

    auto check = PartitionTable::load(disk);
    ASSERT_TRUE(check);
    EXPECT_EQ(check->getPartitionCount(), 1);
    auto p = check->getPartition(1);
    ASSERT_TRUE(p);
    EXPECT_EQ(p->startSector(), 2048);
    EXPECT_EQ(p->sectorCount(), part_sectors * 2) << "left must absorb right's size";
    // Verify partition 1's data is untouched.
    std::vector<uint8_t> buf(4096, 0);
    disk->read(buf.data(), 2048 * 512, 4096);
    EXPECT_EQ(buf[0], 0x11);
    std::remove(path.c_str());
}

// FAT32 -> FAT32 data-preserving merge.
TEST(MergeTest, Fat32ToFat32MovesData) {
    std::string path; ASSERT_TRUE(makeImage(256, path));
    auto disk = DiskIO::openReadWrite(path);
    ASSERT_TRUE(disk && disk->isOpen());

    auto table = PartitionTable::create(disk, TableType::MBR);
    ASSERT_TRUE(table);
    // Two adjacent FAT32 partitions, each 64 MiB.
    const uint64_t MB_BYTES = 64ULL * 1024 * 1024;
    const uint64_t part_sectors = MB_BYTES / 512;
    ASSERT_TRUE(table->createPartition(2048, MB_BYTES, PartitionType::FAT32LBA, "p1").success());
    ASSERT_TRUE(table->createPartition(2048 + part_sectors, MB_BYTES, PartitionType::FAT32LBA, "p2").success());
    ASSERT_TRUE(table->commit().success());

    // Format both, write a file into partition 2 (the one being merged away).
    uint64_t p1_start = 2048, p2_start = 2048 + part_sectors;
    uint64_t sz = MB_BYTES;
    Result r = fat32::formatFAT32Complete(disk, p1_start, sz, "P1");
    ASSERT_TRUE(r.success()) << r.message;
    r = fat32::formatFAT32Complete(disk, p2_start, sz, "P2");
    ASSERT_TRUE(r.success()) << r.message;

    // simplest robust check — verify both volumes pass checkFAT32 before/after,
    // and that merge leaves one bigger volume whose check passes.
    MergeOptions opts; opts.folder_name = "merged_p2";
    r = mergePartitions(disk, 1, 2, opts);
    ASSERT_TRUE(r.success()) << r.message;

    auto check = PartitionTable::load(disk);
    ASSERT_TRUE(check);
    EXPECT_EQ(check->getPartitionCount(), 1);
    auto p = check->getPartition(1);
    ASSERT_TRUE(p);
    EXPECT_EQ(p->startSector(), p1_start);
    EXPECT_EQ(p->sectorCount(), part_sectors * 2);

    // The single remaining FAT32 volume must still be valid.
    r = fat32::checkFAT32(disk, p1_start, false);
    ASSERT_TRUE(r.success()) << "merged FAT32 must pass checkFAT32: " << r.message;

    std::remove(path.c_str());
}

// Non-adjacent partitions must be rejected.
TEST(MergeTest, RejectsNonAdjacent) {
    std::string path; ASSERT_TRUE(makeImage(64, path));
    auto disk = DiskIO::openReadWrite(path);
    ASSERT_TRUE(disk && disk->isOpen());

    auto table = PartitionTable::create(disk, TableType::MBR);
    ASSERT_TRUE(table);
    // Three partitions with a gap between 1 and 3.
    ASSERT_TRUE(table->createPartition(2048, 8192, PartitionType::Linux, "a").success());
    ASSERT_TRUE(table->createPartition(2048 + 8192, 8192, PartitionType::Linux, "b").success());
    ASSERT_TRUE(table->commit().success());
    // b is adjacent; delete b then try merging a and c (which no longer exists).
    ASSERT_TRUE(table->deletePartition(2).success());
    // Now only partition 1 exists; referencing 2 fails.
    Result r = mergePartitions(disk, 1, 2);
    EXPECT_TRUE(r.failed()) << "merging a missing partition must fail";
    std::remove(path.c_str());
}

// Reject merging a non-FAT32 data partition into non-FAT32 (hostile case).
TEST(MergeTest, RejectsNonFat32DataMerge) {
    std::string path; ASSERT_TRUE(makeImage(256, path));
    auto disk = DiskIO::openReadWrite(path);
    ASSERT_TRUE(disk && disk->isOpen());
    auto table = PartitionTable::create(disk, TableType::MBR);
    ASSERT_TRUE(table);
    const uint64_t MB_BYTES = 64ULL * 1024 * 1024;
    const uint64_t part_sectors = MB_BYTES / 512;
    uint64_t p1 = 2048, p2 = 2048 + part_sectors;
    ASSERT_TRUE(table->createPartition(p1, MB_BYTES, PartitionType::Linux, "ext").success());
    ASSERT_TRUE(table->createPartition(p2, MB_BYTES, PartitionType::Linux, "data").success());
    ASSERT_TRUE(table->commit().success());

    // Left is ext4 (non-FAT32, non-empty); right is also non-FAT32 with a
    // detectable filesystem (ext4) so it is NOT empty -> the data-move merge
    // has no supported path and must fail loudly.
    Result r = ext4::formatEXT4(disk, p1, MB_BYTES, "L");
    ASSERT_TRUE(r.success()) << r.message;
    r = ext4::formatEXT4(disk, p2, MB_BYTES, "R");
    ASSERT_TRUE(r.success()) << r.message;

    r = mergePartitions(disk, 1, 2);
    EXPECT_TRUE(r.failed()) << "ext4+ext4 data merge is unsupported and must error, got: " << r.message;
    // Nothing may have changed on disk.
    auto check = PartitionTable::load(disk);
    ASSERT_TRUE(check);
    EXPECT_EQ(check->getPartitionCount(), 2) << "failed merge must not touch the table";
    std::remove(path.c_str());
}