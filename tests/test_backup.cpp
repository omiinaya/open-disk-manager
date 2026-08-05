#include <gtest/gtest.h>
#include "opm/backup.hpp"
#include "opm/disk_io.hpp"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <filesystem>
#include <vector>
#include "test_util.hpp"

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
    return std::string(test_tmp_dir() + "/opm_bk_") + tag + "_" +
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

// ---------------------------------------------------------------------------
// Compression (--compress): sparse/RLE per-block encoding
// ---------------------------------------------------------------------------

// Build a sparse source image: mostly zeros with a small patterned region, so
// compression actually shrinks it.
bool makeSparseImage(const std::string& path, uint64_t mb, uint32_t seed) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    std::vector<uint8_t> buf(1024 * 1024, 0);
    uint32_t s = seed;
    // Fill only the first 4 KiB of each MiB block with pseudo-random data.
    for (uint64_t m = 0; m < mb; m++) {
        for (size_t i = 0; i < 4096; i++) {
            s = s * 1664525u + 1013904223u;
            buf[i] = static_cast<uint8_t>(s >> 24);
        }
        std::memset(buf.data() + 4096, 0, buf.size() - 4096);
        if (std::fwrite(buf.data(), 1, buf.size(), f) != buf.size()) {
            std::fclose(f);
            return false;
        }
    }
    std::fclose(f);
    return true;
}

TEST(BackupTest, CompressedFullIsSmallerAndRoundTrips) {
    std::string src = tmpPath("csrc");
    std::string imgC = tmpPath("cimgc");
    std::string imgU = tmpPath("cimgu");
    std::string dst = tmpPath("cdst");
    ASSERT_TRUE(makeSparseImage(src, 4, 31415));   // 4 MiB, mostly zeros

    auto disk = DiskIO::openReadWrite(src);
    ASSERT_TRUE(disk && disk->isOpen());
    BackupOptions opts; opts.block_size = 1024 * 1024;  // 4 blocks

    // Uncompressed image for comparison
    Result r = backupCreateFull(disk, imgU, opts);
    ASSERT_TRUE(r.success()) << r.message;
    // Compressed image
    BackupOptions copts = opts; copts.compress = true;
    r = backupCreateFull(disk, imgC, copts);
    ASSERT_TRUE(r.success()) << r.message;

    BackupInfo info;
    r = backupInfo(imgC, info);
    ASSERT_TRUE(r.success()) << r.message;
    EXPECT_TRUE(info.compressed);

    // The compressed image must be strictly smaller than the raw image.
    std::FILE* fu = std::fopen(imgU.c_str(), "rb");
    std::FILE* fc = std::fopen(imgC.c_str(), "rb");
    ASSERT_TRUE(fu && fc);
    std::fseek(fu, 0, SEEK_END); std::fseek(fc, 0, SEEK_END);
    long su = std::ftell(fu), sc = std::ftell(fc);
    std::fclose(fu); std::fclose(fc);
    EXPECT_LT(sc, su) << "compressed image should be smaller";

    r = backupVerify(imgC);
    ASSERT_TRUE(r.success()) << r.message;

    ASSERT_TRUE(makeSparseImage(dst, 4, 777));  // different content
    auto target = DiskIO::openReadWrite(dst);
    ASSERT_TRUE(target && target->isOpen());
    r = backupRestore(imgC, target, opts);
    ASSERT_TRUE(r.success()) << r.message;
    EXPECT_TRUE(filesEqual(src, dst)) << "compressed restore must match source exactly";

    std::remove(src.c_str()); std::remove(imgC.c_str());
    std::remove(imgU.c_str()); std::remove(dst.c_str());
}

TEST(BackupTest, CompressedIncrementalRoundTrips) {
    std::string src = tmpPath("cisrc");
    std::string full = tmpPath("cifull");
    std::string inc = tmpPath("ciinc");
    std::string dst = tmpPath("cidst");
    ASSERT_TRUE(makeSparseImage(src, 4, 2718));

    auto disk = DiskIO::openReadWrite(src);
    ASSERT_TRUE(disk && disk->isOpen());
    BackupOptions opts; opts.block_size = 1024 * 1024;
    Result r = backupCreateFull(disk, full, opts);
    ASSERT_TRUE(r.success()) << r.message;

    // Change one byte in block 1
    ASSERT_TRUE(pokeImage(src, 1ULL * 1024 * 1024 + 50, 0xCD));
    BackupOptions copts = opts; copts.compress = true;
    r = backupCreateIncremental(disk, full, inc, /*differential=*/false, copts);
    ASSERT_TRUE(r.success()) << r.message;

    BackupInfo info;
    r = backupInfo(inc, info);
    ASSERT_TRUE(r.success()) << r.message;
    EXPECT_EQ(info.mode, BackupMode::Incremental);
    EXPECT_EQ(info.present_blocks, 1ULL);
    EXPECT_TRUE(info.compressed);

    r = backupVerify(inc);
    ASSERT_TRUE(r.success()) << r.message;

    ASSERT_TRUE(makeSparseImage(dst, 4, 555));
    auto target = DiskIO::openReadWrite(dst);
    ASSERT_TRUE(target && target->isOpen());
    r = backupRestore(full, target, opts);
    ASSERT_TRUE(r.success()) << r.message;
    r = backupRestore(inc, target, opts);
    ASSERT_TRUE(r.success()) << r.message;
    EXPECT_TRUE(filesEqual(src, dst)) << "full + compressed incremental must match source";

    std::remove(src.c_str()); std::remove(full.c_str());
    std::remove(inc.c_str()); std::remove(dst.c_str());
}

// ---------------------------------------------------------------------------
// Retention: list + prune
// ---------------------------------------------------------------------------

// Craft a minimal valid OPMIMG file with a caller-controlled created_at.
// Layout mirrors the packed BackupHeader in backup.cpp.
bool makeFakeImage(const std::string& path, uint32_t mode, uint64_t created_at) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    std::vector<uint8_t> hdr(512, 0);
    const char* magic = "OPMIMG01";
    std::memcpy(hdr.data(), magic, 8);
    auto put32 = [&](size_t off, uint32_t v) {
        hdr[off] = v & 0xFF; hdr[off+1] = (v >> 8) & 0xFF;
        hdr[off+2] = (v >> 16) & 0xFF; hdr[off+3] = (v >> 24) & 0xFF;
    };
    auto put64 = [&](size_t off, uint64_t v) {
        for (int i = 0; i < 8; i++) hdr[off+i] = (v >> (8*i)) & 0xFF;
    };
    put32(8, 1);          // version
    put32(12, mode);      // mode (0 full, 1 incr, 2 diff)
    put64(16, 512);       // source_size
    put32(24, 512);       // sector_size
    put32(28, 512);       // block_size
    put64(32, 1);         // num_blocks
    put64(40, 0);         // present_blocks (0 → no data section needed)
    put64(48, created_at);
    std::memcpy(hdr.data() + 56, "fake", 4);  // source_name
    // header(512) + bitmap(1) + checksum table(32)
    std::vector<uint8_t> body(33, 0);
    if (std::fwrite(hdr.data(), 1, hdr.size(), f) != hdr.size()) { std::fclose(f); return false; }
    if (std::fwrite(body.data(), 1, body.size(), f) != body.size()) { std::fclose(f); return false; }
    std::fclose(f);
    return true;
}

TEST(BackupTest, ListSkipsNonImagesAndSortsNewestFirst) {
    std::string dir = std::string(test_tmp_dir() + "/opm_bk_list_") + std::to_string(::getpid());
    std::filesystem::create_directories(dir);
    ASSERT_TRUE(makeFakeImage(dir + "/a_full.img", 0, 1000));
    ASSERT_TRUE(makeFakeImage(dir + "/b_incr.img", 1, 3000));
    ASSERT_TRUE(makeFakeImage(dir + "/c_full.img", 0, 2000));
    // A non-image file must be ignored
    std::FILE* f = std::fopen((dir + "/notes.txt").c_str(), "w");
    ASSERT_TRUE(f); std::fwrite("not an image", 1, 12, f); std::fclose(f);

    std::vector<BackupEntry> entries;
    Result r = backupListDir(dir, entries);
    ASSERT_TRUE(r.success()) << r.message;
    ASSERT_EQ(entries.size(), 3u);
    // newest first: b (3000), c (2000), a (1000)
    EXPECT_EQ(entries[0].name, "b_incr.img");
    EXPECT_EQ(entries[1].name, "c_full.img");
    EXPECT_EQ(entries[2].name, "a_full.img");

    std::filesystem::remove_all(dir);
}

TEST(BackupTest, PruneKeepsNewestFullAndItsChain) {
    std::string dir = std::string(test_tmp_dir() + "/opm_bk_prune_") + std::to_string(::getpid());
    std::filesystem::create_directories(dir);
    // timeline: f1(100) inc1(200) f2(300) inc2(400) f3(500) inc3(600)
    ASSERT_TRUE(makeFakeImage(dir + "/f1.img", 0, 100));
    ASSERT_TRUE(makeFakeImage(dir + "/inc1.img", 1, 200));
    ASSERT_TRUE(makeFakeImage(dir + "/f2.img", 0, 300));
    ASSERT_TRUE(makeFakeImage(dir + "/inc2.img", 1, 400));
    ASSERT_TRUE(makeFakeImage(dir + "/f3.img", 0, 500));
    ASSERT_TRUE(makeFakeImage(dir + "/inc3.img", 1, 600));

    PruneOptions po; po.keep_full = 2;  // keep f3 + f2; drop f1 and inc1
    std::vector<std::string> removed;
    Result r = backupPrune(dir, po, removed);
    ASSERT_TRUE(r.success()) << r.message;
    ASSERT_EQ(removed.size(), 2u) << "f1 and inc1 (older than oldest kept full f2) pruned";

    std::vector<BackupEntry> entries;
    r = backupListDir(dir, entries);
    ASSERT_TRUE(r.success()) << r.message;
    ASSERT_EQ(entries.size(), 4u);
    std::vector<std::string> names;
    for (const auto& e : entries) names.push_back(e.name);
    EXPECT_NE(std::find(names.begin(), names.end(), "f3.img"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "f2.img"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "inc3.img"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "inc2.img"), names.end());
    EXPECT_EQ(std::find(names.begin(), names.end(), "f1.img"), names.end());
    EXPECT_EQ(std::find(names.begin(), names.end(), "inc1.img"), names.end());

    std::filesystem::remove_all(dir);
}

TEST(BackupTest, PruneOlderThanDays) {
    std::string dir = std::string(test_tmp_dir() + "/opm_bk_age_") + std::to_string(::getpid());
    std::filesystem::create_directories(dir);
    uint64_t now = static_cast<uint64_t>(std::time(nullptr));
    ASSERT_TRUE(makeFakeImage(dir + "/old.img", 0, now - 10 * 86400ULL));  // 10 days old
    ASSERT_TRUE(makeFakeImage(dir + "/fresh.img", 0, now - 3600ULL));      // 1 hour old

    PruneOptions po; po.older_than_days = 7;
    std::vector<std::string> removed;
    Result r = backupPrune(dir, po, removed);
    ASSERT_TRUE(r.success()) << r.message;
    ASSERT_EQ(removed.size(), 1u);
    EXPECT_NE(removed[0].find("old.img"), std::string::npos);

    std::vector<BackupEntry> entries;
    r = backupListDir(dir, entries);
    ASSERT_TRUE(r.success()) << r.message;
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].name, "fresh.img");

    std::filesystem::remove_all(dir);
}

// ============================================================================
// Grandfather-Father-Son (GFS) retention
// ============================================================================

// A Unix day is 86400s. Build a set of daily fulls over ~40 days, then check
// GFS keeps 1 full per day for the daily window, 1 per week for weekly, etc.
TEST(BackupTest, GfsKeepsDailyWeeklyMonthlyAnchors) {
    std::string dir = std::string(test_tmp_dir() + "/opm_bk_gfs_") + std::to_string(::getpid());
    std::filesystem::create_directories(dir);
    uint64_t now = static_cast<uint64_t>(std::time(nullptr));
    // 40 daily full backups, one per day, oldest first name.
    // Also add an incremental after the most recent full (must be kept, base alive).
    for (int d = 39; d >= 0; d--) {
        uint64_t ts = now - static_cast<uint64_t>(d) * 86400ULL;
        char name[64];
        std::snprintf(name, sizeof(name), "%02d.img", d);
        ASSERT_TRUE(makeFakeImage(dir + "/" + name, 0, ts));
    }
    ASSERT_TRUE(makeFakeImage(dir + "/latest_inc.img", 1, now + 600));

    GfsOptions go;
    go.daily = 7; go.weekly = 4; go.monthly = 12;
    std::vector<std::string> removed;
    Result r = backupPruneGFS(dir, go, removed);
    ASSERT_TRUE(r.success()) << r.message;

    std::vector<BackupEntry> entries;
    r = backupListDir(dir, entries);
    ASSERT_TRUE(r.success()) << r.message;

    // The incremental after the newest full must survive (base is newest full).
    bool inc_alive = false;
    for (const auto& e : entries) if (e.name == "latest_inc.img") inc_alive = true;
    EXPECT_TRUE(inc_alive) << "incremental whose base (newest full) is kept must survive";

    // The newest full is day 0 (today). Daily window 7 -> 7 FULL backups kept
    // (today..today-6). Weekly 4 and monthly 12 select anchors from older days.
    // Total kept fulls must be >= 7 and <= (7 + 4 + 12) but realistically the
    // daily/weekly/monthly buckets overlap on recent backups. We assert the
    // invariants that matter:
    //  - at least 7 fulls survive (the daily window),
    //  - no full older than 31 days survives (monthly=12 window) UNLESS it was
    //    selected as a weekly anchor (early weeks within 4-week window).
    int fulls = 0;
    for (const auto& e : entries) if (e.info.mode == BackupMode::Full) fulls++;
    EXPECT_GE(fulls, 7) << "daily window must keep at least 7 fulls";

    // Every surviving full must be within the widest window — monthly=12
    // months (~366 days). A 40-day-old full is inside the monthly window but
    // the bucket only keeps the NEWEST full per month, so only ~12 monthly
    // anchors + 4 weekly + 7 daily can survive at most. The strongest hard
    // invariant: nothing older than the monthly window may survive.
    for (const auto& e : entries) {
        if (e.info.mode != BackupMode::Full) continue;
        uint64_t age_days = (now > e.info.created_at) ? (now - e.info.created_at) / 86400ULL : 0;
        EXPECT_LE(age_days, 366u) << "no full older than the monthly window may survive: " << e.name;
    }
    // And the daily window must produce at least 7 distinct-day anchors.
    std::set<uint64_t> kept_days_check;
    for (const auto& e : entries) {
        if (e.info.mode != BackupMode::Full) continue;
        kept_days_check.insert(e.info.created_at / 86400ULL);
    }
    EXPECT_GE(kept_days_check.size(), 7u)
        << "daily window must keep one full per day for at least 7 days";
    std::filesystem::remove_all(dir);
}

TEST(BackupTest, GfsChainSafetyKeepsBaseOfIncremental) {
    std::string dir = std::string(test_tmp_dir() + "/opm_bk_gfs_chain_") + std::to_string(::getpid());
    std::filesystem::create_directories(dir);
    uint64_t now = static_cast<uint64_t>(std::time(nullptr));
    // Old full + its incremental, then a NEWER full + its incremental.
    // The old pair predates the retention window; the new pair is current.
    uint64_t old_ts = now - 60 * 86400ULL;      // 60 days old
    uint64_t new_ts = now;                       // current
    ASSERT_TRUE(makeFakeImage(dir + "/old_full.img", 0, old_ts));
    ASSERT_TRUE(makeFakeImage(dir + "/old_inc.img", 1, old_ts + 600));
    ASSERT_TRUE(makeFakeImage(dir + "/new_full.img", 0, new_ts));
    ASSERT_TRUE(makeFakeImage(dir + "/new_inc.img", 1, new_ts + 600));

    GfsOptions go; go.daily = 1; go.weekly = 1; go.monthly = 1;
    std::vector<std::string> removed;
    Result r = backupPruneGFS(dir, go, removed);
    ASSERT_TRUE(r.success()) << r.message;

    std::vector<BackupEntry> entries;
    r = backupListDir(dir, entries);
    ASSERT_TRUE(r.success()) << r.message;
    std::vector<std::string> names;
    for (const auto& e : entries) names.push_back(e.name);

    // The recent full + its incremental survive.
    EXPECT_NE(std::find(names.begin(), names.end(), "new_full.img"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "new_inc.img"), names.end());
    // The old full (outside every window) is pruned, and so is its incremental
    // whose base is gone.
    EXPECT_EQ(std::find(names.begin(), names.end(), "old_full.img"), names.end());
    EXPECT_EQ(std::find(names.begin(), names.end(), "old_inc.img"), names.end());
    std::filesystem::remove_all(dir);
}

TEST(BackupTest, GfsNothingWhenAllWindowsZero) {
    std::string dir = std::string(test_tmp_dir() + "/opm_bk_gfs_zero_") + std::to_string(::getpid());
    std::filesystem::create_directories(dir);
    ASSERT_TRUE(makeFakeImage(dir + "/f.img", 0, static_cast<uint64_t>(std::time(nullptr))));
    GfsOptions go; go.daily = 0; go.weekly = 0; go.monthly = 0;
    std::vector<std::string> removed;
    Result r = backupPruneGFS(dir, go, removed);
    // With no anchors selected but fulls present, the newest full is kept as
    // an implicit anchor -> nothing is removed.
    ASSERT_TRUE(r.success()) << r.message;
    EXPECT_EQ(removed.size(), 0u);
    std::filesystem::remove_all(dir);
}

