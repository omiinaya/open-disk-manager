#include <gtest/gtest.h>
#include "opm/boot.hpp"
#include "opm/disk_io.hpp"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include "test_util.hpp"

using namespace opm;

namespace {

// Build a minimal ISO9660 image with a single file at the root:
//   /HELLO.TXT  containing "hello world"
// Layout: PVD at sector 16, root directory at sector 20, file at sector 21.
void writeLE16(uint8_t* p, uint16_t v) { p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; }
void writeBE16(uint8_t* p, uint16_t v) { p[0] = (v >> 8) & 0xFF; p[1] = v & 0xFF; }
void writeLE32(uint8_t* p, uint32_t v) {
    for (int i = 0; i < 4; i++) p[i] = (v >> (8 * i)) & 0xFF;
}
void writeBE32(uint8_t* p, uint32_t v) {
    for (int i = 0; i < 4; i++) p[i] = (v >> (8 * (3 - i))) & 0xFF;
}

std::string makeTestISO(const std::string& path) {
    constexpr uint32_t SECTOR = 2048;
    constexpr uint32_t TOTAL_SECTORS = 22;
    std::vector<uint8_t> image(TOTAL_SECTORS * SECTOR, 0);

    // --- PVD at sector 16 ---
    uint8_t* pvd = &image[16 * SECTOR];
    pvd[0] = 1;                                   // type: primary volume descriptor
    std::memcpy(pvd + 1, "CD001", 5);             // identifier
    pvd[6] = 1;                                   // version
    // volume space size (both-endian at offset 80)
    writeLE32(pvd + 80, TOTAL_SECTORS);
    writeBE32(pvd + 84, TOTAL_SECTORS);
    // logical block size (both-endian at offset 128) = 2048
    writeLE16(pvd + 128, SECTOR);
    writeBE16(pvd + 130, SECTOR);

    // Root directory record at PVD offset 156 (34 bytes)
    uint8_t* root_rec = pvd + 156;
    root_rec[0] = 34;                              // len_dr
    root_rec[1] = 0;                               // ext attr len
    writeLE32(root_rec + 2, 20);                   // extent LBA (LE)
    writeBE32(root_rec + 6, 20);                   // extent LBA (BE)
    writeLE32(root_rec + 10, SECTOR);              // data length (LE)
    writeBE32(root_rec + 14, SECTOR);              // data length (BE)
    root_rec[25] = 0x02;                           // flags: directory
    root_rec[32] = 1;                              // name len
    root_rec[33] = 0x00;                           // name = root

    // --- Root directory at sector 20 ---
    uint8_t* dir = &image[20 * SECTOR];
    // "." entry
    dir[0] = 34; dir[1] = 0;
    writeLE32(dir + 2, 20); writeBE32(dir + 6, 20);
    writeLE32(dir + 10, SECTOR); writeBE32(dir + 14, SECTOR);
    dir[25] = 0x02; dir[32] = 1; dir[33] = 0x00;
    // ".." entry at offset 34
    uint8_t* up = dir + 34;
    up[0] = 34; up[1] = 0;
    writeLE32(up + 2, 20); writeBE32(up + 6, 20);
    writeLE32(up + 10, SECTOR); writeBE32(up + 14, SECTOR);
    up[25] = 0x02; up[32] = 1; up[33] = 0x01;
    // "HELLO.TXT;1" entry at offset 68
    uint8_t* file_rec = dir + 68;
    file_rec[0] = 42; file_rec[1] = 0;
    writeLE32(file_rec + 2, 21); writeBE32(file_rec + 6, 21);
    writeLE32(file_rec + 10, 11); writeBE32(file_rec + 14, 11);
    file_rec[25] = 0x00;                           // regular file
    file_rec[32] = 11;                             // name len
    std::memcpy(file_rec + 33, "HELLO.TXT;1", 11);

    // --- File data at sector 21 ---
    std::memcpy(&image[21 * SECTOR], "hello world", 11);

    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(image.data()), image.size());
    out.close();
    return path;
}

} // namespace

TEST(BootTest, ExtractISOSingleFile) {
    std::string iso = test_tmp_dir() + "/opm_test_iso_" + std::to_string(::getpid()) + ".iso";
    std::string outdir = test_tmp_dir() + "/opm_test_iso_out_" + std::to_string(::getpid());
    makeTestISO(iso);

    Result r = extractISO(iso, outdir);
    ASSERT_TRUE(r.success()) << r.message;

    std::string extracted = outdir + "/HELLO.TXT";
    ASSERT_TRUE(std::filesystem::exists(extracted));
    std::ifstream in(extracted, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
    EXPECT_EQ(content, "hello world");

    std::remove(iso.c_str());
    // NFS can be flaky with recursive remove_all; clean up best-effort.
    std::error_code ec;
    std::filesystem::remove(extracted, ec);
    ec.clear();
    std::filesystem::remove_all(outdir, ec);
}

TEST(BootTest, ExtractISORejectsInvalid) {
    std::string bad = test_tmp_dir() + "/opm_test_bad.iso";
    std::ofstream out(bad, std::ios::binary);
    out.write("this is not an iso image", 24);
    out.close();

    Result r = extractISO(bad, test_tmp_dir() + "/opm_test_bad_out");
    EXPECT_TRUE(r.failed());

    std::remove(bad.c_str());
    std::filesystem::remove_all(test_tmp_dir() + "/opm_test_bad_out");
}
