#include <gtest/gtest.h>
#include "opm/backup.hpp"
#include "opm/disk_io.hpp"
#include <cstdio>
#include <cstring>
#include <vector>

using namespace opm;

namespace {

// Create an image file with deterministic pseudo-random content seeded by `seed`
// so tests can verify exact block equality after restore.
bool makeSourceImage(const std::string& path, uint64_t mb, uint32_t seed) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    std::vector<uint8_t> buf(1024 * 1024);
    uint32_t s = seed;
    for (uint64_t m = 0; m < mb; m++) {
        for (size_t i = 0; i < buf.size(); i++) {
            s = s * 1664525u + 1013904223u;
            buf[i] = static_cast<uint8_t>(s >> 24);
        }
        if (std::fwrite(buf.data(), 1, buf.size(), f) != buf.size()) {
            std::fclose(f);
            return false;
        }
    }
    std::fclose(f);
    return true;
}

// Write one byte pattern into a slice of the image (simulating "data change").
bool pokeImage(const std::string& path, uint64_t offset_bytes, uint8_t value) {
    std::FILE* f = std::fopen(path.c_str(), "r+b");
    if (!f) return false;
    if (std::fseek(f, static_cast<long>(offset_bytes), SEEK_SET) != 0) {
        std::fclose(f);
        return false;
    }
    if (std::fwrite(&value, 1, 1, f) != 1) {
        std::fclose(f);
        return false;
    }
    std::fclose(f);
    return true;
}

bool filesEqual(const std::string& a, const std::string& b) {
    std::FILE* fa = std::fopen(a.c_str(), "rb");
    std::FILE* fb = std::fopen(b.c_str(), "rb");
    if (!fa || !fb) { if (fa) std::fclose(fa); if (fb) std::fclose(fb); return false; }
    uint8_t ba[65536], bb[65536];
    while (true) {
        size_t na = std::fread(ba, 1, sizeof(ba), fa);
        size_t nb = std::fread(bb, 1, sizeof(bb), fb);
        if (na != nb) { std::fclose(fa); std::fclose(fb); return false; }
        if (na == 0) break;
        if (std::memcmp(ba, bb, na) != 0) { std::fclose(fa); std::fclose(fb); return false; }
    }
    std::fclose(fa); std::fclose(fb);
    return true;
}

std::string tmpPath(const char* tag) {
    return std::string("/tmp/opm_bk_") + tag + "_" +
           std::to_string(::getpid()) + "_" + std::to_string(std::rand()) + ".img";
}

} // namespace

TEST(BackupTest, FullBackupRestoreRoundTrip) {
    std::string src = tmpPath("src");
    std::string img = tmpPath("img");
    std::string dst = tmpPath("dst");
    ASSERT_TRUE(makeSourceImage(src, 3, 12345));  // 3 MiB
    // dst must exist with different content so we can prove restore overwrites.
    ASSERT_TRUE(makeSourceImage(dst, 3, 99999));

    auto disk = DiskIO::openReadWrite(src);
    ASSERT_TRUE(disk && disk->isOpen());
    BackupOptions opts; opts.block_size = 512 * 1024;  // 512 KiB blocks for test
    Result r = backupCreateFull(disk, img, opts);
    ASSERT_TRUE(r.success()) << r.message;

    BackupInfo info;
    r = backupInfo(img, info);
    ASSERT_TRUE(r.success()) << r.message;
    EXPECT_EQ(info.mode, BackupMode::Full);
    EXPECT_EQ(info.source_size, 3ULL * 1024 * 1024);
    EXPECT_EQ(info.block_size, 512u * 1024);
    EXPECT_EQ(info.num_blocks, 6ULL);        // 3 MiB / 512 KiB
    EXPECT_EQ(info.present_blocks, 6ULL);

    r = backupVerify(img);
    ASSERT_TRUE(r.success()) << r.message;

    auto target = DiskIO::openReadWrite(dst);
    ASSERT_TRUE(target && target->isOpen());
    r = backupRestore(img, target, opts);
    ASSERT_TRUE(r.success()) << r.message;

    EXPECT_TRUE(filesEqual(src, dst)) << "restored bytes must match source exactly";
    std::remove(src.c_str()); std::remove(img.c_str()); std::remove(dst.c_str());
}

TEST(BackupTest, IncrementalStoresOnlyChangedBlocks) {
    std::string src = tmpPath("isrc");
    std::string full = tmpPath("ifull");
    std::string inc = tmpPath("iinc");
    ASSERT_TRUE(makeSourceImage(src, 4, 777));

    auto disk = DiskIO::openReadWrite(src);
    ASSERT_TRUE(disk && disk->isOpen());
    BackupOptions opts; opts.block_size = 1024 * 1024;  // 1 MiB -> 4 blocks
    Result r = backupCreateFull(disk, full, opts);
    ASSERT_TRUE(r.success()) << r.message;

    // Change exactly one block: byte at 2 MiB + 100 (block index 2)
    ASSERT_TRUE(pokeImage(src, 2ULL * 1024 * 1024 + 100, 0xAB));

    r = backupCreateIncremental(disk, full, inc, /*differential=*/false, opts);
    ASSERT_TRUE(r.success()) << r.message;

    BackupInfo info;
    r = backupInfo(inc, info);
    ASSERT_TRUE(r.success()) << r.message;
    EXPECT_EQ(info.mode, BackupMode::Incremental);
    EXPECT_EQ(info.num_blocks, 4ULL);
    EXPECT_EQ(info.present_blocks, 1ULL) << "only the changed block should be stored";

    r = backupVerify(inc);
    ASSERT_TRUE(r.success()) << r.message;

    // Restore: apply the incremental over the FULL image's content restored first.
    std::string dst = tmpPath("idst");
    ASSERT_TRUE(makeSourceImage(dst, 4, 424242));
    // First restore the full image to dst, then apply the incremental.
    auto target = DiskIO::openReadWrite(dst);
    ASSERT_TRUE(target && target->isOpen());
    r = backupRestore(full, target, opts);
    ASSERT_TRUE(r.success()) << r.message;
    r = backupRestore(inc, target, opts);
    ASSERT_TRUE(r.success()) << r.message;
    EXPECT_TRUE(filesEqual(src, dst)) << "full + incremental must reproduce current source";

    std::remove(src.c_str()); std::remove(full.c_str());
    std::remove(inc.c_str()); std::remove(dst.c_str());
}

TEST(BackupTest, DifferentialBaseMustBeFull) {
    std::string src = tmpPath("dsrc");
    std::string full = tmpPath("dfull");
    std::string inc = tmpPath("dinc");
    std::string dif = tmpPath("ddif");
    ASSERT_TRUE(makeSourceImage(src, 2, 5));

    auto disk = DiskIO::openReadWrite(src);
    ASSERT_TRUE(disk && disk->isOpen());
    BackupOptions opts; opts.block_size = 1024 * 1024;

    Result r = backupCreateFull(disk, full, opts);
    ASSERT_TRUE(r.success()) << r.message;
    ASSERT_TRUE(pokeImage(src, 0, 0x11));
    r = backupCreateIncremental(disk, full, inc, /*differential=*/false, opts);
    ASSERT_TRUE(r.success()) << r.message;

    // Differential against the incremental base must be rejected.
    r = backupCreateIncremental(disk, inc, dif, /*differential=*/true, opts);
    ASSERT_TRUE(r.failed()) << "differential requires a full base";
    EXPECT_NE(r.message.find("full"), std::string::npos);

    std::remove(src.c_str()); std::remove(full.c_str());
    std::remove(inc.c_str()); std::remove(dif.c_str());
}

TEST(BackupTest, VerifyDetectsCorruption) {
    std::string src = tmpPath("vsrc");
    std::string img = tmpPath("vimg");
    ASSERT_TRUE(makeSourceImage(src, 2, 42));

    auto disk = DiskIO::openReadWrite(src);
    ASSERT_TRUE(disk && disk->isOpen());
    BackupOptions opts; opts.block_size = 1024 * 1024;
    Result r = backupCreateFull(disk, img, opts);
    ASSERT_TRUE(r.success()) << r.message;
    r = backupVerify(img);
    ASSERT_TRUE(r.success()) << r.message;

    // Corrupt a byte in the image payload (past the header region).
    ASSERT_TRUE(pokeImage(img, 512 + 1 + 64, 0x00));  // header(512) + bitmap(1) + first table byte...
    r = backupVerify(img);
    EXPECT_TRUE(r.failed()) << "verify must detect corrupted stored data";

    std::remove(src.c_str()); std::remove(img.c_str());
}

TEST(BackupTest, RejectsNonOpmImage) {
    std::string junk = tmpPath("junk");
    std::FILE* f = std::fopen(junk.c_str(), "wb");
    ASSERT_TRUE(f);
    const char* data = "this is definitely not an opm image file, just some junk bytes";
    std::fwrite(data, 1, std::strlen(data), f);
    std::fclose(f);

    BackupInfo info;
    Result r = backupInfo(junk, info);
    EXPECT_TRUE(r.failed());
    std::remove(junk.c_str());
}
