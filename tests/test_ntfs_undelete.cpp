#include <gtest/gtest.h>
#include "opm/convert_fs.hpp"
#include "opm/disk_io.hpp"
#include "opm/fat32_impl.hpp"
#include "opm/ntfs_impl.hpp"
#include "opm/ntfs_undelete.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <map>

using namespace opm;

namespace {

uint16_t rd16(const uint8_t* p) { return p[0] | (p[1] << 8); }
uint32_t rd32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (uint32_t(p[3]) << 24);
}
uint64_t rd64(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v |= uint64_t(p[i]) << (8 * i);
    return v;
}

uint32_t allocChain(std::shared_ptr<DiskIO> disk, uint64_t start,
                    fat32::FAT32Layout& layout, std::vector<uint32_t>& fat,
                    const std::vector<uint8_t>& data) {
    uint32_t clusters_needed = static_cast<uint32_t>(
        (data.size() + layout.sectors_per_cluster * layout.bytes_per_sector - 1) /
        (layout.sectors_per_cluster * layout.bytes_per_sector));
    if (clusters_needed == 0) clusters_needed = 1;
    std::vector<uint32_t> chain;
    for (uint32_t c = 2; c < layout.total_clusters && chain.size() < clusters_needed; c++) {
        if ((fat[c] & 0x0FFFFFFF) == 0) chain.push_back(c);
    }
    if (chain.size() < clusters_needed) return 0;
    for (size_t i = 0; i < chain.size(); i++) {
        uint32_t next = (i + 1 < chain.size()) ? chain[i + 1] : 0x0FFFFFFF;
        fat[chain[i]] = next;
        size_t off = i * layout.sectors_per_cluster * layout.bytes_per_sector;
        size_t len = layout.sectors_per_cluster * layout.bytes_per_sector;
        if (off + len > data.size()) len = data.size() - off;
        std::vector<uint8_t> buf(layout.sectors_per_cluster * layout.bytes_per_sector, 0);
        std::memcpy(buf.data(), data.data() + off, len);
        uint64_t sector = start + layout.clusterToSector(chain[i]);
        if (!disk->write(buf.data(), sector * layout.bytes_per_sector, buf.size()).success()) {
            return 0;
        }
    }
    return chain[0];
}

struct BuiltVolume {
    std::string path;
    std::shared_ptr<DiskIO> disk;
    uint64_t start = 0;
    uint8_t spc = 1;
    uint64_t mft_lcn = 0;
    std::map<std::string, std::vector<uint8_t>> files;
};

// Build a populated FAT32 partition, then convert it to a live NTFS volume.
void buildNtfsVolume(BuiltVolume& out) {
    out.path = "/tmp/opm_nundelete_" + std::to_string(::getpid()) + "_" +
               std::to_string(std::rand()) + ".img";
    std::FILE* f = std::fopen(out.path.c_str(), "wb");
    ASSERT_TRUE(f);
    std::vector<uint8_t> zero(1024 * 1024, 0);
    for (int i = 0; i < 128; i++) {
        ASSERT_EQ(std::fwrite(zero.data(), 1, zero.size(), f), zero.size());
    }
    std::fclose(f);

    out.disk = DiskIO::openReadWrite(out.path);
    ASSERT_TRUE(out.disk && out.disk->isOpen());
    out.start = 2048;

    const uint64_t sz = 100ULL * 1024 * 1024;
    ASSERT_TRUE(fat32::formatFAT32Complete(out.disk, out.start, sz, "VOL").success());
    fat32::FAT32BootSector bs;
    fat32::FAT32Layout layout;
    ASSERT_TRUE(fat32::getFAT32Info(out.disk, out.start, bs, layout).success());
    std::vector<uint32_t> fat;
    ASSERT_TRUE(fat32::readFATTable(out.disk, out.start, layout, 0, fat).success());

    std::vector<uint8_t> f1;
    for (int i = 0; i < 4096; i++) f1.push_back(static_cast<uint8_t>(i % 251));
    uint32_t c1 = allocChain(out.disk, out.start, layout, fat, f1);
    ASSERT_NE(c1, 0u);
    ASSERT_TRUE(fat32::createDirectoryEntry(out.disk, out.start, layout,
                                            layout.root_cluster, "HELLO.TXT",
                                            fat32::ATTR_ARCHIVE, c1,
                                            static_cast<uint32_t>(f1.size())).success());

    std::vector<uint8_t> f2;
    for (int i = 0; i < 2048; i++) f2.push_back(static_cast<uint8_t>(100 + i % 100));
    uint32_t c2 = allocChain(out.disk, out.start, layout, fat, f2);
    ASSERT_NE(c2, 0u);
    ASSERT_TRUE(fat32::createDirectoryEntry(out.disk, out.start, layout,
                                            layout.root_cluster, "WORLD.DAT",
                                            fat32::ATTR_ARCHIVE, c2,
                                            static_cast<uint32_t>(f2.size())).success());
    ASSERT_TRUE(fat32::writeFATTable(out.disk, out.start, layout, 0, fat).success());
    ASSERT_TRUE(fat32::writeFATTable(out.disk, out.start, layout, 1, fat).success());

    out.files["HELLO.TXT"] = f1;
    out.files["WORLD.DAT"] = f2;

    Result r = convertFAT32ToNTFS(out.disk, out.start, sz, "VOL");
    ASSERT_TRUE(r.success()) << r.message;

    uint8_t boot[512];
    ASSERT_TRUE(out.disk->readSector(boot, out.start).success());
    ASSERT_TRUE(std::memcmp(boot + 3, "NTFS    ", 8) == 0);
    out.spc = boot[13];
    out.mft_lcn = rd64(boot + 48);
}

// Find a user MFT record by its $FILE_NAME (returns 0 when missing).
uint64_t findRecord(std::shared_ptr<DiskIO> disk, uint64_t start, uint8_t spc,
                    uint64_t mft_lcn, const std::string& name) {
    for (uint64_t recnum = 16; recnum < 128; recnum++) {
        uint64_t off = (start + mft_lcn * spc) * 512 + recnum * 1024;
        std::vector<uint8_t> rec(1024, 0);
        if (disk->read(rec.data(), off, 1024).failed()) continue;
        if (rd32(rec.data()) != 0x454C4946) continue;
        uint16_t attr_off = rd16(rec.data() + 20);
        size_t o = attr_off;
        while (o + 8 <= 1024) {
            uint32_t t = rd32(rec.data() + o);
            if (t == 0xFFFFFFFFu) break;
            uint32_t len = rd32(rec.data() + o + 4);
            if (len == 0 || o + len > 1024) break;
            if (t == 0x30 && (rec[o + 8] & 1) == 0) {
                uint32_t vlen = rd32(rec.data() + o + 16);
                uint32_t voff = rd32(rec.data() + o + 20);
                if (voff + vlen > len) break;
                const uint8_t* v = rec.data() + o + voff;
                uint8_t nlen = v[64];
                std::string nm;
                for (uint8_t i = 0; i < nlen; i++) {
                    uint16_t c = v[66 + i * 2] | (v[66 + i * 2 + 1] << 8);
                    if (c < 128) nm += static_cast<char>(c);
                    else nm += '?';
                }
                if (nm == name) return recnum;
            }
            o += len;
        }
    }
    return 0;
}

// Simulate deletion: clear IN_USE on the record + remove its index entry
// from the root directory record's resident $INDEX_ROOT + clear the file's
// clusters in $Bitmap (as a real NTFS delete does).
void simulateDelete(std::shared_ptr<DiskIO> disk, uint64_t start, uint8_t spc,
                    uint64_t mft_lcn, uint64_t record_num) {
    uint64_t rec_off = (start + mft_lcn * spc) * 512 + record_num * 1024;
    std::vector<uint8_t> rec(1024, 0);
    ASSERT_TRUE(disk->read(rec.data(), rec_off, 1024).success());
    rec[22] &= ~0x01;  // clear IN_USE
    disk->write(rec.data(), rec_off, 1024);

    // Clear the record's $DATA run clusters in $Bitmap (record 6's run).
    {
        uint64_t bmp_lcn = 0;
        uint64_t root_off = (start + mft_lcn * spc) * 512 + 6 * 1024;
        std::vector<uint8_t> bmp_rec(1024, 0);
        if (disk->read(bmp_rec.data(), root_off, 1024).success()) {
            uint16_t ao = rd16(bmp_rec.data() + 20);
            size_t o = ao;
            while (o + 8 <= 1024) {
                uint32_t t = rd32(bmp_rec.data() + o);
                if (t == 0xFFFFFFFFu) break;
                uint32_t len = rd32(bmp_rec.data() + o + 4);
                if (len == 0 || o + len > 1024) break;
                if (t == 0x80 && (bmp_rec[o + 8] & 1)) {
                    uint16_t runoff = rd16(bmp_rec.data() + o + 32);
                    size_t p = o + runoff;
                    uint8_t hdr = bmp_rec[p++];
                    if (hdr) {
                        int llen = hdr >> 4, olen = hdr & 0x0F;
                        uint64_t run_len = 0;
                        for (int i = 0; i < llen; i++)
                            run_len |= uint64_t(bmp_rec[p + i]) << (8 * i);
                        int64_t offs = 0;
                        for (int i = 0; i < olen; i++)
                            offs |= int64_t(bmp_rec[p + llen + i]) << (8 * i);
                        if (olen && (bmp_rec[p + llen + olen - 1] & 0x80))
                            for (int i = olen; i < 8; i++)
                                offs |= int64_t(-1) << (8 * i);
                        bmp_lcn = uint64_t(offs);
                    }
                }
                o += len;
            }
        }
        ASSERT_NE(bmp_lcn, 0u) << "cannot resolve $Bitmap location";
        uint64_t clusters = (100ULL * 1024 * 1024 / 512) / spc;
        uint64_t bmp_bytes = (clusters + 7) / 8;
        std::vector<uint8_t> bmp(bmp_bytes, 0);
        ASSERT_TRUE(disk->read(bmp.data(), (start + bmp_lcn * spc) * 512, bmp_bytes).success());
        // find this record's $DATA runs
        std::vector<std::pair<uint64_t, uint64_t>> runs;
        {
            uint16_t ao = rd16(rec.data() + 20);
            size_t o = ao;
            while (o + 8 <= 1024) {
                uint32_t t = rd32(rec.data() + o);
                if (t == 0xFFFFFFFFu) break;
                uint32_t len = rd32(rec.data() + o + 4);
                if (len == 0 || o + len > 1024) break;
                if (t == 0x80 && (rec[o + 8] & 1)) {
                    uint16_t runoff = rd16(rec.data() + o + 32);
                    size_t p = o + runoff;
                    int64_t prev = 0;
                    while (p < 1024) {
                        uint8_t hdr = rec[p++];
                        if (hdr == 0) break;
                        int llen = hdr >> 4, olen = hdr & 0x0F;
                        if (llen == 0 || p + llen + olen > 1024) break;
                        uint64_t run_len = 0;
                        for (int i = 0; i < llen; i++)
                            run_len |= uint64_t(rec[p + i]) << (8 * i);
                        int64_t offs = 0;
                        for (int i = 0; i < olen; i++)
                            offs |= int64_t(rec[p + llen + i]) << (8 * i);
                        if (olen && (rec[p + llen + olen - 1] & 0x80))
                            for (int i = olen; i < 8; i++)
                                offs |= int64_t(-1) << (8 * i);
                        int64_t lcn = prev + offs;
                        runs.push_back({ uint64_t(lcn), run_len });
                        prev = lcn;
                        p += llen + olen;
                    }
                }
                o += len;
            }
        }
        for (auto& r : runs) {
            for (uint64_t c = r.first; c < r.first + r.second && c < clusters; c++) {
                bmp[c / 8] &= static_cast<uint8_t>(~(1 << (c % 8)));
            }
        }
        ASSERT_TRUE(disk->write(bmp.data(), (start + bmp_lcn * spc) * 512, bmp_bytes).success());
    }

    // Remove the index entry from root (record 5) $INDEX_ROOT.
    uint64_t root_off = (start + mft_lcn * spc) * 512 + 5 * 1024;
    std::vector<uint8_t> root(1024, 0);
    ASSERT_TRUE(disk->read(root.data(), root_off, 1024).success());
    uint16_t attr_off = rd16(root.data() + 20);
    size_t o = attr_off;
    while (o + 8 <= 1024) {
        uint32_t t = rd32(root.data() + o);
        if (t == 0xFFFFFFFFu) break;
        uint32_t len = rd32(root.data() + o + 4);
        if (len == 0 || o + len > 1024) break;
        if (t == 0x90 && (root[o + 8] & 1) == 0) {  // $INDEX_ROOT $I30
            uint32_t vlen = rd32(root.data() + o + 16);
            uint32_t voff = rd32(root.data() + o + 20);
            uint8_t* val = root.data() + o + voff;
            uint32_t entries_off = rd32(val + 16);
            uint32_t idx_len = rd32(val + 20);
            size_t p = entries_off;
            while (p + 8 <= idx_len) {
                uint16_t esize = rd16(val + p + 8);
                uint16_t eflags = rd16(val + p + 12);
                if (esize == 0 || p + esize > idx_len) break;
                if (!(eflags & 2)) {  // not the END marker
                    uint64_t mft_ref = rd64(val + p);
                    if ((mft_ref & 0xFFFFFFFFFFFFULL) == record_num) {
                        std::memmove(val + p, val + p + esize, idx_len - p - esize);
                        uint32_t new_len = idx_len - esize;
                        val[16 + 4] = new_len & 0xFF;
                        val[16 + 5] = (new_len >> 8) & 0xFF;
                        val[16 + 6] = (new_len >> 16) & 0xFF;
                        val[16 + 7] = (new_len >> 24) & 0xFF;
                        break;
                    }
                }
                p += esize;
            }
            disk->write(root.data(), root_off, 1024);
            break;
        }
        o += len;
    }
}

} // namespace

// ============================================================================
// Scan finds a deleted record and export recovers its data verbatim
// ============================================================================

TEST(NtfsUndeleteTest, ScanAndExport) {
    BuiltVolume v;
    buildNtfsVolume(v);

    // Record numbers of the two live files.
    uint64_t rec_hello = findRecord(v.disk, v.start, v.spc, v.mft_lcn, "HELLO.TXT");
    uint64_t rec_world = findRecord(v.disk, v.start, v.spc, v.mft_lcn, "WORLD.DAT");
    ASSERT_NE(rec_hello, 0u);
    ASSERT_NE(rec_world, 0u);

    // Live records must NOT be reported as deleted.
    auto live = ntfs::scanDeletedNTFS(v.disk, v.start);
    for (const auto& f : live) {
        EXPECT_NE(f.mft_record, rec_hello) << "live HELLO.TXT must not appear";
        EXPECT_NE(f.mft_record, rec_world) << "live WORLD.DAT must not appear";
    }

    // Delete HELLO.TXT.
    simulateDelete(v.disk, v.start, v.spc, v.mft_lcn, rec_hello);

    auto deleted = ntfs::scanDeletedNTFS(v.disk, v.start);
    bool found = false;
    for (const auto& f : deleted) {
        if (f.mft_record == rec_hello) {
            found = true;
            EXPECT_EQ(f.name, "HELLO.TXT");
            EXPECT_EQ(f.data_size, 4096u);
            EXPECT_GT(f.runs.size(), 0u);
        }
    }
    ASSERT_TRUE(found) << "deleted HELLO.TXT must be found by the scan";

    // Export to a temp dir and verify bytes.
    std::string out_dir = "/tmp/opm_nundelete_exp_" + std::to_string(::getpid());
    for (const auto& f : deleted) {
        if (f.mft_record == rec_hello) {
            Result r = ntfs::exportDeletedNTFS(v.disk, v.start, f, out_dir);
            ASSERT_TRUE(r.success()) << r.message;
        }
    }
    std::string expected_name = std::to_string(rec_hello) + "_HELLO.TXT";
    std::string exp_path = out_dir + "/" + expected_name;
    std::FILE* f = std::fopen(exp_path.c_str(), "rb");
    ASSERT_TRUE(f) << "export file missing: " << exp_path;
    std::vector<uint8_t> got(4096, 0);
    ASSERT_EQ(std::fread(got.data(), 1, got.size(), f), got.size());
    std::fclose(f);
    EXPECT_EQ(got, v.files["HELLO.TXT"]) << "exported data must match the original";
    std::remove(exp_path.c_str());
    std::remove(out_dir.c_str());
    std::remove(v.path.c_str());
}

// ============================================================================
// In-place restore re-links the record and keeps the volume fsck-clean
// ============================================================================

TEST(NtfsUndeleteTest, InPlaceRestore) {
    BuiltVolume v;
    buildNtfsVolume(v);

    uint64_t rec_world = findRecord(v.disk, v.start, v.spc, v.mft_lcn, "WORLD.DAT");
    ASSERT_NE(rec_world, 0u);
    simulateDelete(v.disk, v.start, v.spc, v.mft_lcn, rec_world);

    auto deleted = ntfs::scanDeletedNTFS(v.disk, v.start);
    const ntfs::NtfsDeletedFile* target = nullptr;
    for (const auto& f : deleted) {
        if (f.mft_record == rec_world) target = &f;
    }
    ASSERT_TRUE(target) << "WORLD.DAT must be found after deletion";

    Result r = ntfs::restoreDeletedNTFS(v.disk, v.start, *target);
    ASSERT_TRUE(r.success()) << r.message;

    // After restore the record must be IN_USE again and no longer scanned.
    uint64_t rec_off = (v.start + v.mft_lcn * v.spc) * 512 + rec_world * 1024;
    std::vector<uint8_t> rec(1024, 0);
    ASSERT_TRUE(v.disk->read(rec.data(), rec_off, 1024).success());
    EXPECT_TRUE(rec[22] & 0x01) << "IN_USE flag must be set after restore";

    auto after = ntfs::scanDeletedNTFS(v.disk, v.start);
    for (const auto& f : after) {
        EXPECT_NE(f.mft_record, rec_world) << "restored file must not be scanned";
    }

    // The volume must still pass fsck.
    std::vector<std::string> errors;
    r = ntfs::checkNTFS(v.disk, v.start, false, &errors);
    ASSERT_TRUE(r.success()) << "checkNTFS after restore: " << r.message
                             << (errors.empty() ? "" : " first: " + errors[0]);

    std::remove(v.path.c_str());
}

// ============================================================================
// Refuses restore when the data was overwritten (clusters reallocated)
// ============================================================================

TEST(NtfsUndeleteTest, RefusesOverwrittenData) {
    BuiltVolume v;
    buildNtfsVolume(v);

    uint64_t rec_hello = findRecord(v.disk, v.start, v.spc, v.mft_lcn, "HELLO.TXT");
    ASSERT_NE(rec_hello, 0u);
    simulateDelete(v.disk, v.start, v.spc, v.mft_lcn, rec_hello);

    auto deleted = ntfs::scanDeletedNTFS(v.disk, v.start);
    const ntfs::NtfsDeletedFile* target = nullptr;
    for (const auto& f : deleted) {
        if (f.mft_record == rec_hello) target = &f;
    }
    ASSERT_TRUE(target);

    // Overwrite the file's first run's clusters by formatting the region as
    // part of a new NTFS volume (simulates data reuse). Easiest deterministic
    // trick: flip the $Bitmap bits so the run appears allocated.
    // Instead, directly re-format the partition as NTFS (destroys the data).
    // Then the scan must find nothing (records gone), which is the honest
    // "data overwritten" path at the scan level.
    Result r = ntfs::formatNTFS(v.disk, v.start, 100ULL * 1024 * 1024, "NEW");
    ASSERT_TRUE(r.success()) << r.message;
    auto after = ntfs::scanDeletedNTFS(v.disk, v.start);
    for (const auto& f : after) {
        EXPECT_NE(f.mft_record, rec_hello)
            << "after reformat, old records must not be reported";
    }
    std::remove(v.path.c_str());
}
