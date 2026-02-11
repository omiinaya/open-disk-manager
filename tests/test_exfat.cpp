#include <gtest/gtest.h>
#include "opm/exfat_impl.hpp"
#include <cstring>
#include <vector>

using namespace opm;
using namespace opm::exfat;

// Test exFAT constants
TEST(ExFATTest, Constants) {
    EXPECT_EQ(EXFAT_BOOT_SIGNATURE, 0xEB);
    EXPECT_EQ(EXFAT_BOOT_SIGNATURE_2, 0x76);
    EXPECT_EQ(EXFAT_BOOT_SIGNATURE_3, 0x90);
    EXPECT_EQ(EXFAT_BOOT_SECTOR_CHECKSUM, 0xAA55);
    EXPECT_EQ(EXFAT_FIRST_DATA_CLUSTER, 2);
    EXPECT_EQ(EXFAT_CLUSTER_FREE, 0x00000000);
    EXPECT_EQ(EXFAT_CLUSTER_BAD, 0xFFFFFFF7);
    EXPECT_EQ(EXFAT_CLUSTER_END, 0xFFFFFFFF);
}

// Test exFAT boot sector initialization
TEST(ExFATTest, BootSectorInit) {
    ExFATBootSector bs;
    memset(&bs, 0, sizeof(bs));
    
    bs.init(
        0x100000,    // volume_length (32MB in sectors)
        9,           // bytes_per_sector_shift (512 bytes = 2^9)
        0,           // sectors_per_cluster_shift (1 sector per cluster = 2^0)
        24,          // fat_offset (sectors)
        256,         // fat_length (sectors)
        280,         // cluster_heap_offset (sectors)
        65407,       // cluster_count
        2,           // first_cluster_of_root
        0x12345678   // volume_serial
    );
    
    EXPECT_EQ(bs.bs_jmp[0], EXFAT_BOOT_SIGNATURE);
    EXPECT_EQ(bs.bs_jmp[1], EXFAT_BOOT_SIGNATURE_2);
    EXPECT_EQ(bs.bs_jmp[2], EXFAT_BOOT_SIGNATURE_3);
    EXPECT_EQ(bs.bs_volume_length, 0x100000);
    EXPECT_EQ(bs.bs_fat_offset, 24);
    EXPECT_EQ(bs.bs_cluster_heap_offset, 280);
    EXPECT_EQ(bs.bs_first_cluster_of_root, 2);
    EXPECT_EQ(bs.bs_boot_signature, EXFAT_BOOT_SECTOR_CHECKSUM);
}

// Test exFAT boot sector validation
TEST(ExFATTest, BootSectorValidation) {
    ExFATBootSector bs;
    memset(&bs, 0, sizeof(bs));
    
    // Invalid - no boot signature
    EXPECT_FALSE(bs.isValid());
    
    // Valid - with correct signature and minimum volume size
    bs.bs_jmp[0] = EXFAT_BOOT_SIGNATURE;
    bs.bs_jmp[1] = EXFAT_BOOT_SIGNATURE_2;
    bs.bs_jmp[2] = EXFAT_BOOT_SIGNATURE_3;
    memcpy(bs.bs_file_system_name, EXFAT_FILE_SYSTEM_NAME, 8);
    bs.bs_boot_signature = EXFAT_BOOT_SECTOR_CHECKSUM;
    bs.bs_volume_length = EXFAT_VOLUME_LENGTH_MIN;  // Minimum volume size
    bs.bs_bytes_per_sector_shift = 9;  // 512 bytes
    bs.bs_sectors_per_cluster_shift = 0;  // 1 sector per cluster
    EXPECT_TRUE(bs.isValid());
}

// Test exFAT layout calculation
TEST(ExFATTest, LayoutCalculation) {
    ExFATLayout layout;
    
    // Test 1GB volume (1GB = 2097152 sectors at 512 bytes each)
    layout.calculate(1073741824ULL);  // 1GB in bytes
    
    EXPECT_EQ(layout.bytes_per_sector_shift, 9);  // 512 bytes
    EXPECT_EQ(layout.bytes_per_sector, 512);
    EXPECT_EQ(layout.sectors_per_cluster_shift, 3);  // 4KB clusters for 1GB volume
    EXPECT_EQ(layout.sectors_per_cluster, 8);  // 8 sectors = 4KB
    EXPECT_GE(layout.volume_length, EXFAT_VOLUME_LENGTH_MIN);  // Must meet minimum
    EXPECT_TRUE(layout.validate());
}

// Test cluster/sector conversion
TEST(ExFATTest, ClusterSectorConversion) {
    ExFATLayout layout;
    layout.calculate(1073741824ULL);
    
    // Test cluster to sector conversion
    uint64_t sector = layout.clusterToSector(2);  // First data cluster
    EXPECT_EQ(sector, layout.cluster_heap_offset);
    
    // Test sector to cluster conversion
    uint32_t cluster = layout.sectorToCluster(layout.cluster_heap_offset);
    EXPECT_EQ(cluster, 2);
    
    // Test sector before cluster heap
    cluster = layout.sectorToCluster(layout.cluster_heap_offset - 1);
    EXPECT_EQ(cluster, 0);
}

// Test exFAT checksum calculation
TEST(ExFATTest, BootChecksum) {
    std::vector<uint8_t> data(11, 0);
    data[0] = EXFAT_BOOT_SIGNATURE;
    data[1] = EXFAT_BOOT_SIGNATURE_2;
    data[2] = EXFAT_BOOT_SIGNATURE_3;
    
    uint32_t checksum = calculateExFATBootChecksum(data.data(), data.size());
    EXPECT_NE(checksum, 0);  // Checksum should be calculated
}

// Test name checksum calculation
TEST(ExFATTest, NameChecksum) {
    char16_t name[] = u"test.txt";
    uint16_t checksum = calculateExFATNameChecksum(name, 8);
    EXPECT_NE(checksum, 0);  // Checksum should be calculated
}

// Test directory entry types
TEST(ExFATTest, DirectoryEntryTypes) {
    EXPECT_EQ(EXFAT_ENTRY_END, 0x00);
    EXPECT_EQ(EXFAT_ENTRY_ALLOCATION_BITMAP, 0x81);
    EXPECT_EQ(EXFAT_ENTRY_UPCASE_TABLE, 0x82);
    EXPECT_EQ(EXFAT_ENTRY_VOLUME_LABEL, 0x83);
    EXPECT_EQ(EXFAT_ENTRY_FILE, 0x85);
    EXPECT_EQ(EXFAT_ENTRY_STREAM_EXTENSION, 0xC0);
    EXPECT_EQ(EXFAT_ENTRY_FILE_NAME, 0xC1);
}

// Test file attributes
TEST(ExFATTest, FileAttributes) {
    EXPECT_EQ(EXFAT_ATTR_READ_ONLY, 0x0001);
    EXPECT_EQ(EXFAT_ATTR_HIDDEN, 0x0002);
    EXPECT_EQ(EXFAT_ATTR_SYSTEM, 0x0004);
    EXPECT_EQ(EXFAT_ATTR_VOLUME, 0x0008);
    EXPECT_EQ(EXFAT_ATTR_DIRECTORY, 0x0010);
    EXPECT_EQ(EXFAT_ATTR_ARCHIVE, 0x0020);
}

// Test volume label entry
TEST(ExFATTest, VolumeLabelEntry) {
    ExFATVolumeLabelEntry entry;
    memset(&entry, 0, sizeof(entry));
    
    entry.entry_type = EXFAT_ENTRY_VOLUME_LABEL;
    entry.character_count = 5;
    entry.volume_label[0] = u'T';
    entry.volume_label[1] = u'e';
    entry.volume_label[2] = u's';
    entry.volume_label[3] = u't';
    entry.volume_label[4] = u'\0';
    
    EXPECT_EQ(entry.entry_type, EXFAT_ENTRY_VOLUME_LABEL);
    EXPECT_EQ(entry.character_count, 5);
}

// Test allocation bitmap entry
TEST(ExFATTest, AllocationBitmapEntry) {
    ExFATAllocationBitmapEntry entry;
    memset(&entry, 0, sizeof(entry));
    
    entry.entry_type = EXFAT_ENTRY_ALLOCATION_BITMAP;
    entry.bitmap_flags = 0x00;  // First allocation bitmap
    entry.first_cluster = 2;    // First data cluster
    entry.data_length = 8192;   // 8KB bitmap
    
    EXPECT_EQ(entry.entry_type, EXFAT_ENTRY_ALLOCATION_BITMAP);
    EXPECT_EQ(entry.first_cluster, 2);
}

// Test file entry
TEST(ExFATTest, FileEntry) {
    ExFATFileEntry entry;
    memset(&entry, 0, sizeof(entry));
    
    entry.entry_type = EXFAT_ENTRY_FILE;
    entry.secondary_count = 3;  // Stream extension + file name
    entry.file_attributes = EXFAT_ATTR_ARCHIVE;
    entry.checksum = 0x1234;
    
    EXPECT_EQ(entry.entry_type, EXFAT_ENTRY_FILE);
    EXPECT_EQ(entry.secondary_count, 3);
    EXPECT_EQ(entry.file_attributes, EXFAT_ATTR_ARCHIVE);
}

// Test stream extension entry
TEST(ExFATTest, StreamExtensionEntry) {
    ExFATStreamExtensionEntry entry;
    memset(&entry, 0, sizeof(entry));
    
    entry.entry_type = EXFAT_ENTRY_STREAM_EXTENSION;
    entry.flags = 0x03;  // AllocationPossible + NoFatChain
    entry.name_length = 8;
    entry.name_hash = 0x5678;
    entry.first_cluster = 2;
    entry.data_length = 4096;
    entry.valid_data_length = 4096;
    
    EXPECT_EQ(entry.entry_type, EXFAT_ENTRY_STREAM_EXTENSION);
    EXPECT_EQ(entry.first_cluster, 2);
    EXPECT_EQ(entry.data_length, 4096);
}

// Test file name entry
TEST(ExFATTest, FileNameEntry) {
    ExFATFileNameEntry entry;
    memset(&entry, 0, sizeof(entry));
    
    entry.entry_type = EXFAT_ENTRY_FILE_NAME;
    entry.flags = 0x00;
    entry.file_name[0] = u't';
    entry.file_name[1] = u'e';
    entry.file_name[2] = u's';
    entry.file_name[3] = u't';
    
    EXPECT_EQ(entry.entry_type, EXFAT_ENTRY_FILE_NAME);
}

// Test layout validation for different volume sizes
TEST(ExFATTest, LayoutValidationSizes) {
    // Small volume (1GB - above 512MB minimum)
    {
        ExFATLayout layout;
        layout.calculate(1073741824ULL);  // 1GB
        EXPECT_GE(layout.volume_length, EXFAT_VOLUME_LENGTH_MIN);
        EXPECT_GE(layout.cluster_count, 2);
        EXPECT_TRUE(layout.validate());
    }
    
    // Medium volume (16GB)
    {
        ExFATLayout layout;
        layout.calculate(17179869184ULL);  // 16GB
        EXPECT_GE(layout.volume_length, EXFAT_VOLUME_LENGTH_MIN);
        EXPECT_GE(layout.cluster_count, 2);
        EXPECT_TRUE(layout.validate());
    }
    
    // Large volume (128GB)
    {
        ExFATLayout layout;
        layout.calculate(137438953472ULL);  // 128GB
        EXPECT_GE(layout.volume_length, EXFAT_VOLUME_LENGTH_MIN);
        EXPECT_GE(layout.cluster_count, 2);
        EXPECT_TRUE(layout.validate());
    }
}

// Test cluster chain constants
TEST(ExFATTest, ClusterChainValues) {
    // Free cluster
    EXPECT_EQ(EXFAT_CLUSTER_FREE, 0x00000000);
    
    // First valid cluster
    EXPECT_EQ(EXFAT_CLUSTER_ALLOCATED_MIN, 0x00000002);
    EXPECT_EQ(EXFAT_FIRST_DATA_CLUSTER, 2);
    
    // Bad cluster
    EXPECT_EQ(EXFAT_CLUSTER_BAD, 0xFFFFFFF7);
    
    // End of chain
    EXPECT_EQ(EXFAT_CLUSTER_END, 0xFFFFFFFF);
    
    // Last valid cluster
    EXPECT_EQ(EXFAT_CLUSTER_ALLOCATED_MAX, 0xFFFFFFF6);
}

// Test serial number generation
TEST(ExFATTest, SerialNumberGeneration) {
    uint32_t serial1 = generateExFATSerial();
    uint32_t serial2 = generateExFATSerial();
    
    // Serial numbers should be different (with very high probability)
    EXPECT_NE(serial1, serial2);
    
    // Serial should be non-zero
    EXPECT_NE(serial1, 0);
    EXPECT_NE(serial2, 0);
}
