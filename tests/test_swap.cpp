#include <gtest/gtest.h>
#include "opm/swap.hpp"
#include "opm/disk_io.hpp"
#include "opm/utils.hpp"
#include <cstdio>
#include <cstring>
#include <vector>

using namespace opm;

namespace {

bool makeImage(uint64_t mb, std::string& path) {
    path = "/tmp/opm_swap_" + std::to_string(::getpid()) + "_" +
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

TEST(SwapTest, FormatWritesSignatureAndDetects) {
    std::string path;
    ASSERT_TRUE(makeImage(16, path));
    auto disk = DiskIO::openReadWrite(path);
    ASSERT_TRUE(disk && disk->isOpen());

    // No swap signature before formatting.
    EXPECT_FALSE(isSwap(disk, 2048));
    EXPECT_NE(disk->detectFilesystem(2048), FileSystemType::Swap);

    Result r = formatSwap(disk, 2048, 8ULL * 1024 * 1024, "swapvol");
    ASSERT_TRUE(r.success()) << r.message;

    EXPECT_TRUE(isSwap(disk, 2048));
    EXPECT_EQ(disk->detectFilesystem(2048), FileSystemType::Swap);

    // Verify raw magic placement: "SWAPSPACE2" at byte 4088 (spans the first
    // 4096-byte page, so read two pages).
    uint8_t page[8192];
    ASSERT_TRUE(disk->read(page, 2048ULL * 512, 8192).success());
    EXPECT_EQ(std::memcmp(&page[4088], "SWAPSPACE2", 10), 0);
    // Version field = 1 at byte 4084.
    uint32_t version = page[4084] | (page[4085] << 8) | (page[4086] << 16) | (page[4087] << 24);
    EXPECT_EQ(version, 1u);
    // Label at byte 4068.
    EXPECT_EQ(std::memcmp(&page[4068], "swapvol", 7), 0);

    disk->close();
    std::remove(path.c_str());
}

TEST(SwapTest, FormatTooSmallFails) {
    std::string path;
    ASSERT_TRUE(makeImage(4, path));
    auto disk = DiskIO::openReadWrite(path);
    ASSERT_TRUE(disk && disk->isOpen());

    Result r = formatSwap(disk, 2048, 2048, "");  // 1 sector < 4096 bytes
    EXPECT_TRUE(r.failed());

    disk->close();
    std::remove(path.c_str());
}

TEST(SwapTest, DetectionIgnoresZeroedImage) {
    std::string path;
    ASSERT_TRUE(makeImage(4, path));
    auto disk = DiskIO::openReadWrite(path);
    ASSERT_TRUE(disk && disk->isOpen());

    EXPECT_FALSE(isSwap(disk, 2048));
    EXPECT_NE(disk->detectFilesystem(2048), FileSystemType::Swap);

    disk->close();
    std::remove(path.c_str());
}
