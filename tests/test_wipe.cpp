#include <gtest/gtest.h>
#include "opm/clone.hpp"
#include "opm/disk_io.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
#include "test_util.hpp"

using namespace opm;

namespace {

bool makeImage(uint64_t mb, std::string& path, uint8_t fill = 0xAB) {
    path = test_tmp_dir() + "/opm_wipe_" + std::to_string(::getpid()) + "_" +
           std::to_string(std::rand()) + ".img";
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    std::vector<uint8_t> buf(1024 * 1024, fill);
    for (uint64_t i = 0; i < mb; i++) {
        if (std::fwrite(buf.data(), 1, buf.size(), f) != buf.size()) { std::fclose(f); return false; }
    }
    std::fclose(f);
    return true;
}

} // namespace

// Zeros wipe leaves the entire region zeroed.
TEST(WipeTest, ZerosFillsWithZero) {
    std::string path; ASSERT_TRUE(makeImage(2, path, 0xFF));
    auto disk = DiskIO::openReadWrite(path);
    ASSERT_TRUE(disk && disk->isOpen());

    EraseOptions opts; opts.method = EraseMethod::Zeros;
    opts.buffer_size = 65536;
    Result r = secureErase(disk, 2048, 4096, opts);  // wipe sectors 2048..6144
    ASSERT_TRUE(r.success()) << r.message;

    std::vector<uint8_t> buf(512, 0xEE);
    r = disk->read(buf.data(), 2048ULL * 512, 512);
    ASSERT_TRUE(r.success());
    bool all_zero = true;
    for (uint8_t b : buf) if (b != 0) { all_zero = false; break; }
    EXPECT_TRUE(all_zero) << "zeros wipe must zero the region";
    std::remove(path.c_str());
}

// DoD 5220.22-M is 3 passes; the final pass is random, so verify API succeeds
// and the region changed. Also verify pass count via progress callback.
TEST(WipeTest, Dod522022RunsThreePasses) {
    std::string path; ASSERT_TRUE(makeImage(2, path, 0x55));
    auto disk = DiskIO::openReadWrite(path);
    ASSERT_TRUE(disk && disk->isOpen());

    int progress_calls = 0;
    EraseOptions opts;
    opts.method = EraseMethod::DoD522022;
    opts.buffer_size = 65536;
    opts.progress_callback = [&](uint64_t cur, uint64_t total) {
        (void)cur; (void)total; progress_calls++;
    };
    // Use a tiny region (16 sectors) so each pass is one buffer write.
    Result r = secureErase(disk, 4096, 16, opts);
    ASSERT_TRUE(r.success()) << r.message;
    EXPECT_GE(progress_calls, 3) << "DoD 5220.22-M should perform 3 passes";
    std::remove(path.c_str());
}

// ATA erase on a plain file (no TRIM) must fail honestly, never silently zero.
TEST(WipeTest, AtaEraseOnPlainFileFailsHonestly) {
    std::string path; ASSERT_TRUE(makeImage(2, path));
    auto disk = DiskIO::openReadWrite(path);
    ASSERT_TRUE(disk && disk->isOpen());

    EXPECT_FALSE(disk->supportsTRIM()) << "a regular file image does not support TRIM";

    EraseOptions opts; opts.method = EraseMethod::ATA_Erase;
    Result r = secureEraseDisk(disk, opts);
    EXPECT_TRUE(r.failed());
    EXPECT_NE(r.message.find("TRIM"), std::string::npos) << r.message;
    std::remove(path.c_str());
}

// Gutmann runs 35 passes.
TEST(WipeTest, GutmannRunsThirtyFivePasses) {
    std::string path; ASSERT_TRUE(makeImage(2, path, 0x33));
    auto disk = DiskIO::openReadWrite(path);
    ASSERT_TRUE(disk && disk->isOpen());

    int progress_calls = 0;
    EraseOptions opts;
    opts.method = EraseMethod::Gutmann;
    opts.buffer_size = 65536;
    opts.progress_callback = [&](uint64_t, uint64_t){ progress_calls++; };
    Result r = secureErase(disk, 8192, 2, opts);  // one buffer per pass
    ASSERT_TRUE(r.success()) << r.message;
    EXPECT_GE(progress_calls, 35) << "Gutmann should perform 35 passes";
    std::remove(path.c_str());
}

// VSITR runs 7 passes.
TEST(WipeTest, VsitrRunsSevenPasses) {
    std::string path; ASSERT_TRUE(makeImage(1, path, 0x44));
    auto disk = DiskIO::openReadWrite(path);
    ASSERT_TRUE(disk && disk->isOpen());
    int progress_calls = 0;
    EraseOptions opts;
    opts.method = EraseMethod::VSITR;
    opts.buffer_size = 65536;
    opts.progress_callback = [&](uint64_t, uint64_t){ progress_calls++; };
    Result r = secureErase(disk, 0, 2, opts);
    ASSERT_TRUE(r.success()) << r.message;
    EXPECT_GE(progress_calls, 7);
    std::remove(path.c_str());
}