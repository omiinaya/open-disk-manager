#include <gtest/gtest.h>
#include "opm/utils.hpp"

using namespace opm;

TEST(UtilsTest, FormatBytes) {
    EXPECT_EQ(utils::formatBytes(512), "512 B");
    EXPECT_EQ(utils::formatBytes(1024), "1.00 KB");
    EXPECT_EQ(utils::formatBytes(1024 * 1024), "1.00 MB");
}

TEST(UtilsTest, CRC32) {
    const char* test_data = "123456789";
    uint32_t crc = utils::crc32(reinterpret_cast<const uint8_t*>(test_data), 9);
    EXPECT_EQ(crc, 0xCBF43926);
}

TEST(UtilsTest, Alignment) {
    EXPECT_TRUE(utils::isAligned(2048, 2048));
    EXPECT_FALSE(utils::isAligned(2049, 2048));
    EXPECT_EQ(utils::alignUp(2049, 2048), 4096);
}
