#include <gtest/gtest.h>
#include "opm/convert_fs.hpp"
#include "opm/disk_io.hpp"
#include "opm/fat32_impl.hpp"
#include "opm/ext4_impl.hpp"
#include "opm/ntfs_impl.hpp"
#include "opm/partition_table.hpp"
#include "opm/filesystem.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <map>

using namespace opm;

namespace {

bool makeImage(uint64_t mb, std::string& path) {
    path = "/tmp/opm_conv_" + std::to_string(::getpid()) + "_" +
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

uint32_t rd32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (uint32_t(p[3]) << 24);
}
uint16_t rd16(const uint8_t* p) { return p[0] | (p[1] << 8); }
uint64_t rd64(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v |= uint64_t(p[i]) << (8 * i);
    return v;
}

// Read a FAT32 layout from the boot sector (delegates to the real parser).
bool readFatLayout(std::shared_ptr<DiskIO> disk, uint64_t start,
                   fat32::FAT32Layout& layout) {
    fat32::FAT32BootSector bs;
    return fat32::getFAT32Info(disk, start, bs, layout).success();
}

// Allocate a cluster chain in the FAT and write data into it. Returns the
// first cluster. Mirrors the fat32 helper set.
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
        // write the data chunk for this cluster
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

// Build a populated FAT32 partition at start_sector: root files + one dir.
void buildFat32Image(std::shared_ptr<DiskIO> disk, uint64_t start, uint64_t mb,
                     std::map<std::string, std::vector<uint8_t>>& files_out) {
    const uint64_t sz = mb * 1024 * 1024;
    ASSERT_TRUE(fat32::formatFAT32Complete(disk, start, sz, "MYLABEL").success());

    fat32::FAT32Layout layout;
    ASSERT_TRUE(readFatLayout(disk, start, layout));
    std::vector<uint32_t> fat;
    ASSERT_TRUE(fat32::readFATTable(disk, start, layout, 0, fat).success());

    // File 1: "README.TXT" — short name, ~4KB content
    std::vector<uint8_t> f1;
    for (int i = 0; i < 4096; i++) f1.push_back(static_cast<uint8_t>(i % 251));
    uint32_t c1 = allocChain(disk, start, layout, fat, f1);
    ASSERT_NE(c1, 0u);
    ASSERT_TRUE(fat32::createDirectoryEntry(disk, start, layout, layout.root_cluster,
                                            "README.TXT", fat32::ATTR_ARCHIVE,
                                            c1, static_cast<uint32_t>(f1.size())).success());

    // File 2: "REPORT   " (short 8.3), smaller
    std::vector<uint8_t> f2;
    for (int i = 0; i < 512; i++) f2.push_back(static_cast<uint8_t>(200 + i % 50));
    uint32_t c2 = allocChain(disk, start, layout, fat, f2);
    ASSERT_NE(c2, 0u);
    ASSERT_TRUE(fat32::createDirectoryEntry(disk, start, layout, layout.root_cluster,
                                            "REPORT", fat32::ATTR_ARCHIVE,
                                            c2, static_cast<uint32_t>(f2.size())).success());

    // Persist FAT
    ASSERT_TRUE(fat32::writeFATTable(disk, start, layout, 0, fat).success());
    ASSERT_TRUE(fat32::writeFATTable(disk, start, layout, 1, fat).success());

    files_out["README.TXT"] = f1;
    files_out["REPORT"] = f2;
}

// Parse the $FILE_NAME attribute from an MFT record; returns name + parent.
struct ParsedFileName {
    std::string name;
    uint64_t parent_ref = 0;
    uint64_t data_size = 0;
    uint32_t attrs = 0;
    bool is_dir = false;
};

bool parseFileNameAttr(const std::vector<uint8_t>& rec, ParsedFileName& out) {
    if (rec.size() < 56) return false;
    if (rd32(rec.data() + 0) != 0x454C4946) return false;  // FILE
    uint16_t attr_off = rd16(rec.data() + 20);
    size_t off = attr_off;
    while (off + 8 <= rec.size()) {
        uint32_t type = rd32(rec.data() + off);
        if (type == 0xFFFFFFFFu) break;
        uint32_t len = rd32(rec.data() + off + 4);
        if (len == 0 || off + len > rec.size()) break;
        if (type == 0x30) {  // $FILE_NAME
            uint32_t vlen = rd32(rec.data() + off + 16);
            uint32_t voff = rd32(rec.data() + off + 20);
            if (voff + vlen > len) break;
            const uint8_t* v = rec.data() + off + voff;
            out.parent_ref = rd64(v + 0);
            out.data_size = rd64(v + 48);
            out.attrs = rd32(v + 56);
            out.is_dir = (out.attrs & 0x10) != 0;
            uint8_t nlen = v[64];
            std::string nm;
            for (uint8_t i = 0; i < nlen; i++) {
                uint16_t c = v[66 + i * 2] | (v[66 + i * 2 + 1] << 8);
                if (c < 128) nm += static_cast<char>(c);
                else nm += '?';
            }
            out.name = nm;
            return true;
        }
        off += len;
    }
    return false;
}

// Parse the $DATA non-resident run list; returns (lcn,len) runs.
std::vector<std::pair<uint64_t, uint64_t>> parseDataRuns(const std::vector<uint8_t>& rec) {
    std::vector<std::pair<uint64_t, uint64_t>> runs;
    if (rec.size() < 56) return runs;
    uint16_t attr_off = rd16(rec.data() + 20);
    size_t off = attr_off;
    while (off + 8 <= rec.size()) {
        uint32_t type = rd32(rec.data() + off);
        if (type == 0xFFFFFFFFu) break;
        uint32_t len = rd32(rec.data() + off + 4);
        if (len == 0 || off + len > rec.size()) break;
        if (type == 0x80 && rec[off + 8] == 1) {  // $DATA non-resident
            uint16_t runoff = rd16(rec.data() + off + 32);
            size_t p = off + runoff;
            int64_t prev = 0;
            while (p < rec.size()) {
                uint8_t hdr = rec[p++];
                if (hdr == 0) break;
                int llen = hdr >> 4;
                int olen = hdr & 0x0F;
                if (llen == 0 || p + llen + olen > rec.size()) break;
                uint64_t run_len = 0;
                for (int i = 0; i < llen; i++) run_len |= uint64_t(rec[p + i]) << (8 * i);
                int64_t offs = 0;
                for (int i = 0; i < olen; i++) offs |= int64_t(rec[p + llen + i]) << (8 * i);
                if (olen > 0 && (rec[p + llen + olen - 1] & 0x80)) {
                    for (int i = olen; i < 8; i++) offs |= int64_t(-1) << (8 * i);
                }
                int64_t lcn = prev + offs;
                runs.push_back({ uint64_t(lcn), run_len });
                prev = lcn;
                p += llen + olen;
            }
            return runs;
        }
        off += len;
    }
    return runs;
}

} // namespace

// ============================================================================
// Basic conversion: FAT32 -> NTFS, data preserved, structure valid
// ============================================================================

TEST(ConvertFSTest, Fat32ToNtfsBasic) {
    std::string path;
    ASSERT_TRUE(makeImage(64, path));
    auto disk = DiskIO::openReadWrite(path);
    ASSERT_TRUE(disk && disk->isOpen());

    const uint64_t start = 2048;
    std::map<std::string, std::vector<uint8_t>> files;
    buildFat32Image(disk, start, 60, files);
    ASSERT_EQ(files.size(), 2u);

    // Record the physical offsets of each file's data BEFORE conversion.
    fat32::FAT32Layout layout;
    ASSERT_TRUE(readFatLayout(disk, start, layout));
    std::vector<uint32_t> fat;
    ASSERT_TRUE(fat32::readFATTable(disk, start, layout, 0, fat).success());

    // Find the first cluster of README.TXT from its dir entry.
    uint32_t readme_cluster = 0;
    {
        // walk root dir chain
        std::vector<uint8_t> buf(layout.sectors_per_cluster * layout.bytes_per_sector);
        uint32_t cl = layout.root_cluster;
        for (size_t guard = 0; guard < 1000; guard++) {
            if (cl < 2) break;
            uint64_t sector = start + layout.clusterToSector(cl);
            ASSERT_TRUE(disk->read(buf.data(), sector * layout.bytes_per_sector, buf.size()).success());
            const size_t count = buf.size() / 32;
            for (size_t i = 0; i < count; i++) {
                const uint8_t* e = buf.data() + i * 32;
                if (e[0] == 0x00) { /* free */ }
                else if (e[0] == 0xE5) { /* deleted */ }
                else if (e[11] == 0x0F) { /* LFN */ }
                else {
                    char nm[12];
                    std::memcpy(nm, e, 11);
                    nm[11] = 0;
                    // decode 8.3 short name: stem (8) + "." + ext (3), trimmed
                    char stem[9] = {0}, ext[4] = {0};
                    int s = 0;
                    while (s < 8 && nm[s] != ' ') { stem[s] = nm[s]; s++; }
                    int x = 0;
                    while (x < 3 && nm[8 + x] != ' ') { ext[x] = nm[8 + x]; x++; }
                    std::string n(stem);
                    if (ext[0]) { n += "."; n += ext; }
                    if (n == "README.TXT") {
                        readme_cluster = (rd16(e + 20) << 16) | rd16(e + 26);
                    }
                }
            }
            uint32_t next = fat[cl] & 0x0FFFFFFF;
            if (next >= 0x0FFFFFF8) break;
            cl = next;
        }
    }
    ASSERT_NE(readme_cluster, 0u);

    // Physical offset of README.TXT data in the image
    uint64_t readme_offset = (start + layout.clusterToSector(readme_cluster)) * layout.bytes_per_sector;

    // --- Convert ---
    Result r = convertFAT32ToNTFS(disk, start, 60ULL * 1024 * 1024, "CONVERTED");
    ASSERT_TRUE(r.success()) << r.message;

    // --- Verify 1: filesystem detection ---
    EXPECT_EQ(detectFilesystemAt(disk, start), FileSystemType::NTFS);

    // --- Verify 2: project's own checkNTFS passes ---
    std::vector<std::string> errors;
    r = ntfs::checkNTFS(disk, start, false, &errors);
    ASSERT_TRUE(r.success()) << "checkNTFS failed: " << r.message
                             << (errors.empty() ? "" : " first error: " + errors[0]);

    // --- Verify 3: data preserved at the same physical offsets ---
    {
        std::vector<uint8_t> got(files["README.TXT"].size(), 0);
        ASSERT_TRUE(disk->read(got.data(), readme_offset, got.size()).success());
        EXPECT_EQ(got, files["README.TXT"]) << "file data must be unchanged at same offset";
    }

    // --- Verify 4: independent MFT walk finds both files ---
    {
        uint8_t boot[512];
        ASSERT_TRUE(disk->readSector(boot, start).success());
        ASSERT_TRUE(std::memcmp(boot + 3, "NTFS    ", 8) == 0);
        uint8_t spc = boot[13];
        uint64_t mft_lcn = rd64(boot + 48);
        std::map<std::string, ParsedFileName> found;
        // Walk records 16..64 looking for FILE records
        for (uint64_t recnum = 16; recnum < 64; recnum++) {
            uint64_t off = (start + mft_lcn * spc) * 512 + recnum * 1024;
            std::vector<uint8_t> rec(1024, 0);
            ASSERT_TRUE(disk->read(rec.data(), off, 1024).success());
            ParsedFileName pfn;
            if (parseFileNameAttr(rec, pfn)) {
                found[pfn.name] = pfn;
            }
        }
        ASSERT_TRUE(found.count("README.TXT")) << "README.TXT not found in MFT";
        ASSERT_TRUE(found.count("REPORT")) << "REPORT not found in MFT";
        EXPECT_EQ(found["README.TXT"].data_size, 4096u);
        EXPECT_EQ(found["REPORT"].data_size, 512u);
        EXPECT_FALSE(found["README.TXT"].is_dir);
    }

    // --- Verify 5: $DATA run list of README.TXT points at the original LCN ---
    {
        uint8_t boot[512];
        ASSERT_TRUE(disk->readSector(boot, start).success());
        uint8_t spc = boot[13];
        uint64_t mft_lcn = rd64(boot + 48);
        uint64_t off = (start + mft_lcn * spc) * 512 + 16 * 1024;  // first user record
        std::vector<uint8_t> rec(1024, 0);
        ASSERT_TRUE(disk->read(rec.data(), off, 1024).success());
        ParsedFileName pfn;
        ASSERT_TRUE(parseFileNameAttr(rec, pfn));
        if (pfn.name == "README.TXT") {
            auto runs = parseDataRuns(rec);
            ASSERT_FALSE(runs.empty());
            uint64_t expected_lcn = layout.data_start_sector / layout.sectors_per_cluster +
                                    (readme_cluster - 2);
            EXPECT_EQ(runs[0].first, expected_lcn)
                << "run LCN must equal the original FAT cluster mapped to NTFS LCN";
        }
    }

    disk.reset();
    std::remove(path.c_str());
}

// ============================================================================
// Label propagation: FAT32 volume label -> NTFS $VOLUME_NAME
// ============================================================================

TEST(ConvertFSTest, LabelPropagates) {
    std::string path;
    ASSERT_TRUE(makeImage(64, path));
    auto disk = DiskIO::openReadWrite(path);
    ASSERT_TRUE(disk && disk->isOpen());

    const uint64_t start = 2048;
    fat32::formatFAT32Complete(disk, start, 60ULL * 1024 * 1024, "MYLABEL");
    Result r = convertFAT32ToNTFS(disk, start, 60ULL * 1024 * 1024, "");
    ASSERT_TRUE(r.success()) << r.message;

    // read $Volume record name
    uint8_t boot[512];
    ASSERT_TRUE(disk->readSector(boot, start).success());
    uint8_t spc = boot[13];
    uint64_t mft_lcn = rd64(boot + 48);
    uint64_t off = (start + mft_lcn * spc) * 512 + 3 * 1024;
    std::vector<uint8_t> rec(1024, 0);
    ASSERT_TRUE(disk->read(rec.data(), off, 1024).success());
    uint16_t attr_off = rd16(rec.data() + 20);
    std::string vol_name;
    size_t o = attr_off;
    while (o + 8 <= 1024) {
        uint32_t type = rd32(rec.data() + o);
        if (type == 0xFFFFFFFFu) break;
        uint32_t len = rd32(rec.data() + o + 4);
        if (len == 0 || o + len > 1024) break;
        if (type == 0x60) {  // $VOLUME_NAME
            uint32_t vlen = rd32(rec.data() + o + 16);
            uint32_t voff = rd32(rec.data() + o + 20);
            for (uint32_t i = 0; i + 1 < vlen; i += 2) {
                uint16_t c = rec[o + voff + i] | (rec[o + voff + i + 1] << 8);
                if (c < 128) vol_name += static_cast<char>(c);
            }
        }
        o += len;
    }
    EXPECT_EQ(vol_name, "MYLABEL");
    disk.reset();
    std::remove(path.c_str());
}

// ============================================================================
// convertPartitionToNTFS front-end through the partition table
// ============================================================================

TEST(ConvertFSTest, PartitionFrontEnd) {
    std::string path;
    ASSERT_TRUE(makeImage(128, path));
    auto disk = DiskIO::openReadWrite(path);
    ASSERT_TRUE(disk && disk->isOpen());

    // MBR with one FAT32 partition
    auto table = PartitionTable::create(disk, TableType::MBR);
    ASSERT_TRUE(table);
    ASSERT_TRUE(table->createPartition(2048, 100ULL * 1024 * 1024,
                                       PartitionType::FAT32LBA, "fat").success());
    ASSERT_TRUE(table->commit().success());

    fat32::formatFAT32Complete(disk, 2048, 100ULL * 1024 * 1024, "PARTLABEL");

    std::string label;
    Result r = convertPartitionToNTFS(disk, 1, &label);
    ASSERT_TRUE(r.success()) << r.message;
    EXPECT_EQ(label, "PARTLABEL");

    // Partition still intact
    auto check = PartitionTable::load(disk);
    ASSERT_TRUE(check);
    ASSERT_EQ(check->getPartitionCount(), 1);
    auto p = check->getPartition(1);
    ASSERT_TRUE(p);
    EXPECT_EQ(p->startSector(), 2048);
    EXPECT_EQ(detectFilesystemAt(disk, p->startSector()), FileSystemType::NTFS);

    // checkNTFS still passes end-to-end
    std::vector<std::string> errors;
    r = ntfs::checkNTFS(disk, p->startSector(), false, &errors);
    ASSERT_TRUE(r.success()) << r.message;

    disk.reset();
    std::remove(path.c_str());
}

// ============================================================================
// Rejects non-FAT32 and preserves data on failure
// ============================================================================

TEST(ConvertFSTest, RejectsNonFat32) {
    std::string path;
    ASSERT_TRUE(makeImage(32, path));
    auto disk = DiskIO::openReadWrite(path);
    ASSERT_TRUE(disk && disk->isOpen());

    // ext4 partition
    Result r = ext4::formatEXT4(disk, 2048, 30ULL * 1024 * 1024, "EXT");
    ASSERT_TRUE(r.success()) << r.message;

    r = convertFAT32ToNTFS(disk, 2048, 30ULL * 1024 * 1024, "");
    EXPECT_TRUE(r.failed()) << "must refuse non-FAT32 volume";

    // detection still says ext4
    EXPECT_EQ(detectFilesystemAt(disk, 2048), FileSystemType::EXT4);

    disk.reset();
    std::remove(path.c_str());
}

// ============================================================================
// Deterministic round trip: conversion twice? No — NTFS is a sink; instead
// verify idempotent detection after conversion and that re-converting refuses.
// ============================================================================

TEST(ConvertFSTest, RefusesSecondConversion) {
    std::string path;
    ASSERT_TRUE(makeImage(64, path));
    auto disk = DiskIO::openReadWrite(path);
    ASSERT_TRUE(disk && disk->isOpen());

    const uint64_t start = 2048;
    fat32::formatFAT32Complete(disk, start, 60ULL * 1024 * 1024, "L");
    ASSERT_TRUE(convertFAT32ToNTFS(disk, start, 60ULL * 1024 * 1024, "").success());
    // second conversion must fail (not FAT32 anymore)
    Result r = convertFAT32ToNTFS(disk, start, 60ULL * 1024 * 1024, "");
    EXPECT_TRUE(r.failed());

    disk.reset();
    std::remove(path.c_str());
}
