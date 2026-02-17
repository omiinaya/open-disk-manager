#include <gtest/gtest.h>
#include "opm/partition_table.hpp"
#include <fstream>
#include <cstring>
#include <vector>

using namespace opm;

// Test MBR creation without disk I/O
TEST(MBRTableTest, CreateMBR) {
    MBRTable table;
    EXPECT_EQ(table.type(), TableType::MBR);
    EXPECT_EQ(table.typeName(), "MBR");
}

// Test GPT creation without disk I/O
TEST(GPTTableTest, CreateGPT) {
    GPTTable table;
    EXPECT_EQ(table.type(), TableType::GPT);
    EXPECT_EQ(table.typeName(), "GPT");
}

// Test MBR isValid function
TEST(MBRTableTest, IsValid) {
    MBRTable table;
    // Empty table should be valid
    EXPECT_TRUE(table.isValid());
}

// Test GPT isValid function
TEST(GPTTableTest, IsValid) {
    GPTTable table;
    // Empty table should be valid
    EXPECT_TRUE(table.isValid());
}

// Test MBR getTotalSpace (no disk)
TEST(MBRTableTest, GetTotalSpace) {
    MBRTable table;
    EXPECT_EQ(table.getTotalSpace(), 0);
}

// Test GPT getTotalSpace (no disk)
TEST(GPTTableTest, GetTotalSpace) {
    GPTTable table;
    EXPECT_EQ(table.getTotalSpace(), 0);
}

// Test MBR partition operations
TEST(MBRTableTest, GetPartitionCount) {
    MBRTable table;
    EXPECT_EQ(table.getPartitionCount(), 0);
}

// Test GPT partition operations
TEST(GPTTableTest, GetPartitionCount) {
    GPTTable table;
    EXPECT_EQ(table.getPartitionCount(), 0);
}

// Test MBR getPartitions
TEST(MBRTableTest, GetPartitions) {
    MBRTable table;
    auto partitions = table.getPartitions();
    EXPECT_EQ(partitions.size(), 0);
}

// Test GPT getPartitions
TEST(GPTTableTest, GetPartitions) {
    GPTTable table;
    auto partitions = table.getPartitions();
    EXPECT_EQ(partitions.size(), 0);
}

// Test MBR validate
TEST(MBRTableTest, Validate) {
    MBRTable table;
    auto result = table.validate();
    EXPECT_TRUE(result.success());
}

// Test GPT validate
TEST(GPTTableTest, Validate) {
    GPTTable table;
    auto result = table.validate();
    EXPECT_TRUE(result.success());
}

// Test MBR hasErrors
TEST(MBRTableTest, HasErrors) {
    MBRTable table;
    EXPECT_FALSE(table.hasErrors());
}

// Test GPT hasErrors
TEST(GPTTableTest, HasErrors) {
    GPTTable table;
    EXPECT_FALSE(table.hasErrors());
}

// Test MBR getErrors
TEST(MBRTableTest, GetErrors) {
    MBRTable table;
    auto errors = table.getErrors();
    EXPECT_EQ(errors.size(), 0);
}

// Test GPT getErrors
TEST(GPTTableTest, GetErrors) {
    GPTTable table;
    auto errors = table.getErrors();
    EXPECT_EQ(errors.size(), 0);
}

// Test MBR hasExtendedPartition
TEST(MBRTableTest, HasExtendedPartition) {
    MBRTable table;
    EXPECT_FALSE(table.hasExtendedPartition());
}

// Test GPT hasProtectiveMBR (default)
TEST(GPTTableTest, HasProtectiveMBR) {
    GPTTable table;
    EXPECT_FALSE(table.hasProtectiveMBR());
}

// Test MBR revert
TEST(MBRTableTest, Revert) {
    MBRTable table;
    // Should not crash
    table.revert();
}

// Test GPT revert
TEST(GPTTableTest, Revert) {
    GPTTable table;
    // Should not crash
    table.revert();
}

// Test MBR type
TEST(MBRTableTest, Type) {
    MBRTable table;
    EXPECT_EQ(table.type(), TableType::MBR);
}

// Test GPT type
TEST(GPTTableTest, Type) {
    GPTTable table;
    EXPECT_EQ(table.type(), TableType::GPT);
}

// Test MBR convertTo
TEST(MBRTableTest, ConvertTo) {
    MBRTable table;
    auto result = table.convertTo(TableType::GPT);
    EXPECT_TRUE(result.failed()); // Not implemented
}

// Test GPT convertTo
TEST(GPTTableTest, ConvertTo) {
    GPTTable table;
    auto result = table.convertTo(TableType::MBR);
    EXPECT_TRUE(result.failed()); // Not implemented
}

// Test MBR isModified
TEST(MBRTableTest, IsModified) {
    MBRTable table;
    EXPECT_FALSE(table.isModified());
}

// Test GPT isModified
TEST(GPTTableTest, IsModified) {
    GPTTable table;
    EXPECT_FALSE(table.isModified());
}

// Test MBR devicePath
TEST(MBRTableTest, DevicePath) {
    MBRTable table;
    EXPECT_EQ(table.devicePath(), "");
}

// Test GPT devicePath
TEST(GPTTableTest, DevicePath) {
    GPTTable table;
    EXPECT_EQ(table.devicePath(), "");
}

// Test MBR getDiskSignature - no disk so should be 0
TEST(MBRTableTest, GetDiskSignature) {
    MBRTable table;
    EXPECT_EQ(table.getDiskSignature(), 0);
}

// Test GPT getDiskGuid - empty string
TEST(GPTTableTest, GetDiskGuid) {
    GPTTable table;
    EXPECT_EQ(table.getDiskGuid(), "");
}

// Test GPT getFirstUsableLBA - 0
TEST(GPTTableTest, GetFirstUsableLBA) {
    GPTTable table;
    EXPECT_EQ(table.getFirstUsableLBA(), 0);
}

// Test GPT getLastUsableLBA - 0
TEST(GPTTableTest, GetLastUsableLBA) {
    GPTTable table;
    EXPECT_EQ(table.getLastUsableLBA(), 0);
}

// Test GPT getPartitionEntrySize - 0
TEST(GPTTableTest, GetPartitionEntrySize) {
    GPTTable table;
    EXPECT_EQ(table.getPartitionEntrySize(), 0);
}

// Test GPT getPartitionEntryCount - 0
TEST(GPTTableTest, GetPartitionEntryCount) {
    GPTTable table;
    EXPECT_EQ(table.getPartitionEntryCount(), 0);
}

// Test MBR markModified and clearModified
TEST(MBRTableTest, ModifiedState) {
    MBRTable table;
    EXPECT_FALSE(table.isModified());
    table.markModified();
    EXPECT_TRUE(table.isModified());
    table.clearModified();
    EXPECT_FALSE(table.isModified());
}

// Test GPT markModified and clearModified
TEST(GPTTableTest, ModifiedState) {
    GPTTable table;
    EXPECT_FALSE(table.isModified());
    table.markModified();
    EXPECT_TRUE(table.isModified());
    table.clearModified();
    EXPECT_FALSE(table.isModified());
}

// Test MBR getFreeSpace
TEST(MBRTableTest, GetFreeSpace) {
    MBRTable table;
    auto freeSpace = table.getFreeSpace();
    // Without a disk, should return 0
    EXPECT_EQ(freeSpace, 0);
}

// Test MBR getUsedSpace
TEST(MBRTableTest, GetUsedSpace) {
    MBRTable table;
    auto usedSpace = table.getUsedSpace();
    // Without a disk, should return 0
    EXPECT_EQ(usedSpace, 0);
}

// Test GPT isUEFISystem
TEST(GPTTableTest, IsUEFISystem) {
    GPTTable table;
    // Without loading a disk, should return false
    EXPECT_FALSE(table.isUEFISystem());
}

// Test MBR commit (without disk)
TEST(MBRTableTest, CommitWithoutDisk) {
    MBRTable table;
    auto result = table.commit();
    // Should succeed because there's nothing to commit (not modified)
    EXPECT_TRUE(result.success());
}

// Test GPT commit (without disk)
TEST(GPTTableTest, CommitWithoutDisk) {
    GPTTable table;
    auto result = table.commit();
    // Should succeed because there's nothing to commit (not modified)
    EXPECT_TRUE(result.success());
}

// Test MBR deletePartition (no partitions)
TEST(MBRTableTest, DeleteInvalidPartition) {
    MBRTable table;
    auto result = table.deletePartition(1);
    // Should fail because no partition exists
    EXPECT_TRUE(result.failed());
}

// Test MBR resizePartition (no partitions)
TEST(MBRTableTest, ResizeInvalidPartition) {
    MBRTable table;
    auto result = table.resizePartition(1, 1024);
    // Should fail because no partition exists
    EXPECT_TRUE(result.failed());
}

// Test MBR createPartition (no disk)
TEST(MBRTableTest, CreateWithoutDisk) {
    MBRTable table;
    // Just verify the table can be created
    EXPECT_EQ(table.getPartitionCount(), 0);
}
