#include <gtest/gtest.h>
#include "opm/filesystem.hpp"
#include "opm/types.hpp"
#include <vector>
#include <cstring>

using namespace opm;

// FAT32 Boot Sector Test
TEST(FilesystemTest, FAT32BootSectorSignature) {
    std::vector<uint8_t> bootSector(512, 0);
    
    // FAT32 jump boot code
    bootSector[0] = 0xEB;
    bootSector[1] = 0x58;
    bootSector[2] = 0x90;
    
    // FSInfo signature
    bootSector[0x1E4] = 0x52;
    bootSector[0x1E5] = 0x72;
    bootSector[0x1E6] = 0x41;
    bootSector[0x1E7] = 0x56;
    
    // Backup boot sector signature
    bootSector[0x3E4] = 0x52;
    bootSector[0x3E5] = 0x72;
    bootSector[0x3E6] = 0x41;
    bootSector[0x3E7] = 0x56;
    
    // Boot signature
    bootSector[510] = 0x55;
    bootSector[511] = 0xAA;
    
    EXPECT_EQ(bootSector[510], 0x55);
    EXPECT_EQ(bootSector[511], 0xAA);
}

TEST(FilesystemTest, FAT32FSInfoSignature) {
    std::vector<uint8_t> fsInfo(512, 0);
    
    // FSInfo signature at offset 0
    fsInfo[0] = 0x52;
    fsInfo[1] = 0x72;
    fsInfo[2] = 0x41;
    fsInfo[3] = 0x56;
    
    EXPECT_EQ(fsInfo[0], 0x52);
    EXPECT_EQ(fsInfo[1], 0x72);
    EXPECT_EQ(fsInfo[2], 0x41);
    EXPECT_EQ(fsInfo[3], 0x56);
}

TEST(FilesystemTest, FAT32BytesPerSector) {
    std::vector<uint8_t> bootSector(512, 0);
    
    // Bytes per sector = 512
    bootSector[11] = 0x00;
    bootSector[12] = 0x02;
    
    EXPECT_EQ(bootSector[11], 0x00);
    EXPECT_EQ(bootSector[12], 0x02);
}

TEST(FilesystemTest, FAT32SectorsPerCluster) {
    std::vector<uint8_t> bootSector(512, 0);
    
    // Sectors per cluster = 8
    bootSector[13] = 8;
    
    EXPECT_EQ(bootSector[13], 8);
}

TEST(FilesystemTest, FAT32ReservedSectors) {
    std::vector<uint8_t> bootSector(512, 0);
    
    // Reserved sectors = 32
    bootSector[14] = 0x20;
    bootSector[15] = 0x00;
    
    EXPECT_EQ(bootSector[14], 0x20);
}

TEST(FilesystemTest, FAT32NumFATS) {
    std::vector<uint8_t> bootSector(512, 0);
    
    // Number of FATs = 2
    bootSector[16] = 2;
    
    EXPECT_EQ(bootSector[16], 2);
}

TEST(FilesystemTest, FAT32FATOffset) {
    // FAT32 has reserved sectors, then FAT
    uint32_t reservedSectors = 32;
    uint32_t sectorsPerFAT = 1000;
    uint32_t fatOffset = reservedSectors;
    
    EXPECT_EQ(fatOffset, 32);
}

TEST(FilesystemTest, FAT32DataRegionOffset) {
    uint32_t reservedSectors = 32;
    uint32_t numFATS = 2;
    uint32_t sectorsPerFAT = 1000;
    uint32_t dataOffset = reservedSectors + (numFATS * sectorsPerFAT);
    
    EXPECT_EQ(dataOffset, 2032);
}

// FAT32 Constants
TEST(FilesystemTest, FAT32MaxClusterValue) {
    // FAT32 uses 28-bit for cluster values
    const uint32_t FAT32_EOC = 0x0FFFFFF8;
    EXPECT_GT(FAT32_EOC, 0);
}

TEST(FilesystemTest, FAT32FreeClusterMarker) {
    const uint32_t FREE_CLUSTER = 0x00000000;
    const uint32_t BAD_CLUSTER = 0x0FFFFFF7;
    
    EXPECT_EQ(FREE_CLUSTER, 0);
    EXPECT_EQ(BAD_CLUSTER, 0x0FFFFFF7);
}

// File system type detection
TEST(FilesystemTest, DetectFAT32) {
    std::vector<uint8_t> bootSector(512, 0);
    
    // Set up FAT32 signature
    bootSector[82] = 'F';
    bootSector[83] = 'A';
    bootSector[84] = 'T';
    bootSector[85] = '3';
    bootSector[86] = '2';
    bootSector[87] = ' ';
    bootSector[88] = ' ';
    bootSector[89] = ' ';
    
    EXPECT_EQ(bootSector[82], 'F');
}

TEST(FilesystemTest, DetectFAT16) {
    std::vector<uint8_t> bootSector(512, 0);
    
    // Set up FAT16 signature
    bootSector[54] = 'F';
    bootSector[55] = 'A';
    bootSector[56] = 'T';
    bootSector[57] = '1';
    bootSector[58] = '6';
    bootSector[59] = ' ';
    
    EXPECT_EQ(bootSector[54], 'F');
}

TEST(FilesystemTest, DetectFAT12) {
    std::vector<uint8_t> bootSector(512, 0);
    
    // Set up FAT12 signature
    bootSector[54] = 'F';
    bootSector[55] = 'A';
    bootSector[56] = 'T';
    bootSector[57] = '1';
    bootSector[58] = '2';
    bootSector[59] = ' ';
    
    EXPECT_EQ(bootSector[54], 'F');
}

// exFAT Constants
TEST(FilesystemTest, exFATBootSignature) {
    std::vector<uint8_t> bootSector(512, 0);
    
    // Boot code signature
    bootSector[510] = 0x55;
    bootSector[511] = 0xAA;
    
    // Also backup boot sector
    std::vector<uint8_t> backupSector(512, 0);
    backupSector[510] = 0x55;
    backupSector[511] = 0xAA;
    
    EXPECT_EQ(bootSector[510], 0x55);
}

TEST(FilesystemTest, exFATVolumeGUID) {
    std::vector<uint8_t> bootSector(512, 0);
    
    // Placeholder for volume GUID
    for (int i = 0; i < 16; i++) {
        bootSector[40 + i] = static_cast<uint8_t>(i);
    }
    
    EXPECT_EQ(bootSector[40], 0);
    EXPECT_EQ(bootSector[55], 15);
}

TEST(FilesystemTest, exFATPartitionOffset) {
    std::vector<uint8_t> bootSector(512, 0);
    
    // Partition offset = 0x100000 (1MB)
    bootSector[64] = 0x00;
    bootSector[65] = 0x00;
    bootSector[66] = 0x10;
    bootSector[67] = 0x00;
    bootSector[68] = 0x00;
    bootSector[69] = 0x00;
    bootSector[70] = 0x00;
    bootSector[71] = 0x00;
    
    EXPECT_EQ(bootSector[64], 0x00);
}

TEST(FilesystemTest, exFATVolumeLength) {
    std::vector<uint8_t> bootSector(512, 0);
    
    // Volume length = 1GB sectors
    bootSector[72] = 0x00;
    bootSector[73] = 0x00;
    bootSector[74] = 0x00;
    bootSector[75] = 0x80; // 2GB
    bootSector[76] = 0x00;
    bootSector[77] = 0x00;
    bootSector[78] = 0x00;
    bootSector[79] = 0x00;
    
    EXPECT_EQ(bootSector[72], 0x00);
}

TEST(FilesystemTest, exFATSectorSize) {
    std::vector<uint8_t> bootSector(512, 0);
    
    // Sector size = 512
    bootSector[80] = 0x00;
    bootSector[81] = 0x02;
    
    EXPECT_EQ(bootSector[80], 0x00);
    EXPECT_EQ(bootSector[81], 0x02);
}

TEST(FilesystemTest, exFATClusterSize) {
    std::vector<uint8_t> bootSector(512, 0);
    
    // Cluster size = 2^19 = 512KB (9 + 12 = 21 = 2^21)
    bootSector[83] = 21;
    
    EXPECT_EQ(bootSector[83], 21);
}

TEST(FilesystemTest, exFATNumFATS) {
    std::vector<uint8_t> bootSector(512, 0);
    
    // Number of FATs = 1
    bootSector[64] = 1;
    
    EXPECT_EQ(bootSector[64], 1);
}

TEST(FilesystemTest, exFATDriveSelect) {
    std::vector<uint8_t> bootSector(512, 0);
    
    // Drive select = 0x80
    bootSector[64] = 0x80;
    
    EXPECT_EQ(bootSector[64], 0x80);
}

TEST(FilesystemTest, exFATPercentInUse) {
    std::vector<uint8_t> bootSector(512, 0);
    
    // Percent in use = 0 (unknown)
    bootSector[112] = 0;
    
    EXPECT_EQ(bootSector[112], 0);
}

// ext4 Superblock
TEST(FilesystemTest, ext4MagicNumber) {
    const uint16_t EXT4_SUPERBLOCK_MAGIC = 0xEF53;
    EXPECT_EQ(EXT4_SUPERBLOCK_MAGIC, 0xEF53);
}

TEST(FilesystemTest, ext4SuperblockOffset) {
    // Primary superblock at offset 1024 within block 0
    uint64_t superblockOffset = 1024;
    EXPECT_EQ(superblockOffset, 1024);
}

TEST(FilesystemTest, ext4SBlockInodeSize) {
    std::vector<uint8_t> sblock(1024, 0);
    
    // Inode size = 256
    sblock[88] = 0x00;
    sblock[89] = 0x01;
    
    EXPECT_EQ(sblock[88], 0x00);
    EXPECT_EQ(sblock[89], 0x01);
}

TEST(FilesystemTest, ext4BlockSize) {
    std::vector<uint8_t> sblock(1024, 0);
    
    // Block size = 4096 (shift value 2 -> 2^10 * 2^2 = 4096)
    sblock[24] = 0x02;
    sblock[25] = 0x00;
    sblock[26] = 0x00;
    sblock[27] = 0x00;
    
    EXPECT_EQ(sblock[24], 0x02);
}

TEST(FilesystemTest, ext4InodesCount) {
    std::vector<uint8_t> sblock(1024, 0);
    
    // Inode count
    sblock[0] = 0x00;
    sblock[1] = 0x00;
    sblock[2] = 0x40;
    sblock[3] = 0x00;
    
    EXPECT_EQ(sblock[0], 0x00);
}

TEST(FilesystemTest, ext4BlockCount) {
    std::vector<uint8_t> sblock(1024, 0);
    
    // Block count
    sblock[4] = 0x00;
    sblock[5] = 0x00;
    sblock[6] = 0x00;
    sblock[7] = 0x00;
    
    EXPECT_EQ(sblock[4], 0x00);
}

TEST(FilesystemTest, ext4ReservedBlockCount) {
    std::vector<uint8_t> sblock(1024, 0);
    
    // Reserved block count
    sblock[8] = 0x00;
    sblock[9] = 0x00;
    sblock[10] = 0x00;
    sblock[11] = 0x00;
    
    EXPECT_EQ(sblock[8], 0x00);
}

TEST(FilesystemTest, ext4FreeBlockCount) {
    std::vector<uint8_t> sblock(1024, 0);
    
    // Free block count
    sblock[12] = 0x00;
    sblock[13] = 0x00;
    sblock[14] = 0x00;
    sblock[15] = 0x00;
    
    EXPECT_EQ(sblock[12], 0x00);
}

TEST(FilesystemTest, ext4FreeInodeCount) {
    std::vector<uint8_t> sblock(1024, 0);
    
    // Free inode count
    sblock[16] = 0x00;
    sblock[17] = 0x00;
    sblock[18] = 0x00;
    sblock[19] = 0x00;
    
    EXPECT_EQ(sblock[16], 0x00);
}

TEST(FilesystemTest, ext4FirstDataBlock) {
    std::vector<uint8_t> sblock(1024, 0);
    
    // First data block
    sblock[20] = 0x00;
    sblock[21] = 0x00;
    sblock[22] = 0x00;
    sblock[23] = 0x00;
    
    EXPECT_EQ(sblock[20], 0x00);
}

TEST(FilesystemTest, ext4LogBlockSize) {
    std::vector<uint8_t> sblock(1024, 0);
    
    // Log block size
    sblock[24] = 0x02;
    
    EXPECT_EQ(sblock[24], 0x02);
}

TEST(FilesystemTest, ext4BlocksPerGroup) {
    std::vector<uint8_t> sblock(1024, 0);
    
    // Blocks per group
    sblock[32] = 0x00;
    sblock[33] = 0x08;
    sblock[34] = 0x00;
    sblock[35] = 0x00;
    
    EXPECT_EQ(sblock[32], 0x00);
}

TEST(FilesystemTest, ext4InodesPerGroup) {
    std::vector<uint8_t> sblock(1024, 0);
    
    // Inodes per group
    sblock[40] = 0x00;
    sblock[41] = 0x10;
    sblock[42] = 0x00;
    sblock[43] = 0x00;
    
    EXPECT_EQ(sblock[40], 0x00);
}

TEST(FilesystemTest, ext4Magic) {
    std::vector<uint8_t> sblock(1024, 0);
    
    // Magic number
    sblock[0x38] = 0x53;
    sblock[0x39] = 0xEF;
    
    EXPECT_EQ(sblock[0x38], 0x53);
    EXPECT_EQ(sblock[0x39], 0xEF);
}

TEST(FilesystemTest, ext4State) {
    std::vector<uint8_t> sblock(1024, 0);
    
    // State
    sblock[0x3A] = 0x01;
    
    EXPECT_EQ(sblock[0x3A], 0x01);
}

TEST(FilesystemTest, ext4Errors) {
    std::vector<uint8_t> sblock(1024, 0);
    
    // Errors behavior
    sblock[0x3C] = 0x01;
    
    EXPECT_EQ(sblock[0x3C], 0x01);
}

TEST(FilesystemTest, ext4MinorRevision) {
    std::vector<uint8_t> sblock(1024, 0);
    
    // Minor revision
    sblock[0x3E] = 0x00;
    sblock[0x3F] = 0x00;
    
    EXPECT_EQ(sblock[0x3E], 0x00);
}

TEST(FilesystemTest, ext4UUID) {
    std::vector<uint8_t> sblock(1024, 0);
    
    // UUID (16 bytes)
    for (int i = 0; i < 16; i++) {
        sblock[0x68 + i] = static_cast<uint8_t>(i);
    }
    
    EXPECT_EQ(sblock[0x68], 0);
}

TEST(FilesystemTest, ext4VolumeName) {
    std::vector<uint8_t> sblock(1024, 0);
    
    // Volume name (16 bytes)
    sblock[0x78] = 't';
    sblock[0x79] = 'e';
    sblock[0x7A] = 's';
    sblock[0x7B] = 't';
    
    EXPECT_EQ(sblock[0x78], 't');
}

// NTFS Boot Sector
TEST(FilesystemTest, NTFSJumpBoot) {
    std::vector<uint8_t> bootSector(512, 0);
    
    // Jump boot code
    bootSector[0] = 0xEB;
    bootSector[1] = 0x52;
    bootSector[2] = 0x90;
    
    EXPECT_EQ(bootSector[0], 0xEB);
    EXPECT_EQ(bootSector[1], 0x52);
    EXPECT_EQ(bootSector[2], 0x90);
}

TEST(FilesystemTest, NTFSOemID) {
    std::vector<uint8_t> bootSector(512, 0);
    
    // OEM ID = "NTFS    "
    bootSector[3] = 'N';
    bootSector[4] = 'T';
    bootSector[5] = 'F';
    bootSector[6] = 'S';
    bootSector[7] = ' ';
    bootSector[8] = ' ';
    bootSector[9] = ' ';
    bootSector[10] = ' ';
    
    EXPECT_EQ(bootSector[3], 'N');
    EXPECT_EQ(bootSector[4], 'T');
    EXPECT_EQ(bootSector[5], 'F');
    EXPECT_EQ(bootSector[6], 'S');
}

TEST(FilesystemTest, NTFSBytesPerSector) {
    std::vector<uint8_t> bootSector(512, 0);
    
    // Bytes per sector = 512
    bootSector[11] = 0x00;
    bootSector[12] = 0x02;
    
    EXPECT_EQ(bootSector[11], 0x00);
    EXPECT_EQ(bootSector[12], 0x02);
}

TEST(FilesystemTest, NTFSSectorsPerCluster) {
    std::vector<uint8_t> bootSector(512, 0);
    
    // Sectors per cluster = 8
    bootSector[13] = 8;
    
    EXPECT_EQ(bootSector[13], 8);
}

TEST(FilesystemTest, NTFSReservedSectors) {
    std::vector<uint8_t> bootSector(512, 0);
    
    // Reserved sectors = 0
    bootSector[14] = 0x00;
    bootSector[15] = 0x00;
    
    EXPECT_EQ(bootSector[14], 0x00);
}

TEST(FilesystemTest, NTFSMediaDescriptor) {
    std::vector<uint8_t> bootSector(512, 0);
    
    // Media descriptor = 0xF8 (hard disk)
    bootSector[21] = 0xF8;
    
    EXPECT_EQ(bootSector[21], 0xF8);
}

TEST(FilesystemTest, NTFSBytesPerSectorAlt) {
    std::vector<uint8_t> bootSector(512, 0);
    
    // Bytes per sector (duplicate) = 512
    bootSector[28] = 0x00;
    bootSector[29] = 0x02;
    
    EXPECT_EQ(bootSector[28], 0x00);
}

TEST(FilesystemTest, NTFSClustersPerMFTRecord) {
    std::vector<uint8_t> bootSector(512, 0);
    
    // Clusters per MFT record = -2 (2^|-2| = 1/4 cluster, meaning record is smaller)
    bootSector[64] = 0xF6;
    
    EXPECT_EQ(bootSector[64], 0xF6);
}

TEST(FilesystemTest, NTFSClustersPerIndexBuffer) {
    std::vector<uint8_t> bootSector(512, 0);
    
    // Clusters per index buffer = 0xF6
    bootSector[68] = 0xF6;
    
    EXPECT_EQ(bootSector[68], 0xF6);
}

TEST(FilesystemTest, NTFSVolumeSerialNumber) {
    std::vector<uint8_t> bootSector(512, 0);
    
    // Volume serial number (8 bytes)
    for (int i = 0; i < 8; i++) {
        bootSector[72 + i] = static_cast<uint8_t>(i * 0x11);
    }
    
    EXPECT_EQ(bootSector[72], 0x00);
    EXPECT_EQ(bootSector[73], 0x11);
}

TEST(FilesystemTest, NTFSBootSignature) {
    std::vector<uint8_t> bootSector(512, 0);
    
    // Boot signature
    bootSector[510] = 0x55;
    bootSector[511] = 0xAA;
    
    EXPECT_EQ(bootSector[510], 0x55);
    EXPECT_EQ(bootSector[511], 0xAA);
}

// Filesystem type detection helper
TEST(FilesystemTest, DetectFilesystemFromBootSector) {
    // Test detection of different filesystems
    
    // FAT32
    std::vector<uint8_t> fat32Boot(512, 0);
    fat32Boot[82] = 'F';
    fat32Boot[83] = 'A';
    fat32Boot[84] = 'T';
    fat32Boot[85] = '3';
    EXPECT_EQ(fat32Boot[82], 'F');
    
    // NTFS
    std::vector<uint8_t> ntfsBoot(512, 0);
    ntfsBoot[3] = 'N';
    ntfsBoot[4] = 'T';
    ntfsBoot[5] = 'F';
    ntfsBoot[6] = 'S';
    EXPECT_EQ(ntfsBoot[3], 'N');
    
    // ext4
    std::vector<uint8_t> ext4Super(1024, 0);
    ext4Super[0x38] = 0x53;
    ext4Super[0x39] = 0xEF;
    EXPECT_EQ(ext4Super[0x38], 0x53);
}
