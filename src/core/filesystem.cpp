#include "opm/filesystem.hpp"
#include "opm/disk_io.hpp"
#include "opm/utils.hpp"
#include <cstring>
#include <algorithm>

namespace opm {

std::string getFilesystemName(FileSystemType type) {
    switch (type) {
        case FileSystemType::FAT12: return "FAT12";
        case FileSystemType::FAT16: return "FAT16";
        case FileSystemType::FAT32: return "FAT32";
        case FileSystemType::exFAT: return "exFAT";
        case FileSystemType::NTFS: return "NTFS";
        case FileSystemType::EXT2: return "ext2";
        case FileSystemType::EXT3: return "ext3";
        case FileSystemType::EXT4: return "ext4";
        case FileSystemType::HFS: return "HFS";
        case FileSystemType::HFSPlus: return "HFS+";
        case FileSystemType::APFS: return "APFS";
        case FileSystemType::Swap: return "Linux swap";
        case FileSystemType::LVM2: return "LVM2";
        case FileSystemType::RAID: return "Linux RAID";
        case FileSystemType::EFI: return "EFI System";
        default: return "Unknown";
    }
}

bool isFilesystemSupported(FileSystemType type) {
    switch (type) {
        case FileSystemType::FAT32:
        case FileSystemType::NTFS:
        case FileSystemType::EXT4:
            return true;  // Supported for format
        default:
            return false;  // Read-only or not supported
    }
}

// Placeholder implementations for Phase 3.1
// These will be expanded in subsequent sub-phases

class FAT32FileSystem : public FileSystem {
public:
    FileSystemType type() const override { return FileSystemType::FAT32; }
    std::string name() const override { return "FAT32"; }
    
    Result create(std::shared_ptr<DiskIO> disk, 
                  uint64_t start_sector,
                  uint64_t size_bytes,
                  const std::string& label = "",
                  uint32_t cluster_size = 0) override {
        // Phase 3.2: Full FAT32 implementation
        (void)disk;
        (void)start_sector;
        (void)size_bytes;
        (void)label;
        (void)cluster_size;
        return Result::error("FAT32 create not yet implemented");
    }
    
    Result check(std::shared_ptr<DiskIO> disk,
                uint64_t start_sector,
                bool repair = false,
                std::vector<std::string>* errors = nullptr) override {
        // Phase 3.2: Full FAT32 check implementation
        (void)disk;
        (void)start_sector;
        (void)repair;
        (void)errors;
        return Result::error("FAT32 check not yet implemented");
    }
    
    Result resize(std::shared_ptr<DiskIO> disk,
                   uint64_t start_sector,
                   uint64_t new_size_bytes) override {
        // Phase 3.2: FAT32 resize implementation
        (void)disk;
        (void)start_sector;
        (void)new_size_bytes;
        return Result::error("FAT32 resize not yet implemented");
    }
    
    Result getInfo(std::shared_ptr<DiskIO> disk,
                    uint64_t start_sector,
                    FSInfo& info) override {
        // Phase 3.1: Basic FAT32 info reading
        uint8_t boot_sector[512];
        if (disk->readSector(boot_sector, start_sector).failed()) {
            return Result::error("Failed to read FAT32 boot sector");
        }
        
        info.type = FileSystemType::FAT32;
        info.label = "";
        
        // Read volume label from boot sector (offset 71, 11 bytes)
        char label[12];
        std::memcpy(label, &boot_sector[71], 11);
        label[11] = '\0';
        // Trim trailing spaces
        info.label = std::string(label);
        size_t end = info.label.find_last_not_of(' ');
        if (end != std::string::npos) {
            info.label = info.label.substr(0, end + 1);
        } else {
            info.label = "";
        }
        
        // Read bytes per sector
        uint16_t bytes_per_sector = boot_sector[11] | (boot_sector[12] << 8);
        info.block_size = bytes_per_sector;
        
        // Read sectors per cluster
        uint8_t sectors_per_cluster = boot_sector[13];
        info.cluster_size = bytes_per_sector * sectors_per_cluster;
        
        // Read total sectors
        uint32_t total_sectors = 0;
        uint16_t total_sectors_16 = boot_sector[19] | (boot_sector[20] << 8);
        uint32_t total_sectors_32 = boot_sector[32] | (boot_sector[33] << 8) | 
                                     (boot_sector[34] << 16) | (boot_sector[35] << 24);
        if (total_sectors_16 != 0) {
            total_sectors = total_sectors_16;
        } else {
            total_sectors = total_sectors_32;
        }
        
        info.total_size = static_cast<uint64_t>(total_sectors) * bytes_per_sector;
        
        // Read free space from FSInfo sector
        uint16_t fs_info_sector = boot_sector[48] | (boot_sector[49] << 8);
        if (fs_info_sector != 0) {
            uint8_t fs_info[512];
            if (disk->readSector(fs_info, start_sector + fs_info_sector).success()) {
                // Check FSInfo signature
                if (fs_info[0] == 'R' && fs_info[1] == 'R' && 
                    fs_info[2] == 'A' && fs_info[3] == 'r') {
                    uint32_t free_clusters = fs_info[488] | (fs_info[489] << 8) |
                                          (fs_info[490] << 16) | (fs_info[491] << 24);
                    info.free_size = static_cast<uint64_t>(free_clusters) * 
                                    info.cluster_size;
                    info.used_size = info.total_size - info.free_size;
                }
            }
        }
        
        return Result::ok();
    }
};

class EXT4FileSystem : public FileSystem {
public:
    FileSystemType type() const override { return FileSystemType::EXT4; }
    std::string name() const override { return "ext4"; }
    
    Result create(std::shared_ptr<DiskIO> disk, 
                  uint64_t start_sector,
                  uint64_t size_bytes,
                  const std::string& label = "",
                  uint32_t cluster_size = 0) override {
        // Phase 3.3: Full ext4 implementation
        (void)disk;
        (void)start_sector;
        (void)size_bytes;
        (void)label;
        (void)cluster_size;
        return Result::error("ext4 create not yet implemented");
    }
    
    Result check(std::shared_ptr<DiskIO> disk,
                uint64_t start_sector,
                bool repair = false,
                std::vector<std::string>* errors = nullptr) override {
        // Phase 3.3: Full ext4 check implementation
        (void)disk;
        (void)start_sector;
        (void)repair;
        (void)errors;
        return Result::error("ext4 check not yet implemented");
    }
    
    Result resize(std::shared_ptr<DiskIO> disk,
                   uint64_t start_sector,
                   uint64_t new_size_bytes) override {
        // Phase 3.3: ext4 resize implementation
        (void)disk;
        (void)start_sector;
        (void)new_size_bytes;
        return Result::error("ext4 resize not yet implemented");
    }
    
    Result getInfo(std::shared_ptr<DiskIO> disk,
                    uint64_t start_sector,
                    FSInfo& info) override {
        // Phase 3.1: Basic ext4 info reading
        uint8_t superblock[1024];  // ext superblock is 1024 bytes
        if (disk->read(superblock, start_sector * 512 + 1024, 1024).failed()) {
            return Result::error("Failed to read ext4 superblock");
        }
        
        info.type = FileSystemType::EXT4;
        
        // Read block size
        uint32_t log_block_size = superblock[24] | (superblock[25] << 8) |
                                 (superblock[26] << 16) | (superblock[27] << 24);
        info.block_size = 1024 << log_block_size;
        
        // Read total blocks
        uint32_t blocks_count_lo = superblock[4] | (superblock[5] << 8) |
                                  (superblock[6] << 16) | (superblock[7] << 24);
        uint32_t blocks_count_hi = superblock[216] | (superblock[217] << 8) |
                                  (superblock[218] << 16) | (superblock[219] << 24);
        uint64_t blocks_count = (static_cast<uint64_t>(blocks_count_hi) << 32) | blocks_count_lo;
        
        info.total_size = blocks_count * info.block_size;
        
        // Read free blocks
        uint32_t free_blocks_lo = superblock[12] | (superblock[13] << 8) |
                                   (superblock[14] << 16) | (superblock[15] << 24);
        uint32_t free_blocks_hi = superblock[232] | (superblock[233] << 8) |
                                   (superblock[234] << 16) | (superblock[235] << 24);
        uint64_t free_blocks = (static_cast<uint64_t>(free_blocks_hi) << 32) | free_blocks_lo;
        
        info.free_size = free_blocks * info.block_size;
        info.used_size = info.total_size - info.free_size;
        
        // Read label
        info.label = "";
        for (int i = 0; i < 16 && superblock[120 + i] != '\0'; i++) {
            info.label += static_cast<char>(superblock[120 + i]);
        }
        
        // Read UUID
        info.uuid = utils::guidToString(&superblock[104]);
        
        // Read state (clean/dirty)
        uint16_t state = superblock[58] | (superblock[59] << 8);
        info.dirty = (state != 1);  // 1 = clean
        
        return Result::ok();
    }
};

class NTFSFileSystem : public FileSystem {
public:
    FileSystemType type() const override { return FileSystemType::NTFS; }
    std::string name() const override { return "NTFS"; }
    
    Result create(std::shared_ptr<DiskIO> disk, 
                  uint64_t start_sector,
                  uint64_t size_bytes,
                  const std::string& label = "",
                  uint32_t cluster_size = 0) override {
        // Phase 3.4: Full NTFS implementation
        (void)disk;
        (void)start_sector;
        (void)size_bytes;
        (void)label;
        (void)cluster_size;
        return Result::error("NTFS create not yet implemented - Phase 3.4");
    }
    
    Result check(std::shared_ptr<DiskIO> disk,
                uint64_t start_sector,
                bool repair = false,
                std::vector<std::string>* errors = nullptr) override {
        // Phase 3.4: Full NTFS check implementation
        (void)disk;
        (void)start_sector;
        (void)repair;
        (void)errors;
        return Result::error("NTFS check not yet implemented - Phase 3.4");
    }
    
    Result resize(std::shared_ptr<DiskIO> disk,
                   uint64_t start_sector,
                   uint64_t new_size_bytes) override {
        // Phase 3.4: NTFS resize implementation
        (void)disk;
        (void)start_sector;
        (void)new_size_bytes;
        return Result::error("NTFS resize not yet implemented - Phase 3.4");
    }
    
    Result getInfo(std::shared_ptr<DiskIO> disk,
                    uint64_t start_sector,
                    FSInfo& info) override {
        // Phase 3.1: Basic NTFS info reading
        uint8_t boot_sector[512];
        if (disk->readSector(boot_sector, start_sector).failed()) {
            return Result::error("Failed to read NTFS boot sector");
        }
        
        info.type = FileSystemType::NTFS;
        info.label = "";  // NTFS label is in $Volume, not boot sector
        
        // Read bytes per sector
        uint16_t bytes_per_sector = boot_sector[11] | (boot_sector[12] << 8);
        info.block_size = bytes_per_sector;
        
        // Read sectors per cluster
        uint8_t sectors_per_cluster = boot_sector[13];
        info.cluster_size = bytes_per_sector * sectors_per_cluster;
        
        // Read total sectors (little-endian, unaligned)
        uint64_t total_sectors = 
            static_cast<uint64_t>(boot_sector[40]) |
            (static_cast<uint64_t>(boot_sector[41]) << 8) |
            (static_cast<uint64_t>(boot_sector[42]) << 16) |
            (static_cast<uint64_t>(boot_sector[43]) << 24) |
            (static_cast<uint64_t>(boot_sector[44]) << 32) |
            (static_cast<uint64_t>(boot_sector[45]) << 40) |
            (static_cast<uint64_t>(boot_sector[46]) << 48) |
            (static_cast<uint64_t>(boot_sector[47]) << 56);
        info.total_size = total_sectors * bytes_per_sector;
        
        // Read volume serial number (little-endian)
        uint64_t serial = 
            static_cast<uint64_t>(boot_sector[72]) |
            (static_cast<uint64_t>(boot_sector[73]) << 8) |
            (static_cast<uint64_t>(boot_sector[74]) << 16) |
            (static_cast<uint64_t>(boot_sector[75]) << 24) |
            (static_cast<uint64_t>(boot_sector[76]) << 32) |
            (static_cast<uint64_t>(boot_sector[77]) << 40) |
            (static_cast<uint64_t>(boot_sector[78]) << 48) |
            (static_cast<uint64_t>(boot_sector[79]) << 56);
        uint8_t serial_bytes[8];
        serial_bytes[0] = (serial >> 0) & 0xFF;
        serial_bytes[1] = (serial >> 8) & 0xFF;
        serial_bytes[2] = (serial >> 16) & 0xFF;
        serial_bytes[3] = (serial >> 24) & 0xFF;
        serial_bytes[4] = (serial >> 32) & 0xFF;
        serial_bytes[5] = (serial >> 40) & 0xFF;
        serial_bytes[6] = (serial >> 48) & 0xFF;
        serial_bytes[7] = (serial >> 56) & 0xFF;
        info.uuid = utils::bytesToHex(serial_bytes, 8);
        
        return Result::ok();
    }
};

// Factory implementation
std::unique_ptr<FileSystem> createFileSystem(FileSystemType type) {
    switch (type) {
        case FileSystemType::FAT32:
        case FileSystemType::FAT16:
        case FileSystemType::FAT12:
            return std::make_unique<FAT32FileSystem>();
        case FileSystemType::EXT2:
        case FileSystemType::EXT3:
        case FileSystemType::EXT4:
            return std::make_unique<EXT4FileSystem>();
        case FileSystemType::NTFS:
            return std::make_unique<NTFSFileSystem>();
        default:
            return nullptr;
    }
}

} // namespace opm
