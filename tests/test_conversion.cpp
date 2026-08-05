#include <gtest/gtest.h>
#include "opm/partition_table.hpp"
#include "opm/disk_io.hpp"
#include "opm/utils.hpp"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include "test_util.hpp"

using namespace opm;

namespace {

bool makeImage(uint64_t mb, std::string& path) {
    path = test_tmp_dir() + "/opm_conv_" + std::to_string(::getpid()) + "_" +
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

// ---------------------------------------------------------------------------
// MBR -> GPT
// ---------------------------------------------------------------------------

TEST(ConversionTest, MBRToGPTPreservesPartitions) {
    std::string path;
    ASSERT_TRUE(makeImage(128, path));
    auto disk = DiskIO::openReadWrite(path);
    ASSERT_TRUE(disk && disk->isOpen());

    // Build an MBR table with two partitions.
    auto mbr = MBRTable::createNew(disk);
    ASSERT_TRUE(mbr);
    Result r = mbr->createPartition(2048, 40ULL * 1024 * 1024,
                                    PartitionType::Linux, "root");
    ASSERT_TRUE(r.success()) << r.message;
    r = mbr->createPartition(83968, 20ULL * 1024 * 1024,
                             PartitionType::LinuxSwap, "swap");
    ASSERT_TRUE(r.success()) << r.message;
    r = mbr->setPartitionBootable(1, true);
    ASSERT_TRUE(r.success()) << r.message;
    r = mbr->commit();
    ASSERT_TRUE(r.success()) << r.message;

    // Convert in place.
    r = mbr->convertTo(TableType::GPT);
    ASSERT_TRUE(r.success()) << r.message;

    // A stale commit on the old object must be a no-op (must NOT rewrite the
    // MBR over the fresh GPT).
    r = mbr->commit();
    ASSERT_TRUE(r.success()) << r.message;

    // Reload: must now be GPT with both partitions preserved.
    auto reloaded = PartitionTable::load(disk);
    ASSERT_TRUE(reloaded);
    EXPECT_EQ(reloaded->type(), TableType::GPT);
    auto parts = reloaded->getPartitions();
    ASSERT_EQ(parts.size(), 2u);
    EXPECT_EQ(parts[0].startSector(), 2048u);
    EXPECT_EQ(parts[0].type(), PartitionType::Linux);
    // MBR has no name field, so a converted partition has no name to carry.
    EXPECT_EQ(parts[0].name(), "");
    EXPECT_EQ(parts[1].startSector(), 83968u);
    EXPECT_EQ(parts[1].type(), PartitionType::LinuxSwap);

    disk->close();
    std::remove(path.c_str());
}

TEST(ConversionTest, GPTToMBRPreservesPartitions) {
    std::string path;
    ASSERT_TRUE(makeImage(128, path));
    auto disk = DiskIO::openReadWrite(path);
    ASSERT_TRUE(disk && disk->isOpen());

    auto gpt = GPTTable::createNew(disk);
    ASSERT_TRUE(gpt);
    Result r = gpt->createPartition(2048, 40ULL * 1024 * 1024,
                                    PartitionType::Linux, "root");
    ASSERT_TRUE(r.success()) << r.message;
    r = gpt->createPartition(83968, 20ULL * 1024 * 1024,
                             PartitionType::LinuxSwap, "swap");
    ASSERT_TRUE(r.success()) << r.message;
    r = gpt->commit();
    ASSERT_TRUE(r.success()) << r.message;

    r = gpt->convertTo(TableType::MBR);
    ASSERT_TRUE(r.success()) << r.message;

    r = gpt->commit();
    ASSERT_TRUE(r.success()) << r.message;

    auto reloaded = PartitionTable::load(disk);
    ASSERT_TRUE(reloaded);
    EXPECT_EQ(reloaded->type(), TableType::MBR);
    auto parts = reloaded->getPartitions();
    ASSERT_EQ(parts.size(), 2u);
    EXPECT_EQ(parts[0].startSector(), 2048u);
    EXPECT_EQ(parts[0].type(), PartitionType::Linux);
    EXPECT_EQ(parts[1].startSector(), 83968u);
    EXPECT_EQ(parts[1].type(), PartitionType::LinuxSwap);

    disk->close();
    std::remove(path.c_str());
}

TEST(ConversionTest, GPTToMBRRefusesTooManyPartitions) {
    std::string path;
    ASSERT_TRUE(makeImage(256, path));
    auto disk = DiskIO::openReadWrite(path);
    ASSERT_TRUE(disk && disk->isOpen());

    auto gpt = GPTTable::createNew(disk);
    ASSERT_TRUE(gpt);
    const uint64_t starts[6] = {2048, 43008, 83968, 124928, 165888, 206848};
    for (int i = 0; i < 6; i++) {
        Result r = gpt->createPartition(starts[i], 20ULL * 1024 * 1024,
                                        PartitionType::Linux, "p" + std::to_string(i + 1));
        ASSERT_TRUE(r.success()) << r.message;
    }
    Result r = gpt->commit();
    ASSERT_TRUE(r.success()) << r.message;

    Result conv = gpt->convertTo(TableType::MBR);
    EXPECT_TRUE(conv.failed());
    EXPECT_NE(conv.message.find("at most 4"), std::string::npos);

    // Disk must still be GPT (conversion refused, nothing written).
    auto reloaded = PartitionTable::load(disk);
    ASSERT_TRUE(reloaded);
    EXPECT_EQ(reloaded->type(), TableType::GPT);
    EXPECT_EQ(reloaded->getPartitionCount(), 6);

    disk->close();
    std::remove(path.c_str());
}

TEST(ConversionTest, ConvertWhenAlreadyTargetFails) {
    std::string path;
    ASSERT_TRUE(makeImage(64, path));
    auto disk = DiskIO::openReadWrite(path);
    ASSERT_TRUE(disk && disk->isOpen());

    auto gpt = GPTTable::createNew(disk);
    Result r = gpt->convertTo(TableType::GPT);
    EXPECT_TRUE(r.failed());

    disk->close();
    std::remove(path.c_str());
}

TEST(ConversionTest, ConvertPartitionTableFreeFunctionMBRToGPT) {
    std::string path;
    ASSERT_TRUE(makeImage(64, path));
    auto disk = DiskIO::openReadWrite(path);
    ASSERT_TRUE(disk && disk->isOpen());

    auto mbr = MBRTable::createNew(disk);
    Result r = mbr->createPartition(2048, 20ULL * 1024 * 1024,
                                    PartitionType::Linux, "x");
    ASSERT_TRUE(r.success());
    r = mbr->commit();
    ASSERT_TRUE(r.success());

    r = convertPartitionTable(disk, TableType::GPT);
    ASSERT_TRUE(r.success()) << r.message;

    auto reloaded = PartitionTable::load(disk);
    ASSERT_TRUE(reloaded);
    EXPECT_EQ(reloaded->type(), TableType::GPT);
    EXPECT_EQ(reloaded->getPartitionCount(), 1);

    disk->close();
    std::remove(path.c_str());
}

// ---------------------------------------------------------------------------
// MBR flag operations (active/bootable + hide/unhide)
// ---------------------------------------------------------------------------

TEST(MBRFlagTest, SetPartitionBootableRoundTrip) {
    std::string path;
    ASSERT_TRUE(makeImage(64, path));
    auto disk = DiskIO::openReadWrite(path);
    ASSERT_TRUE(disk && disk->isOpen());

    auto mbr = MBRTable::createNew(disk);
    Result r = mbr->createPartition(2048, 20ULL * 1024 * 1024,
                                    PartitionType::Linux, "p1");
    ASSERT_TRUE(r.success());
    r = mbr->createPartition(43008, 10ULL * 1024 * 1024,
                             PartitionType::NTFS, "p2");
    ASSERT_TRUE(r.success());

    EXPECT_FALSE(mbr->getPartitions()[0].isBootable());
    r = mbr->setPartitionBootable(1, true);
    ASSERT_TRUE(r.success());
    EXPECT_TRUE(mbr->getPartitions()[0].isBootable());
    r = mbr->commit();
    ASSERT_TRUE(r.success());

    auto reloaded = PartitionTable::load(disk);
    ASSERT_TRUE(reloaded);
    EXPECT_EQ(reloaded->type(), TableType::MBR);
    auto parts = reloaded->getPartitions();
    ASSERT_EQ(parts.size(), 2u);
    EXPECT_TRUE(parts[0].isBootable());
    EXPECT_FALSE(parts[1].isBootable());

    // Toggle back off.
    auto mbr2 = dynamic_cast<MBRTable*>(reloaded.get());
    ASSERT_NE(mbr2, nullptr);
    r = mbr2->setPartitionBootable(1, false);
    ASSERT_TRUE(r.success());
    r = mbr2->commit();
    ASSERT_TRUE(r.success());
    auto reloaded2 = PartitionTable::load(disk);
    ASSERT_TRUE(reloaded2);
    EXPECT_FALSE(reloaded2->getPartitions()[0].isBootable());

    disk->close();
    std::remove(path.c_str());
}

TEST(MBRFlagTest, HideUnhideRoundTrip) {
    std::string path;
    ASSERT_TRUE(makeImage(64, path));
    auto disk = DiskIO::openReadWrite(path);
    ASSERT_TRUE(disk && disk->isOpen());

    auto mbr = MBRTable::createNew(disk);
    Result r = mbr->createPartition(2048, 20ULL * 1024 * 1024,
                                    PartitionType::NTFS, "win");
    ASSERT_TRUE(r.success());
    r = mbr->createPartition(43008, 10ULL * 1024 * 1024,
                             PartitionType::Linux, "lin");
    ASSERT_TRUE(r.success());

    // Hide the NTFS partition (0x07 -> 0x17).
    r = mbr->setPartitionHidden(1, true);
    ASSERT_TRUE(r.success());
    r = mbr->commit();
    ASSERT_TRUE(r.success());

    auto reloaded = PartitionTable::load(disk);
    ASSERT_TRUE(reloaded);
    auto parts = reloaded->getPartitions();
    ASSERT_EQ(parts.size(), 2u);
    EXPECT_EQ(static_cast<int>(parts[0].type()), 0x17);
    EXPECT_TRUE(parts[0].isHidden());

    // Hiding a non-FAT-family partition (Linux 0x83) must be refused.
    auto mbr2 = dynamic_cast<MBRTable*>(reloaded.get());
    ASSERT_NE(mbr2, nullptr);
    r = mbr2->setPartitionHidden(2, true);
    EXPECT_TRUE(r.failed());

    // Unhide.
    r = mbr2->setPartitionHidden(1, false);
    ASSERT_TRUE(r.success());
    r = mbr2->commit();
    ASSERT_TRUE(r.success());
    auto reloaded2 = PartitionTable::load(disk);
    ASSERT_TRUE(reloaded2);
    EXPECT_EQ(static_cast<int>(reloaded2->getPartitions()[0].type()), 0x07);

    disk->close();
    std::remove(path.c_str());
}
