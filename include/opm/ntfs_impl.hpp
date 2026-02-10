#pragma once

#include "types.hpp"
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>
#include <string>

namespace opm {

// Forward declaration
class DiskIO;

namespace ntfs {

// Use the forward declaration
using ::opm::DiskIO;

// ============================================================================
// NTFS Constants
// ============================================================================

// NTFS magic
constexpr uint8_t NTFS_OEM_ID[8] = {'N', 'T', 'F', 'S', ' ', ' ', ' ', ' '};
constexpr uint32_t NTFS_BOOT_SECTOR_SIZE = 512;
constexpr uint32_t NTFS_SECTOR_SIZE = 512;
constexpr uint16_t NTFS_BOOT_SIGNATURE = 0xAA55;

// MFT (Master File Table) constants
constexpr uint32_t MFT_RECORD_MAGIC = 0x454C4946;  // "FILE"
constexpr uint32_t INDX_RECORD_MAGIC = 0x58444E49; // "INDX"
constexpr uint32_t BAAD_RECORD_MAGIC = 0x44414142; // "BAAD" (corrupted)
constexpr uint32_t EMPTY_RECORD_MAGIC = 0x00000000;  // Empty record

// MFT record numbers
constexpr uint64_t MFT_MFT = 0;          // $MFT itself
constexpr uint64_t MFT_MIRR = 1;         // $MFTMirr
constexpr uint64_t MFT_LOGFILE = 2;      // $LogFile
constexpr uint64_t MFT_VOLUME = 3;       // $Volume
constexpr uint64_t MFT_ATTRDEF = 4;        // $AttrDef
constexpr uint64_t MFT_ROOT = 5;         // $Root (root directory)
constexpr uint64_t MFT_BITMAP = 6;       // $Bitmap
constexpr uint64_t MFT_BOOT = 7;         // $Boot
constexpr uint64_t MFT_BADCLUS = 8;      // $BadClus
constexpr uint64_t MFT_SECURE = 9;       // $Secure
constexpr uint64_t MFT_UPCASE = 10;      // $UpCase
constexpr uint64_t MFT_EXTEND = 11;      // $Extend
constexpr uint64_t MFT_RESERVED_12 = 12;   // Reserved
constexpr uint64_t MFT_RESERVED_15 = 15;   // Reserved
constexpr uint64_t MFT_USER_START = 16;   // First user file

// Attribute types
constexpr uint32_t ATTR_STANDARD_INFO = 0x10;      // $STANDARD_INFORMATION
constexpr uint32_t ATTR_ATTRIBUTE_LIST = 0x20;    // $ATTRIBUTE_LIST
constexpr uint32_t ATTR_FILE_NAME = 0x30;        // $FILE_NAME
constexpr uint32_t ATTR_OBJECT_ID = 0x40;        // $OBJECT_ID
constexpr uint32_t ATTR_SECURITY_DESC = 0x50;    // $SECURITY_DESCRIPTOR
constexpr uint32_t ATTR_VOLUME_NAME = 0x60;      // $VOLUME_NAME
constexpr uint32_t ATTR_VOLUME_INFO = 0x70;      // $VOLUME_INFORMATION
constexpr uint32_t ATTR_DATA = 0x80;             // $DATA
constexpr uint32_t ATTR_INDEX_ROOT = 0x90;       // $INDEX_ROOT
constexpr uint32_t ATTR_INDEX_ALLOCATION = 0xA0; // $INDEX_ALLOCATION
constexpr uint32_t ATTR_BITMAP = 0xB0;           // $BITMAP
constexpr uint32_t ATTR_REPARSE_POINT = 0xC0;    // $REPARSE_POINT
constexpr uint32_t ATTR_EA_INFO = 0xD0;          // $EA_INFORMATION
constexpr uint32_t ATTR_EA = 0xE0;                // $EA
constexpr uint32_t ATTR_LOGGED_UTIL_STREAM = 0x100; // $LOGGED_UTILITY_STREAM
constexpr uint32_t ATTR_END = 0xFFFFFFFF;          // End of attributes

// MFT record flags
constexpr uint16_t MFT_RECORD_FLAG_IN_USE = 0x0001;
constexpr uint16_t MFT_RECORD_FLAG_DIR = 0x0002;
constexpr uint16_t MFT_RECORD_FLAG_SYSTEM = 0x0004;
constexpr uint16_t MFT_RECORD_FLAG_VIEW_INDEX = 0x0008;

// File attribute flags (Windows-style)
constexpr uint32_t FILE_ATTR_READONLY = 0x00000001;
constexpr uint32_t FILE_ATTR_HIDDEN = 0x00000002;
constexpr uint32_t FILE_ATTR_SYSTEM = 0x00000004;
constexpr uint32_t FILE_ATTR_DIRECTORY = 0x00000010;
constexpr uint32_t FILE_ATTR_ARCHIVE = 0x00000020;
constexpr uint32_t FILE_ATTR_DEVICE = 0x00000040;
constexpr uint32_t FILE_ATTR_NORMAL = 0x00000080;
constexpr uint32_t FILE_ATTR_TEMPORARY = 0x00000100;
constexpr uint32_t FILE_ATTR_SPARSE_FILE = 0x00000200;
constexpr uint32_t FILE_ATTR_REPARSE_POINT = 0x00000400;
constexpr uint32_t FILE_ATTR_COMPRESSED = 0x00000800;
constexpr uint32_t FILE_ATTR_OFFLINE = 0x00001000;
constexpr uint32_t FILE_ATTR_NOT_INDEXED = 0x00002000;
constexpr uint32_t FILE_ATTR_ENCRYPTED = 0x00004000;

// ============================================================================
// NTFS Boot Sector (BPB)
// ============================================================================
struct __attribute__((packed)) NTFSBootSector {
    // Jump instruction (3 bytes)
    uint8_t bs_jmp[3];
    
    // OEM ID (8 bytes)
    char bs_oem[8];
    
    // BIOS Parameter Block (BPB) - 25 bytes
    uint16_t bpb_bytes_per_sector;
    uint8_t bpb_sectors_per_cluster;
    uint16_t bpb_reserved_sectors;
    uint8_t bpb_always_zero_0[3];
    uint16_t bpb_unused_0;
    uint8_t bpb_media_descriptor;
    uint16_t bpb_always_zero_1;
    uint16_t bpb_sectors_per_track;
    uint16_t bpb_number_of_heads;
    uint32_t bpb_hidden_sectors;
    uint32_t bpb_unused_1;
    uint32_t bpb_unused_2;
    uint64_t bpb_total_sectors;
    
    // Physical drive
    uint8_t bs_physical_drive;
    uint8_t bs_reserved_1;
    uint8_t bs_extended_boot_sig;
    uint8_t bs_reserved_2[4];
    
    // MFT information
    uint64_t bs_mft_lcn;             // Logical cluster number for MFT
    uint64_t bs_mft_mirr_lcn;        // Logical cluster number for MFT mirror
    int8_t bs_clusters_per_mft_record;
    uint8_t bs_reserved_3[3];
    int8_t bs_clusters_per_index_record;
    uint8_t bs_reserved_4[3];
    
    // Volume serial number
    uint64_t bs_volume_serial;
    
    // Checksum
    uint32_t bs_checksum;
    
    // Boot code (426 bytes)
    uint8_t bs_boot_code[426];
    
    // Boot signature
    uint16_t bs_boot_signature;
    
    void init(uint64_t total_sectors, uint8_t sectors_per_cluster, 
              uint64_t mft_lcn, uint64_t mft_mirr_lcn, uint64_t serial);
};

// ============================================================================
// MFT Record Header
// ============================================================================
struct __attribute__((packed)) MFTRecordHeader {
    uint32_t mr_magic;               // "FILE"
    uint16_t mr_usn_offset;          // Update sequence number offset
    uint16_t mr_usn_size;            // Update sequence size (in words)
    uint64_t mr_lsn;                 // Log file sequence number
    uint16_t mr_sequence_number;     // Sequence number
    uint16_t mr_hard_link_count;     // Hard link count
    uint16_t mr_attr_offset;         // Offset to first attribute
    uint16_t mr_flags;               // Flags
    uint32_t mr_used_size;           // Used size of MFT record
    uint32_t mr_alloc_size;          // Allocated size of MFT record
    uint64_t mr_base_record;         // Base record (for extension records)
    uint16_t mr_next_attr_id;        // Next attribute ID
    uint16_t mr_record_number;       // Record number (XP)
    uint16_t mr_usn;                 // Update sequence number
    
    bool isValid() const { return mr_magic == MFT_RECORD_MAGIC; }
    bool isInUse() const { return mr_flags & MFT_RECORD_FLAG_IN_USE; }
    bool isDirectory() const { return mr_flags & MFT_RECORD_FLAG_DIR; }
};

// ============================================================================
// Attribute Record Header
// ============================================================================
struct __attribute__((packed)) AttributeHeader {
    uint32_t a_type;                 // Attribute type
    uint32_t a_length;               // Length of attribute
    uint8_t a_non_resident;          // Non-resident flag
    uint8_t a_name_length;           // Name length
    uint16_t a_name_offset;          // Name offset
    uint16_t a_flags;                // Flags
    uint16_t a_id;                   // Attribute ID
};

// Resident attribute header
struct __attribute__((packed)) ResidentAttributeHeader {
    AttributeHeader header;
    uint32_t ra_value_length;        // Value length
    uint16_t ra_value_offset;        // Value offset
    uint8_t ra_flags;                // Flags
    uint8_t ra_reserved;
};

// Non-resident attribute header
struct __attribute__((packed)) NonResidentAttributeHeader {
    AttributeHeader header;
    uint64_t nra_start_vcn;          // Starting VCN
    uint64_t nra_end_vcn;            // Ending VCN
    uint16_t nra_run_offset;         // Run list offset
    uint16_t nra_compression_unit;   // Compression unit size
    uint32_t nra_padding;
    uint64_t nra_alloc_size;         // Allocated size
    uint64_t nra_data_size;          // Data size
    uint64_t nra_init_size;          // Initialized size
    uint64_t nra_compressed_size;    // Compressed size
};

// ============================================================================
// Standard Information Attribute ($STANDARD_INFORMATION)
// ============================================================================
struct __attribute__((packed)) StandardInfo {
    uint64_t si_creation_time;       // File creation time
    uint64_t si_modification_time;   // File modification time
    uint64_t si_mft_change_time;       // MFT change time
    uint64_t si_access_time;         // Last access time
    uint32_t si_file_attributes;     // File attributes
    uint32_t si_max_version;         // Maximum versions
    uint32_t si_version_number;      // Version number
    uint32_t si_class_id;            // Class ID
    uint32_t si_owner_id;            // Owner ID
    uint32_t si_security_id;           // Security ID
    uint64_t si_quota_charged;         // Quota charged
    uint64_t si_usn;                 // Update sequence number
};

// ============================================================================
// File Name Attribute ($FILE_NAME)
// ============================================================================
struct __attribute__((packed)) FileNameAttr {
    uint64_t fn_parent_directory;    // Parent directory MFT reference
    uint64_t fn_creation_time;
    uint64_t fn_modification_time;
    uint64_t fn_mft_change_time;
    uint64_t fn_access_time;
    uint64_t fn_alloc_size;
    uint64_t fn_data_size;
    uint32_t fn_file_attributes;
    uint32_t fn_ea_size;             // Extended attributes size
    uint8_t fn_name_length;
    uint8_t fn_name_type;            // Namespace (DOS, Win32, etc.)
    char fn_name[256];               // File name (variable length)
};

// ============================================================================
// Volume Information Attribute ($VOLUME_INFORMATION)
// ============================================================================
struct __attribute__((packed)) VolumeInfo {
    uint64_t vi_reserved_1;
    uint8_t vi_major_ver;
    uint8_t vi_minor_ver;
    uint16_t vi_flags;
    uint32_t vi_reserved_2;
};

// ============================================================================
// Index Entry (for directories)
// ============================================================================
struct __attribute__((packed)) IndexEntry {
    uint64_t ie_mft_ref;             // MFT reference
    uint16_t ie_size;                // Entry size
    uint16_t ie_key_size;            // Key size
    uint16_t ie_flags;               // Flags
    uint16_t ie_reserved;
    char ie_key[1];                  // Key (variable)
    // Padding to align to 8 bytes
    // Stream (if present)
};

// Index entry flags
constexpr uint16_t INDEX_ENTRY_NODE = 0x0001;
constexpr uint16_t INDEX_ENTRY_END = 0x0002;

// ============================================================================
// NTFS Layout Calculator
// ============================================================================
class NTFSLayout {
public:
    uint64_t total_size;             // Total volume size in bytes
    uint32_t bytes_per_sector;       // Bytes per sector (usually 512)
    uint32_t sectors_per_cluster;    // Sectors per cluster
    uint32_t bytes_per_cluster;      // Bytes per cluster
    uint64_t total_sectors;          // Total sectors
    uint64_t total_clusters;         // Total clusters
    uint64_t mft_lcn;                // MFT logical cluster number
    uint64_t mft_mirr_lcn;           // MFT mirror logical cluster number
    uint32_t mft_record_size;        // MFT record size (usually 1024)
    uint32_t index_record_size;      // Index record size (usually 4096)
    int8_t clusters_per_mft_record;  // Clusters per MFT record (negative = 2^abs)
    int8_t clusters_per_index_record; // Clusters per index record
    uint64_t serial_number;            // Volume serial number
    
    void calculate(uint64_t volume_size_bytes, uint32_t sector_size = 512);
    
    uint64_t clusterToSector(uint64_t cluster) const {
        return cluster * sectors_per_cluster;
    }
    
    uint64_t sectorToCluster(uint64_t sector) const {
        return sector / sectors_per_cluster;
    }
    
    uint64_t mftRecordToCluster(uint64_t record) const {
        return mft_lcn + (record * mft_record_size / bytes_per_cluster);
    }
    
    bool validate() const;
};

// ============================================================================
// NTFS Operations
// ============================================================================

// Initialize boot sector
void initBootSector(NTFSBootSector& bs, const NTFSLayout& layout);

// Generate serial number
uint64_t generateNTFSSerial();

// Calculate NTFS boot sector checksum (not actually used, but for completeness)
uint32_t calculateBootChecksum(const uint8_t* data, size_t length);

// ============================================================================
// MFT Operations
// ============================================================================

// Initialize MFT record
void initMFTRecord(MFTRecordHeader& record, uint64_t record_num, bool is_dir = false);

// Add attribute to MFT record
Result addAttribute(void* record, uint32_t attr_type, const void* data, 
                     uint32_t data_len);

// Fix up update sequence
void fixupUpdateSequence(void* record);

// ============================================================================
// NTFS Format Operations
// ============================================================================

Result formatNTFS(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                   uint64_t size_bytes, const std::string& label);

Result createNTFSBootSector(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                              const NTFSLayout& layout);

Result createMFT(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                  const NTFSLayout& layout);

Result createSystemFiles(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                          const NTFSLayout& layout);

Result createBitmap(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                      const NTFSLayout& layout);

Result createLogFile(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                      const NTFSLayout& layout);

Result createRootDirectory(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                            const NTFSLayout& layout, const std::string& label);

Result createUpCase(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                      const NTFSLayout& layout);

// ============================================================================
// NTFS Check Operations
// ============================================================================

Result checkNTFS(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                  bool repair = false, std::vector<std::string>* errors = nullptr);

Result checkBootSector(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                        bool repair, std::vector<std::string>* errors);

Result checkMFT(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                 const NTFSLayout& layout, bool repair,
                 std::vector<std::string>* errors);

Result checkBitmap(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                    const NTFSLayout& layout, bool repair,
                    std::vector<std::string>* errors);

Result checkLogFile(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                     const NTFSLayout& layout, bool repair,
                     std::vector<std::string>* errors);

// ============================================================================
// NTFS Resize Operations
// ============================================================================

Result resizeNTFS(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                   uint64_t new_size_bytes);

Result extendMFT(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                  const NTFSLayout& old_layout, const NTFSLayout& new_layout);

Result updateBootSectorForResize(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                                   const NTFSLayout& layout);

} // namespace ntfs
} // namespace opm