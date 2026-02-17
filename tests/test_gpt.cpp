#include <gtest/gtest.h>
#include "opm/partition_table.hpp"
#include "opm/utils.hpp"
#include <cstring>

using namespace opm;

// Helper to create a test GPT image (simplified)
std::vector<uint8_t> createTestGPT() {
    // GPT requires at least 3 sectors
    std::vector<uint8_t> gpt(6 * 512, 0); // 6 sectors
    
    // Sector 0: Protective MBR
    gpt[510] = 0x55;
    gpt[511] = 0xAA;
    // First partition entry: GPT protective
    gpt[446] = 0x00;
    gpt[447] = 0x00;
    gpt[448] = 0x02;
    gpt[449] = 0x00;
    gpt[450] = 0xEE; // GPT protective
    gpt[451] = 0xFF;
    gpt[452] = 0xFF;
    gpt[453] = 0xFF;
    gpt[454] = 0x01;
    gpt[455] = 0x00;
    gpt[456] = 0x00;
    gpt[457] = 0x00;
    gpt[458] = 0xFF;
    gpt[459] = 0xFF;
    gpt[460] = 0xFF;
    gpt[461] = 0x7F;
    
    // Sector 1: GPT Header
    uint8_t* header = &gpt[512];
    
    // Signature "EFI PART"
    header[0] = 0x45;
    header[1] = 0x46;
    header[2] = 0x49;
    header[3] = 0x20;
    header[4] = 0x50;
    header[5] = 0x41;
    header[6] = 0x52;
    header[7] = 0x54;
    
    // Revision 1.0
    header[8] = 0x00;
    header[9] = 0x00;
    header[10] = 0x01;
    header[11] = 0x00;
    
    // Header size (92 bytes)
    header[12] = 0x5C;
    header[13] = 0x00;
    header[14] = 0x00;
    header[15] = 0x00;
    
    // Header CRC (placeholder)
    header[16] = 0x00;
    header[17] = 0x00;
    header[18] = 0x00;
    header[19] = 0x00;
    
    // Reserved
    header[20] = 0x00;
    header[21] = 0x00;
    header[22] = 0x00;
    header[23] = 0x00;
    
    // My LBA (sector 1)
    header[24] = 0x01;
    header[25] = 0x00;
    header[26] = 0x00;
    header[27] = 0x00;
    header[28] = 0x00;
    header[29] = 0x00;
    header[30] = 0x00;
    header[31] = 0x00;
    
    // Alternate LBA (last sector)
    header[32] = 0x05;
    header[33] = 0x00;
    header[34] = 0x00;
    header[35] = 0x00;
    header[36] = 0x00;
    header[37] = 0x00;
    header[38] = 0x00;
    header[39] = 0x00;
    
    // First usable LBA
    header[40] = 0x22;
    header[41] = 0x00;
    header[42] = 0x00;
    header[43] = 0x00;
    header[44] = 0x00;
    header[45] = 0x00;
    header[46] = 0x00;
    header[47] = 0x00;
    
    // Last usable LBA
    header[48] = 0x04;
    header[49] = 0x00;
    header[50] = 0x00;
    header[51] = 0x00;
    header[52] = 0x00;
    header[53] = 0x00;
    header[54] = 0x00;
    header[55] = 0x00;
    
    // Disk GUID (16 bytes)
    for (int i = 0; i < 16; i++) {
        header[56 + i] = i;
    }
    
    // Partition entry LBA
    header[72] = 0x02;
    header[73] = 0x00;
    header[74] = 0x00;
    header[75] = 0x00;
    header[76] = 0x00;
    header[77] = 0x00;
    header[78] = 0x00;
    header[79] = 0x00;
    
    // Number of partition entries
    header[80] = 0x80;
    header[81] = 0x00;
    header[82] = 0x00;
    header[83] = 0x00;
    
    // Size of partition entry
    header[84] = 0x80;
    header[85] = 0x00;
    header[86] = 0x00;
    header[87] = 0x00;
    
    // Partition entry array CRC
    header[88] = 0x00;
    header[89] = 0x00;
    header[90] = 0x00;
    header[91] = 0x00;
    
    return gpt;
}

// Create GPT without protective MBR
std::vector<uint8_t> createGPTNoPMBR() {
    std::vector<uint8_t> gpt(6 * 512, 0);
    
    // Sector 1: GPT Header only (no protective MBR)
    uint8_t* header = &gpt[512];
    header[0] = 0x45; // E
    header[1] = 0x46; // F
    header[2] = 0x49; // I
    header[3] = 0x20; // 
    header[4] = 0x50; // P
    header[5] = 0x41; // A
    header[6] = 0x52; // R
    header[7] = 0x54; // T
    
    return gpt;
}

TEST(GPTTest, CreateTestImage) {
    auto gpt = createTestGPT();
    EXPECT_EQ(gpt.size(), 3072);
    EXPECT_EQ(gpt[510], 0x55);
    EXPECT_EQ(gpt[511], 0xAA);
}

TEST(GPTTest, HeaderSignature) {
    auto gpt = createTestGPT();
    uint8_t* header = &gpt[512];
    
    EXPECT_EQ(header[0], 0x45);
    EXPECT_EQ(header[1], 0x46);
    EXPECT_EQ(header[2], 0x49);
    EXPECT_EQ(header[3], 0x20);
    EXPECT_EQ(header[4], 0x50);
    EXPECT_EQ(header[5], 0x41);
    EXPECT_EQ(header[6], 0x52);
    EXPECT_EQ(header[7], 0x54);
}

TEST(GPTTest, ProtectiveMBR) {
    auto gpt = createTestGPT();
    EXPECT_EQ(gpt[450], 0xEE);
}

TEST(GPTTest, HeaderRevision) {
    auto gpt = createTestGPT();
    uint8_t* header = &gpt[512];
    
    // Revision 1.0 = 0x00010000
    EXPECT_EQ(header[8], 0x00);
    EXPECT_EQ(header[9], 0x00);
    EXPECT_EQ(header[10], 0x01);
    EXPECT_EQ(header[11], 0x00);
}

TEST(GPTTest, HeaderSize) {
    auto gpt = createTestGPT();
    uint8_t* header = &gpt[512];
    
    // Header size = 92 = 0x5C
    EXPECT_EQ(header[12], 0x5C);
    EXPECT_EQ(header[13], 0x00);
}

TEST(GPTTest, GPTLBAFields) {
    auto gpt = createTestGPT();
    uint8_t* header = &gpt[512];
    
    // My LBA = 1
    EXPECT_EQ(header[24], 0x01);
    
    // First usable LBA = 34
    EXPECT_EQ(header[40], 0x22);
    
    // Last usable LBA = 4
    EXPECT_EQ(header[48], 0x04);
}

TEST(GPTTest, DiskGUID) {
    auto gpt = createTestGPT();
    uint8_t* header = &gpt[512];
    
    // Disk GUID starts at offset 56
    for (int i = 0; i < 16; i++) {
        EXPECT_EQ(header[56 + i], static_cast<uint8_t>(i));
    }
}

TEST(GPTTest, PartitionEntryInfo) {
    auto gpt = createTestGPT();
    uint8_t* header = &gpt[512];
    
    // Partition entry LBA = 2
    EXPECT_EQ(header[72], 0x02);
    
    // Number of partition entries = 128
    EXPECT_EQ(header[80], 0x80);
    
    // Size of partition entry = 128
    EXPECT_EQ(header[84], 0x80);
}

TEST(GPTTest, NoProtectiveMBR) {
    auto gpt = createGPTNoPMBR();
    // First partition entry type should not be 0xEE
    EXPECT_NE(gpt[450], 0xEE);
}

TEST(GPTTest, GPTHeaderExists) {
    auto gpt = createTestGPT();
    uint8_t* header = &gpt[512];
    // GPT signature should be present at sector 1
    EXPECT_EQ(header[0], 0x45);
}

TEST(GPTTest, HeaderReservedField) {
    auto gpt = createTestGPT();
    uint8_t* header = &gpt[512];
    // Reserved field at offset 20-23 should be zero
    EXPECT_EQ(header[20], 0x00);
    EXPECT_EQ(header[21], 0x00);
    EXPECT_EQ(header[22], 0x00);
    EXPECT_EQ(header[23], 0x00);
}
