#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <algorithm>

namespace opm {

// Basic types
using byte_t = uint8_t;
using sector_t = uint64_t;
using size_t = uint64_t;

// Partition type identifiers
enum class PartitionType : uint8_t {
    Unknown = 0x00,
    FAT12 = 0x01,
    FAT16 = 0x04,
    ExtendedCHS = 0x05,
    FAT16B = 0x06,
    NTFS = 0x07,
    FAT32CHS = 0x0B,
    FAT32LBA = 0x0C,
    FAT16BLBA = 0x0E,
    ExtendedLBA = 0x0F,
    LinuxSwap = 0x82,
    Linux = 0x83,
    LinuxExtended = 0x85,
    LinuxLVM = 0x8E,
    LinuxRAID = 0xFD,
    EFI = 0xEF,
    // GPT protective
    GPTProtective = 0xEE,
};

// File system types
enum class FileSystemType {
    Unknown,
    FAT12,
    FAT16,
    FAT32,
    exFAT,
    NTFS,
    EXT2,
    EXT3,
    EXT4,
    ReFS,
    HFS,
    HFSPlus,
    APFS,
    Swap,
    LVM2,
    RAID,
    EFI,
    Reserved,
};

// GPT partition type GUIDs (partial list)
namespace gpt_type {
    constexpr const char* EFI_SYSTEM = "C12A7328-F81F-11D2-BA4B-00A0C93EC93B";
    constexpr const char* MICROSOFT_RESERVED = "E3C9E316-0B5C-4DB8-817D-F92DF00215AE";
    constexpr const char* MICROSOFT_BASIC_DATA = "EBD0A0A2-B9E5-4433-87C0-68B6B72699C7";
    constexpr const char* LINUX_FILESYSTEM = "0FC63DAF-8483-4772-8E79-3D69D8477DE4";
    constexpr const char* LINUX_SWAP = "0657FD6D-A4AB-43C4-84E5-0933C84B4F4F";
    constexpr const char* LINUX_LVM = "E6D6D379-F507-44C2-A23C-238F2A3DF928";
    constexpr const char* LINUX_RAID = "A19D880F-05FC-4D3B-A006-743F0F84911E";
}

// Disk geometry
struct DiskGeometry {
    uint64_t total_sectors;
    uint32_t bytes_per_sector;
    uint32_t sectors_per_track;
    uint32_t heads;
    uint32_t cylinders;
    bool lba_supported;
    bool lba48_supported;
    
    uint64_t total_bytes() const {
        return total_sectors * bytes_per_sector;
    }
};

// Device information
struct DeviceInfo {
    std::string path;           // Device path (e.g., /dev/sda)
    std::string model;          // Device model name
    std::string serial;         // Serial number
    uint64_t size;              // Total size in bytes
    DiskGeometry geometry;      // Disk geometry
    bool removable;             // Is removable media
    bool readonly;              // Is read-only
    bool ssd;                   // Is SSD (if detectable)
    std::string transport;      // SATA, NVMe, USB, etc.
};

// Alignment constants
constexpr size_t SECTOR_SIZE = 512;
constexpr size_t GPT_SECTOR_SIZE = 512;
constexpr size_t ALIGNMENT_1MB = 2048;      // 1MB / 512B
constexpr size_t ALIGNMENT_4K = 8;          // 4K / 512B
constexpr size_t ALIGNMENT_1MB_4K = 256;    // 1MB / 4K

// Device types for enumeration
enum class DeviceType {
    Disk,
    Partition,
    Loop,
    Unknown
};

// Result type for operations
enum class ResultCode {
    Success = 0,
    Error = 1,
    PermissionDenied = 2,
    DeviceBusy = 3,
    InvalidArgument = 4,
    NotSupported = 5,
    InsufficientSpace = 6,
    DataCorrupted = 7,
    OperationCancelled = 8,
};

struct Result {
    ResultCode code;
    std::string message;
    
    bool success() const { return code == ResultCode::Success; }
    bool failed() const { return code != ResultCode::Success; }
    
    static Result ok() { return {ResultCode::Success, ""}; }
    static Result error(const std::string& msg) { 
        return {ResultCode::Error, msg}; 
    }
};

// Progress callback
using ProgressCallback = std::function<void(uint64_t current, uint64_t total, const std::string& status)>;

} // namespace opm
