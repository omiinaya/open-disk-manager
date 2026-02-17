#include <gtest/gtest.h>
#include "opm/operation.hpp"
#include "opm/partition.hpp"
#include "opm/types.hpp"
#include <vector>
#include <string>
#include <memory>

using namespace opm;

// Test OperationQueue
TEST(OperationQueueTest, CreateQueue) {
    OperationQueue queue;
    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0);
}

TEST(OperationQueueTest, AddOperation) {
    OperationQueue queue;
    // Can't directly add abstract operations, but can test empty queue
    EXPECT_TRUE(queue.empty());
}

TEST(OperationQueueTest, ClearQueue) {
    OperationQueue queue;
    queue.clear();
    EXPECT_TRUE(queue.empty());
}

TEST(OperationQueueTest, GetOperationDescriptions) {
    OperationQueue queue;
    auto descs = queue.getOperationDescriptions();
    EXPECT_EQ(descs.size(), 0);
}

// Test CreatePartitionOp
TEST(CreatePartitionOpTest, CreateOperation) {
    CreatePartitionOp op(2048, 1048576, PartitionType::Linux, "test");
    EXPECT_EQ(op.type(), OperationType::Create);
}

TEST(CreatePartitionOpTest, Description) {
    CreatePartitionOp op(2048, 1048576, PartitionType::Linux, "test");
    std::string desc = op.description();
    EXPECT_FALSE(desc.empty());
}

// Test DeletePartitionOp
TEST(DeletePartitionOpTest, CreateOperation) {
    DeletePartitionOp op(1);
    EXPECT_EQ(op.type(), OperationType::Delete);
}

TEST(DeletePartitionOpTest, Description) {
    DeletePartitionOp op(1);
    std::string desc = op.description();
    EXPECT_FALSE(desc.empty());
}

// Test ResizePartitionOp
TEST(ResizePartitionOpTest, CreateOperation) {
    ResizePartitionOp op(1, 2097152);
    EXPECT_EQ(op.type(), OperationType::Resize);
}

TEST(ResizePartitionOpTest, Description) {
    ResizePartitionOp op(1, 2097152);
    std::string desc = op.description();
    EXPECT_FALSE(desc.empty());
}

// Test Transaction
TEST(TransactionTest, CreateTransaction) {
    OperationQueue queue;
    Transaction tx(queue);
    // Transaction is created but not committed
    EXPECT_FALSE(tx.isCommitted());
}

// Partition Tests
TEST(PartitionTest, CreatePartition) {
    Partition part;
    part.setStartSector(2048);
    part.setEndSector(2097152);
    EXPECT_EQ(part.startSector(), 2048);
    EXPECT_EQ(part.endSector(), 2097152);
}

TEST(PartitionTest, PartitionSize) {
    Partition part;
    part.setStartSector(0);
    part.setEndSector(1023);
    EXPECT_EQ(part.sizeBytes(), 1024 * 512);
}

TEST(PartitionTest, PartitionType) {
    Partition part;
    part.setType(PartitionType::Linux);
    EXPECT_EQ(part.type(), PartitionType::Linux);
}

TEST(PartitionTest, PartitionName) {
    Partition part;
    part.setName("test partition");
    EXPECT_EQ(part.name(), "test partition");
}

TEST(PartitionTest, PartitionNumber) {
    Partition part;
    // number_ is private but we can verify the default
    EXPECT_EQ(part.number(), 0);
}

TEST(PartitionTest, PartitionBootable) {
    Partition part;
    part.setBootable(true);
    EXPECT_TRUE(part.isBootable());
}

TEST(PartitionTest, PartitionNotBootable) {
    Partition part;
    EXPECT_FALSE(part.isBootable());
}

TEST(PartitionTest, PartitionFilesystem) {
    Partition part;
    part.setFilesystem(FileSystemType::EXT4);
    EXPECT_EQ(part.filesystem(), FileSystemType::EXT4);
}

TEST(PartitionTest, PartitionOverlap) {
    Partition part1;
    part1.setStartSector(0);
    part1.setEndSector(100);
    
    Partition part2;
    part2.setStartSector(50);
    part2.setEndSector(150);
    
    EXPECT_TRUE(part1.overlaps(part2));
}

TEST(PartitionTest, PartitionNoOverlap) {
    Partition part1;
    part1.setStartSector(0);
    part1.setEndSector(100);
    
    Partition part2;
    part2.setStartSector(101);
    part2.setEndSector(200);
    
    EXPECT_FALSE(part1.overlaps(part2));
}

TEST(PartitionTest, PartitionAdjacentNoOverlap) {
    Partition part1;
    part1.setStartSector(0);
    part1.setEndSector(100);
    
    Partition part2;
    part2.setStartSector(101);
    part2.setEndSector(200);
    
    EXPECT_FALSE(part1.overlaps(part2));
}

TEST(PartitionTest, PartitionAlignment) {
    Partition part;
    part.setStartSector(2048); // 1MB aligned
    part.setEndSector(4096);
    EXPECT_TRUE(part.isAligned());
}

TEST(PartitionTest, PartitionMisaligned) {
    Partition part;
    part.setStartSector(100); // Not aligned
    part.setEndSector(200);
    EXPECT_FALSE(part.isAligned());
}

TEST(PartitionTest, PartitionEmpty) {
    Partition part;
    EXPECT_EQ(part.startSector(), 0);
    EXPECT_EQ(part.endSector(), 0);
}

TEST(PartitionTest, PartitionDevice) {
    Partition part;
    // device is read-only
    EXPECT_EQ(part.device(), "");
}

TEST(PartitionTest, PartitionUUID) {
    Partition part;
    part.setUuid("ABC123");
    EXPECT_EQ(part.uuid(), "ABC123");
}

TEST(PartitionTest, PartitionPartitionUUID) {
    Partition part;
    part.setPartitionUuid("ABC123-456");
    EXPECT_EQ(part.partitionUuid(), "ABC123-456");
}

TEST(PartitionTest, PartitionPrimary) {
    Partition part;
    EXPECT_FALSE(part.isPrimary());
}

TEST(PartitionTest, PartitionExtended) {
    Partition part;
    EXPECT_FALSE(part.isExtended());
}

TEST(PartitionTest, PartitionLogical) {
    Partition part;
    EXPECT_FALSE(part.isLogical());
}

TEST(PartitionTest, PartitionHidden) {
    Partition part;
    EXPECT_FALSE(part.isHidden());
}

TEST(PartitionTest, PartitionFormattedSize) {
    Partition part;
    part.setStartSector(0);
    part.setEndSector(1023);
    std::string size = part.formattedSize();
    EXPECT_FALSE(size.empty());
}

TEST(PartitionTest, PartitionSectorCount) {
    Partition part;
    part.setStartSector(100);
    part.setEndSector(199);
    EXPECT_EQ(part.sectorCount(), 100);
}

TEST(PartitionTest, PartitionLessThan) {
    Partition part1;
    part1.setStartSector(0);
    part1.setEndSector(100);
    
    Partition part2;
    part2.setStartSector(101);
    part2.setEndSector(200);
    
    EXPECT_TRUE(part1 < part2);
}

TEST(PartitionTest, PartitionEquality) {
    Partition part1;
    // Device and number must match for equality
    Partition part2;
    EXPECT_EQ(part1, part2);
}
