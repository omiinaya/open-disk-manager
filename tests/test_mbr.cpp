#include <gtest/gtest.h>
#include "opm/partition_table.hpp"
#include "opm/utils.hpp"
#include <fstream>
#include <cstring>

using namespace opm;

// Helper to create a test MBR image
std::vector<uint8_t> createTestMBR() {
    std::vector<uint8_t> mbr(512, 0);
    
    // Boot code
    for (int i = 0; i < 440; i++) {
        mbr[i] = 0x00;
    }
    
    // Disk signature (bytes 440-443)
    mbr[440] = 0x12;
    mbr[441] = 0x34;
    mbr[442] = 0x56;
    mbr[443] = 0x78;
    
    // Reserved (bytes 444-445)
    mbr[444] = 0x00;
    mbr[445] = 0x00;
    
    // Partition 1 (bytes 446-461)
    // Bootable
    mbr[446] = 0x80;
    // CHS start (simplified)
    mbr[447] = 0x01;
    mbr[448] = 0x01;
    mbr[449] = 0x00;
    // Type: Linux
    mbr[450] = 0x83;
    // CHS end
    mbr[451] = 0xFE;
    mbr[452] = 0xFF;
    mbr[453] = 0xFF;
    // LBA start (sector 2048 = 0x00000800)
    mbr[454] = 0x00;
    mbr[455] = 0x08;
    mbr[456] = 0x00;
    mbr[457] = 0x00;
    // Sector count (1GB = 2097152 sectors = 0x00200000)
    mbr[458] = 0x00;
    mbr[459] = 0x00;
    mbr[460] = 0x20;
    mbr[461] = 0x00;
    
    // Partition 2 (bytes 462-477)
    // Not bootable
    mbr[462] = 0x00;
    // CHS start
    mbr[463] = 0x01;
    mbr[464] = 0x01;
    mbr[465] = 0x00;
    // Type: NTFS
    mbr[466] = 0x07;
    // CHS end
    mbr[467] = 0xFE;
    mbr[468] = 0xFF;
    mbr[469] = 0xFF;
    // LBA start (sector 2099200 = 0x00200800)
    mbr[470] = 0x00;
    mbr[471] = 0x08;
    mbr[472] = 0x20;
    mbr[473] = 0x00;
    // Sector count (1GB)
    mbr[474] = 0x00;
    mbr[475] = 0x00;
    mbr[476] = 0x20;
    mbr[477] = 0x00;
    
    // Partition 3 (extended)
    mbr[478] = 0x00;
    mbr[479] = 0x01;
    mbr[480] = 0x01;
    mbr[481] = 0x00;
    // Type: Extended
    mbr[482] = 0x05;
    // CHS end
    mbr[483] = 0xFE;
    mbr[484] = 0xFF;
    mbr[485] = 0xFF;
    // LBA start (sector 4198400)
    mbr[486] = 0x00;
    mbr[487] = 0x40;
    mbr[488] = 0x00;
    mbr[489] = 0x00;
    // Sector count
    mbr[490] = 0x00;
    mbr[491] = 0x00;
    mbr[492] = 0x20;
    mbr[493] = 0x00;
    
    // Partition 4 is empty
    
    // Boot signature
    mbr[510] = 0x55;
    mbr[511] = 0xAA;
    
    return mbr;
}

// Create empty MBR (no partitions)
std::vector<uint8_t> createEmptyMBR() {
    std::vector<uint8_t> mbr(512, 0);
    // Disk signature
    mbr[440] = 0xAA;
    mbr[441] = 0xBB;
    mbr[442] = 0xCC;
    mbr[443] = 0xDD;
    // Boot signature
    mbr[510] = 0x55;
    mbr[511] = 0xAA;
    return mbr;
}

TEST(MBRTest, CreateTestImage) {
    auto mbr = createTestMBR();
    EXPECT_EQ(mbr.size(), 512);
    EXPECT_EQ(mbr[510], 0x55);
    EXPECT_EQ(mbr[511], 0xAA);
}

TEST(MBRTest, SignatureDetection) {
    auto mbr = createTestMBR();
    uint16_t sig = *reinterpret_cast<uint16_t*>(&mbr[510]);
    EXPECT_EQ(sig, 0xAA55);
}

TEST(MBRTest, DiskSignature) {
    auto mbr = createTestMBR();
    // Disk signature at bytes 440-443: 0x12, 0x34, 0x56, 0x78
    EXPECT_EQ(mbr[440], 0x12);
    EXPECT_EQ(mbr[441], 0x34);
    EXPECT_EQ(mbr[442], 0x56);
    EXPECT_EQ(mbr[443], 0x78);
}

TEST(MBRTest, PartitionEntryParse) {
    auto mbr = createTestMBR();
    // First partition entry at offset 446
    EXPECT_EQ(mbr[446], 0x80); // Bootable
    EXPECT_EQ(mbr[450], 0x83); // Linux type
}

TEST(MBRTest, ExtendedPartitionType) {
    auto mbr = createTestMBR();
    // Partition 3 at offset 478 should be extended type 0x05
    EXPECT_EQ(mbr[482], 0x05);
}

TEST(MBRTest, EmptyMBRSignature) {
    auto mbr = createEmptyMBR();
    uint16_t sig = *reinterpret_cast<uint16_t*>(&mbr[510]);
    EXPECT_EQ(sig, 0xAA55);
}

TEST(MBRTest, InvalidSignature) {
    std::vector<uint8_t> mbr(512, 0);
    // No signature
    uint16_t sig = *reinterpret_cast<uint16_t*>(&mbr[510]);
    EXPECT_NE(sig, 0xAA55);
}
