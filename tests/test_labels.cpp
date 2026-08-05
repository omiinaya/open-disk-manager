#include <gtest/gtest.h>
#include "opm/fat32_impl.hpp"
#include "opm/ntfs_impl.hpp"
#include "opm/ext4_impl.hpp"
#include "opm/exfat_impl.hpp"
#include "opm/disk_io.hpp"
#include "opm/filesystem.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
#include "test_util.hpp"

using namespace opm;

namespace {

bool makeImage(uint64_t mb, std::string& path) {
    path = test_tmp_dir() + "/opm_lbl_" + std::to_string(::getpid()) + "_" +
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

// Read the FAT32 root-directory volume-label entry name (11 bytes).
std::string readFAT32Label(std::shared_ptr<DiskIO> disk, uint64_t start,
                           const fat32::FAT32Layout& layout) {
    uint64_t root_sec = start + layout.clusterToSector(layout.root_cluster);
    std::vector<uint8_t> data(layout.bytes_per_sector, 0);
    if (disk->readSector(data.data(), root_sec).failed()) return "";
    fat32::FAT32DirEntry* e = reinterpret_cast<fat32::FAT32DirEntry*>(data.data());
    size_t n = layout.bytes_per_sector / 32;
    for (size_t i = 0; i < n; i++) {
        if (e[i].isVolumeLabel()) {
            std::string s(e[i].dir_name, 11);
            // trim spaces
            while (!s.empty() && s.back() == ' ') s.pop_back();
            return s;
        }
    }
    return "";
}

} // namespace

TEST(LabelTest, FAT32SetAndRelabel) {
    std::string path; ASSERT_TRUE(makeImage(96, path));
    auto disk = DiskIO::openReadWrite(path);
    ASSERT_TRUE(disk && disk->isOpen());

    Result r = fat32::formatFAT32Complete(disk, 2048, 64ULL * 1024 * 1024, "OLD");
    ASSERT_TRUE(r.success()) << r.message;

    fat32::FAT32BootSector bs; fat32::FAT32Layout layout;
    r = fat32::getFAT32Info(disk, 2048, bs, layout);
    ASSERT_TRUE(r.success()) << r.message;
    EXPECT_EQ(readFAT32Label(disk, 2048, layout), "OLD");

    r = fat32::setLabel(disk, 2048, "NEW");
    ASSERT_TRUE(r.success()) << r.message;
    EXPECT_EQ(readFAT32Label(disk, 2048, layout), "NEW");

    disk->close(); std::remove(path.c_str());
}

TEST(LabelTest, exFATSetLabel) {
    // exFAT requires volume_length >= 0x100000 sectors (512 MB).
    std::string path; ASSERT_TRUE(makeImage(640, path));
    auto disk = DiskIO::openReadWrite(path);
    ASSERT_TRUE(disk && disk->isOpen());

    Result r = exfat::formatExFAT(disk, 2048, 600ULL * 1024 * 1024, "OLD");
    ASSERT_TRUE(r.success()) << r.message;

    r = exfat::setLabel(disk, 2048, "NEW");
    ASSERT_TRUE(r.success()) << r.message;

    // Verify raw volume-label entry in the root directory.
    exfat::ExFATBootSector boot;
    ASSERT_TRUE(disk->readSector(&boot, 2048).success());
    uint32_t bps = 1u << boot.bs_bytes_per_sector_shift;
    uint32_t spc = 1u << boot.bs_sectors_per_cluster_shift;
    uint64_t root_byte = (2048 + boot.bs_cluster_heap_offset +
                          (boot.bs_first_cluster_of_root - 2) * spc) * bps;
    std::vector<uint8_t> data(bps * spc, 0);
    ASSERT_TRUE(disk->read(data.data(), root_byte, data.size()).success());
    // Entry 0 should be the volume label.
    EXPECT_EQ(static_cast<int>(data[0]), 0x83);
    EXPECT_EQ(static_cast<int>(data[1]), 3);  // 3 chars
    EXPECT_EQ(static_cast<char>(data[2]), 'N');
    EXPECT_EQ(static_cast<char>(data[4]), 'E');

    disk->close(); std::remove(path.c_str());
}

TEST(LabelTest, ext4SetLabel) {
    std::string path; ASSERT_TRUE(makeImage(96, path));
    auto disk = DiskIO::openReadWrite(path);
    ASSERT_TRUE(disk && disk->isOpen());

    Result r = ext4::formatEXT4(disk, 2048, 64ULL * 1024 * 1024, "OLD");
    ASSERT_TRUE(r.success()) << r.message;

    r = ext4::setLabel(disk, 2048, "NEW");
    ASSERT_TRUE(r.success()) << r.message;

    FSInfo info;
    auto fs = createFileSystem(FileSystemType::EXT4);
    ASSERT_TRUE(fs);
    r = fs->getInfo(disk, 2048, info);
    ASSERT_TRUE(r.success()) << r.message;
    EXPECT_EQ(info.label, "NEW");

    disk->close(); std::remove(path.c_str());
}

TEST(LabelTest, NTFSFormatWritesLabelAndRelabels) {
    std::string path; ASSERT_TRUE(makeImage(96, path));
    auto disk = DiskIO::openReadWrite(path);
    ASSERT_TRUE(disk && disk->isOpen());

    // Formatting with a label should now produce a real $VOLUME_NAME.
    Result r = ntfs::formatNTFS(disk, 2048, 64ULL * 1024 * 1024, "OLD");
    ASSERT_TRUE(r.success()) << r.message;

    r = ntfs::setLabel(disk, 2048, "NEW2");
    ASSERT_TRUE(r.success()) << r.message;

    // Read MFT record 3 ($Volume) and look for the $VOLUME_NAME (0x60)
    // resident attribute at the beginning.
    uint8_t boot[512];
    ASSERT_TRUE(disk->readSector(boot, 2048).success());
    auto rd16 = [&](int o){ return (uint16_t)boot[o] | ((uint16_t)boot[o+1] << 8); };
    auto rd64 = [&](int o){
        uint64_t v=0; for(int i=0;i<8;i++) v |= (uint64_t)boot[o+i] << (8*i); return v;
    };
    uint16_t bps = rd16(11);
    uint8_t spc = boot[13];
    uint64_t mft_lcn = rd64(48);
    int8_t cpmr = (int8_t)boot[64];
    uint32_t rec_size = cpmr > 0 ? (uint32_t)cpmr*bps*spc : (1u << (-cpmr));
    uint64_t rec_off = (2048 + mft_lcn*spc)*bps + 3*rec_size;

    std::vector<uint8_t> rec(rec_size, 0);
    ASSERT_TRUE(disk->read(rec.data(), rec_off, rec_size).success());
    // Magic "FILE"
    EXPECT_EQ(rec[0], 'F'); EXPECT_EQ(rec[1], 'I'); EXPECT_EQ(rec[2], 'L'); EXPECT_EQ(rec[3], 'E');

    // Scan resident attributes from offset 56 for $VOLUME_NAME (0x60).
    std::string found;
    size_t off = 56;
    while (off + 8 <= rec.size()) {
        uint32_t type = (uint32_t)rec[off] | ((uint32_t)rec[off+1]<<8) |
                        ((uint32_t)rec[off+2]<<16) | ((uint32_t)rec[off+3]<<24);
        uint32_t alen  = (uint32_t)rec[off+4] | ((uint32_t)rec[off+5]<<8) |
                         ((uint32_t)rec[off+6]<<16) | ((uint32_t)rec[off+7]<<24);
        if (type == 0xFFFFFFFFu) break;
        if (alen < 24 || off + alen > rec.size()) break;
        if (type == 0x60u) {
            uint32_t vlen = (uint32_t)rec[off+16] | ((uint32_t)rec[off+17]<<8) |
                            ((uint32_t)rec[off+18]<<16) | ((uint32_t)rec[off+19]<<24);
            uint16_t voff = (uint16_t)rec[off+20] | ((uint16_t)rec[off+21]<<8);
            for (uint32_t i = 0; i + 1 < vlen && i + 1 < alen; i += 2) {
                uint8_t c = rec[off + voff + i];
                if (c) found += static_cast<char>(c);
            }
        }
        off += alen;
    }
    EXPECT_EQ(found, "NEW2");

    disk->close(); std::remove(path.c_str());
}