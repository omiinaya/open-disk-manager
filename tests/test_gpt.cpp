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
