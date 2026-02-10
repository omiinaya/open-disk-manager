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
    
    // Partition 3 and 4 are empty
    
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
