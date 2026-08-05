#include <gtest/gtest.h>
#include "opm/encryption.hpp"
#include "opm/disk_io.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
#include "test_util.hpp"

using namespace opm;

namespace {

std::string makeImage(const std::vector<std::pair<uint64_t, std::vector<uint8_t>>>& writes) {
    std::string path = test_tmp_dir() + "/opm_crypt_" + std::to_string(::getpid()) + ".img";
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return "";
    std::vector<uint8_t> zero(64 * 1024 * 1024, 0);
    std::fwrite(zero.data(), 1, zero.size(), f);
    for (const auto& [sector, data] : writes) {
        std::fseek(f, static_cast<long>(sector * 512), SEEK_SET);
        std::fwrite(data.data(), 1, data.size(), f);
    }
    std::fclose(f);
    return path;
}

} // namespace

TEST(EncryptionTest, DetectsLUKS1) {
    std::vector<uint8_t> sig(512, 0);
    std::memcpy(sig.data(), "LUKS\xba\xbe", 6);
    sig[6] = 0x01; sig[7] = 0x00;  // version 1
    std::string path = makeImage({{100, sig}});
    ASSERT_FALSE(path.empty());
    auto disk = DiskIO::openReadOnly(path);
    ASSERT_TRUE(disk && disk->isOpen());
    EXPECT_EQ(detectEncryption(disk, 100), EncryptionType::LUKS1);
    EXPECT_TRUE(isLUKS(disk, 100));
    EXPECT_FALSE(isBitLocker(disk, 100));
    disk->close();
    std::remove(path.c_str());
}

TEST(EncryptionTest, DetectsLUKS2) {
    std::vector<uint8_t> sig(512, 0);
    std::memcpy(sig.data(), "LUKS\xba\xbe", 6);
    sig[6] = 0x02; sig[7] = 0x00;  // version 2
    std::string path = makeImage({{200, sig}});
    ASSERT_FALSE(path.empty());
    auto disk = DiskIO::openReadOnly(path);
    ASSERT_TRUE(disk && disk->isOpen());
    EXPECT_EQ(detectEncryption(disk, 200), EncryptionType::LUKS2);
    disk->close();
    std::remove(path.c_str());
}

TEST(EncryptionTest, DetectsBitLocker) {
    std::vector<uint8_t> sig(512, 0);
    sig[0] = 0xEB; sig[1] = 0x52; sig[2] = 0x90;
    std::memcpy(sig.data() + 3, "FVE-FS", 6);
    std::string path = makeImage({{300, sig}});
    ASSERT_FALSE(path.empty());
    auto disk = DiskIO::openReadOnly(path);
    ASSERT_TRUE(disk && disk->isOpen());
    EXPECT_EQ(detectEncryption(disk, 300), EncryptionType::BitLocker);
    EXPECT_TRUE(isBitLocker(disk, 300));
    disk->close();
    std::remove(path.c_str());
}

TEST(EncryptionTest, NoSignatureIsNone) {
    std::string path = makeImage({});
    ASSERT_FALSE(path.empty());
    auto disk = DiskIO::openReadOnly(path);
    ASSERT_TRUE(disk && disk->isOpen());
    EXPECT_EQ(detectEncryption(disk, 0), EncryptionType::None);

    std::string description;
    Result r = describeEncryption(disk, 0, description);
    EXPECT_TRUE(r.success());
    EXPECT_NE(description.find("No encryption"), std::string::npos);
    disk->close();
    std::remove(path.c_str());
}
