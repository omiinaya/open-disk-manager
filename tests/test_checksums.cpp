#include <gtest/gtest.h>
#include "opm/ext4_impl.hpp"
#include "opm/fat32_impl.hpp"
#include <cstring>

using namespace opm;

// ============================================================================
// CRC16 (CCITT, poly 0x1021) — used for legacy ext4 GDT checksums
// ============================================================================

TEST(ChecksumTest, CRC16KnownAnswer) {
    // CRC-16/CCITT-FALSE check value for "123456789" is 0x29B1
    const char* data = "123456789";
    uint16_t crc = ext4::crc16(0xFFFF, reinterpret_cast<const uint8_t*>(data), 9);
    EXPECT_EQ(crc, 0x29B1u);
}

TEST(ChecksumTest, CRC16EmptyInput) {
    uint16_t crc = ext4::crc16(0xFFFF, nullptr, 0);
    EXPECT_EQ(crc, 0xFFFFu);  // no bytes processed, running value unchanged
}

TEST(ChecksumTest, CRC16Deterministic) {
    const uint8_t data[] = {0x00, 0x01, 0x02, 0xFE, 0xFF, 0x80};
    uint16_t a = ext4::crc16(0x0000, data, sizeof(data));
    uint16_t b = ext4::crc16(0x0000, data, sizeof(data));
    EXPECT_EQ(a, b);
    // A single flipped bit must change the checksum
    uint8_t corrupted[sizeof(data)];
    std::memcpy(corrupted, data, sizeof(data));
    corrupted[2] ^= 0x01;
    uint16_t c = ext4::crc16(0x0000, corrupted, sizeof(corrupted));
    EXPECT_NE(a, c);
}

// ============================================================================
// ext4 group-descriptor checksum (legacy GDT_CSUM)
// ============================================================================

TEST(ChecksumTest, Ext4GroupDescChecksum) {
    uint8_t uuid[16];
    for (int i = 0; i < 16; i++) uuid[i] = static_cast<uint8_t>(i);

    ext4::EXT4GroupDesc gd;
    gd.init(0, 4096);
    gd.bg_checksum = 0;  // must be zero during computation

    uint16_t csum0 = ext4::ext4GroupDescChecksum(uuid, 0, gd);
    uint16_t csum1 = ext4::ext4GroupDescChecksum(uuid, 1, gd);
    uint16_t csum2 = ext4::ext4GroupDescChecksum(uuid, 0, gd);

    // Deterministic for the same (uuid, group, desc)
    EXPECT_EQ(csum0, csum2);
    // Different groups produce different checksums
    EXPECT_NE(csum0, csum1);
    // A non-zero stored checksum must not affect a recomputation from a
    // zeroed field (round-trip property used by verify paths)
    gd.bg_checksum = csum0;
    gd.bg_checksum = 0;
    EXPECT_EQ(ext4::ext4GroupDescChecksum(uuid, 0, gd), csum0);
}

TEST(ChecksumTest, Ext4GroupDescChecksumUUIDMatters) {
    ext4::EXT4GroupDesc gd;
    gd.init(0, 4096);
    gd.bg_checksum = 0;

    uint8_t uuid_a[16] = {0};
    uint8_t uuid_b[16] = {0};
    uuid_b[0] = 1;

    uint16_t a = ext4::ext4GroupDescChecksum(uuid_a, 0, gd);
    uint16_t b = ext4::ext4GroupDescChecksum(uuid_b, 0, gd);
    EXPECT_NE(a, b);
}

// ============================================================================
// FAT32 boot-sector checksum round trip
// ============================================================================

TEST(ChecksumTest, FAT32BootSectorChecksumRoundTrip) {
    // The checksum is stored in the first two bytes of the reserved area;
    // the field must be zeroed during computation and on verification.
    fat32::FAT32BootSector bs;
    std::memset(&bs, 0, sizeof(bs));
    bs.bs_jmp_boot[0] = 0xEB;
    bs.bs_jmp_boot[1] = 0x58;
    bs.bs_jmp_boot[2] = 0x90;
    bs.bs_boot_signature = 0xAA55;

    // Simulate the write path: zero field, compute, store LE
    bs.bpb_reserved[0] = 0;
    bs.bpb_reserved[1] = 0;
    uint16_t checksum = fat32::calculateBootSectorChecksum(bs);
    ASSERT_NE(checksum, 0u);
    bs.bpb_reserved[0] = static_cast<uint8_t>(checksum & 0xFF);
    bs.bpb_reserved[1] = static_cast<uint8_t>(checksum >> 8);

    // Simulate the verify path: read stored value, zero field, recompute
    uint16_t stored = static_cast<uint16_t>(bs.bpb_reserved[0]) |
                      (static_cast<uint16_t>(bs.bpb_reserved[1]) << 8);
    bs.bpb_reserved[0] = 0;
    bs.bpb_reserved[1] = 0;
    uint16_t computed = fat32::calculateBootSectorChecksum(bs);

    EXPECT_EQ(stored, computed);
}

TEST(ChecksumTest, FAT32BootSectorChecksumSensitiveToContent) {
    fat32::FAT32BootSector a;
    fat32::FAT32BootSector b;
    std::memset(&a, 0, sizeof(a));
    std::memset(&b, 0, sizeof(b));
    a.bs_volume_serial = 0xDEADBEEF;
    b.bs_volume_serial = 0xCAFEBABE;

    uint16_t ca = fat32::calculateBootSectorChecksum(a);
    uint16_t cb = fat32::calculateBootSectorChecksum(b);
    EXPECT_NE(ca, cb);
}
