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

// Build a real GPT on an image file via the public API, then reload it.
bool makeImage(uint64_t mb, std::string& path) {
    path = test_tmp_dir() + "/opm_gpt_ops_" + std::to_string(::getpid()) + ".img";
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

TEST(GPTOpsTest, CreatePartitionCommitReload) {
    std::string path;
    ASSERT_TRUE(makeImage(64, path));
    auto disk = DiskIO::openReadWrite(path);
    ASSERT_TRUE(disk && disk->isOpen());

    auto table = PartitionTable::create(disk, TableType::GPT);
    ASSERT_TRUE(table);
    EXPECT_EQ(table->type(), TableType::GPT);

    Result r = table->createPartition(2048, 20ULL * 1024 * 1024,
                                      PartitionType::Linux, "p1");
    ASSERT_TRUE(r.success()) << r.message;
    r = table->createPartition(43008, 10ULL * 1024 * 1024,
                               PartitionType::LinuxSwap, "p2");
    ASSERT_TRUE(r.success()) << r.message;
    r = table->commit();
    ASSERT_TRUE(r.success()) << r.message;

    // Reload and verify
    auto reloaded = PartitionTable::load(disk);
    ASSERT_TRUE(reloaded);
    EXPECT_EQ(reloaded->type(), TableType::GPT);
    EXPECT_TRUE(reloaded->isValid());
    auto parts = reloaded->getPartitions();
    ASSERT_EQ(parts.size(), 2u);
    EXPECT_EQ(parts[0].startSector(), 2048u);
    EXPECT_EQ(parts[0].name(), "p1");
    EXPECT_EQ(parts[1].startSector(), 43008u);
    EXPECT_EQ(parts[1].name(), "p2");

    // Resize partition 2
    auto gpt = dynamic_cast<GPTTable*>(reloaded.get());
    ASSERT_NE(gpt, nullptr);
    r = gpt->resizePartition(2, 20ULL * 1024 * 1024);
    ASSERT_TRUE(r.success()) << r.message;
    r = gpt->commit();
    ASSERT_TRUE(r.success()) << r.message;

    auto reloaded2 = PartitionTable::load(disk);
    ASSERT_TRUE(reloaded2);
    parts = reloaded2->getPartitions();
    ASSERT_EQ(parts.size(), 2u);
    EXPECT_EQ(parts[1].sectorCount(), 20ULL * 1024 * 1024 / 512);

    // Delete partition 1
    auto gpt2 = dynamic_cast<GPTTable*>(reloaded2.get());
    ASSERT_NE(gpt2, nullptr);
    r = gpt2->deletePartition(1);
    ASSERT_TRUE(r.success()) << r.message;
    r = gpt2->commit();
    ASSERT_TRUE(r.success()) << r.message;

    auto reloaded3 = PartitionTable::load(disk);
    ASSERT_TRUE(reloaded3);
    EXPECT_EQ(reloaded3->getPartitionCount(), 1);

    disk->close();
    std::remove(path.c_str());
}

TEST(GPTOpsTest, RestorePrimaryFromBackup) {
    std::string path; ASSERT_TRUE(makeImage(64, path));
    auto disk = DiskIO::openReadWrite(path);

    auto table = PartitionTable::create(disk, TableType::GPT);
    Result r = table->createPartition(2048, 20ULL * 1024 * 1024,
                                      PartitionType::Linux, "p1");
    ASSERT_TRUE(r.success());
    r = table->commit();
    ASSERT_TRUE(r.success());

    // Corrupt the primary GPT header (sector 1): wipe the signature
    std::vector<uint8_t> zero(512, 0);
    r = disk->writeSector(zero.data(), 1);
    ASSERT_TRUE(r.success());

    // A fresh load should now fail to see GPT
    auto after_corrupt = PartitionTable::load(disk);
    EXPECT_EQ(after_corrupt, nullptr);

    // Recover: build a table from the backup copy
    auto recovered = GPTTable::recover(disk);
    r = recovered->restoreFromBackup();
    ASSERT_TRUE(r.success()) << r.message;
    EXPECT_EQ(recovered->getPartitionCount(), 1);

    // Commit the restored primary so future loads see a valid GPT
    r = recovered->commit();
    ASSERT_TRUE(r.success()) << r.message;

    auto final_table = PartitionTable::load(disk);
    ASSERT_TRUE(final_table);
    EXPECT_TRUE(final_table->isValid());
    auto parts = final_table->getPartitions();
    ASSERT_EQ(parts.size(), 1u);
    EXPECT_EQ(parts[0].name(), "p1");

    disk->close();
    std::remove(path.c_str());
}

TEST(GPTOpsTest, MBRCreateCommitReload) {
    std::string path; ASSERT_TRUE(makeImage(64, path));
    auto disk = DiskIO::openReadWrite(path);

    auto table = PartitionTable::create(disk, TableType::MBR);
    ASSERT_TRUE(table);
    Result r = table->createPartition(2048, 20ULL * 1024 * 1024,
                                      PartitionType::Linux, "p1");
    ASSERT_TRUE(r.success()) << r.message;
    r = table->commit();
    ASSERT_TRUE(r.success()) << r.message;

    auto reloaded = PartitionTable::load(disk);
    ASSERT_TRUE(reloaded);
    EXPECT_EQ(reloaded->type(), TableType::MBR);
    auto parts = reloaded->getPartitions();
    ASSERT_EQ(parts.size(), 1u);
    EXPECT_EQ(parts[0].startSector(), 2048u);

    disk->close();
    std::remove(path.c_str());
}
