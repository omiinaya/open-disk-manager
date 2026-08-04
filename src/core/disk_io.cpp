#include "opm/disk_io.hpp"
#include "opm/exceptions.hpp"
#include "opm/utils.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <linux/fs.h>
#include <linux/hdreg.h>
#include <cstring>
#include <cerrno>

namespace opm {

DiskIO::DiskIO() = default;

DiskIO::~DiskIO() {
    close();
}

std::shared_ptr<DiskIO> DiskIO::open(const std::string& device_path, bool readwrite) {
    auto disk = std::shared_ptr<DiskIO>(new DiskIO());
    disk->device_path_ = device_path;
    disk->readonly_ = !readwrite;
    
    if (disk->openDevice().failed()) {
        return nullptr;
    }
    
    return disk;
}

std::shared_ptr<DiskIO> DiskIO::openReadOnly(const std::string& device_path) {
    return open(device_path, false);
}

std::shared_ptr<DiskIO> DiskIO::openReadWrite(const std::string& device_path) {
    return open(device_path, true);
}

Result DiskIO::openDevice() {
    #ifdef __linux__
    int flags = readonly_ ? O_RDONLY : O_RDWR;
    bool direct = true;
    fd_ = ::open(device_path_.c_str(), flags | O_DIRECT);
    
    if (fd_ < 0) {
        direct = false;
        fd_ = ::open(device_path_.c_str(), flags); // Try without O_DIRECT
    }
    
    if (fd_ < 0) {
        switch (errno) {
            case EACCES:
                return Result::error("Permission denied: " + device_path_);
            case ENOENT:
                return Result::error("Device not found: " + device_path_);
            case EBUSY:
                return Result::error("Device busy: " + device_path_);
            default:
                return Result::error("Failed to open device: " + std::string(strerror(errno)));
        }
    }

    // Some filesystems (overlayfs, certain network mounts) accept an O_DIRECT
    // open but fail every read/write with EINVAL. Probe with an aligned read
    // and fall back to a buffered fd when that happens.
    if (direct) {
        static std::vector<uint8_t> aligned_pool(4096 + 512, 0);
        uint8_t* aligned = aligned_pool.data() +
            (512 - (reinterpret_cast<uintptr_t>(aligned_pool.data()) % 512));
        ssize_t n = ::pread(fd_, aligned, 512, 0);
        if (n < 0 && errno == EINVAL) {
            ::close(fd_);
            direct = false;
            fd_ = ::open(device_path_.c_str(), flags);  // no O_DIRECT
            if (fd_ < 0) {
                return Result::error("Failed to open device without O_DIRECT: " +
                                     std::string(strerror(errno)));
            }
        }
    }
    
    // Detect device size
    if (detectDeviceSize().failed()) {
        ::close(fd_);
        fd_ = -1;
        return Result::error("Failed to detect device size");
    }
    
    // Detect sector size
    detectSectorSize();
    
    // Detect geometry
    detectGeometry();
    
    #endif
    
    return Result::ok();
}

Result DiskIO::detectDeviceSize() {
    #ifdef __linux__
    // Try BLKGETSIZE64 first
    if (ioctl(fd_, BLKGETSIZE64, &device_size_) >= 0) {
        return Result::ok();
    }
    
    // Fallback to BLKGETSIZE
    unsigned long size_sectors;
    if (ioctl(fd_, BLKGETSIZE, &size_sectors) >= 0) {
        device_size_ = static_cast<uint64_t>(size_sectors) * 512;
        return Result::ok();
    }

    // Regular-file fallback (virtual disk images used in tests)
    struct stat st;
    if (fstat(fd_, &st) == 0 && S_ISREG(st.st_mode)) {
        device_size_ = static_cast<uint64_t>(st.st_size);
        return Result::ok();
    }
    
    #endif
    
    return Result::error("Failed to detect device size");
}

Result DiskIO::detectSectorSize() {
    #ifdef __linux__
    int sector_size;
    if (ioctl(fd_, BLKSSZGET, &sector_size) >= 0) {
        sector_size_ = static_cast<uint32_t>(sector_size);
        return Result::ok();
    }
    #endif
    
    // Default to 512
    sector_size_ = 512;
    return Result::ok();
}

Result DiskIO::detectGeometry() {
    #ifdef __linux__
    struct hd_geometry geo;
    if (ioctl(fd_, HDIO_GETGEO, &geo) >= 0) {
        has_geometry_ = true;
        geometry_.heads = geo.heads;
        geometry_.sectors_per_track = geo.sectors;
        geometry_.cylinders = geo.cylinders;
        geometry_.total_sectors = device_size_ / sector_size_;
        geometry_.bytes_per_sector = sector_size_;
        geometry_.lba_supported = true;
        geometry_.lba48_supported = (device_size_ > (2ULL * 1024 * 1024 * 1024 * 1024)); // >2TB
        return Result::ok();
    }
    #endif
    
    has_geometry_ = false;
    return Result::ok();
}

void DiskIO::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

Result DiskIO::read(void* buffer, uint64_t offset, size_t size) {
    if (fd_ < 0) {
        return Result::error("Device not open");
    }
    
    ssize_t result = pread(fd_, buffer, size, static_cast<off_t>(offset));
    if (result < 0) {
        return Result::error("Read failed: " + std::string(strerror(errno)));
    }
    
    if (static_cast<size_t>(result) != size) {
        return Result::error("Short read");
    }
    
    return Result::ok();
}

Result DiskIO::write(const void* buffer, uint64_t offset, size_t size) {
    if (fd_ < 0) {
        return Result::error("Device not open");
    }
    
    if (readonly_) {
        return Result::error("Device is read-only");
    }
    
    ssize_t result = pwrite(fd_, buffer, size, static_cast<off_t>(offset));
    if (result < 0) {
        return Result::error("Write failed: " + std::string(strerror(errno)));
    }
    
    if (static_cast<size_t>(result) != size) {
        return Result::error("Short write");
    }
    
    return Result::ok();
}

Result DiskIO::readSector(void* buffer, uint64_t sector) {
    return read(buffer, sector * sector_size_, sector_size_);
}

Result DiskIO::writeSector(const void* buffer, uint64_t sector) {
    return write(buffer, sector * sector_size_, sector_size_);
}

Result DiskIO::readSectors(void* buffer, uint64_t sector, uint32_t count) {
    return read(buffer, sector * sector_size_, count * sector_size_);
}

Result DiskIO::writeSectors(const void* buffer, uint64_t sector, uint32_t count) {
    return write(buffer, sector * sector_size_, count * sector_size_);
}

Result DiskIO::flush() {
    if (fd_ < 0) {
        return Result::error("Device not open");
    }
    
    #ifdef __linux__
    if (fsync(fd_) < 0) {
        return Result::error("Flush failed: " + std::string(strerror(errno)));
    }
    #endif
    
    return Result::ok();
}

Result DiskIO::sync() {
    #ifdef __linux__
    ::sync();
    #endif
    return Result::ok();
}

FileSystemType DiskIO::detectFilesystem(uint64_t start_sector) {
    uint8_t buffer[512];
    
    if (readSector(buffer, start_sector).failed()) {
        return FileSystemType::Unknown;
    }
    
    // Check for NTFS
    if (memcmp(&buffer[3], "NTFS    ", 8) == 0) {
        return FileSystemType::NTFS;
    }
    
    // Check for FAT32 (BPB structure at offset 82)
    if (memcmp(&buffer[82], "FAT32   ", 8) == 0) {
        return FileSystemType::FAT32;
    }
    
    // Check for FAT16
    if (memcmp(&buffer[54], "FAT16   ", 8) == 0) {
        return FileSystemType::FAT16;
    }
    
    // Check for FAT12
    if (memcmp(&buffer[54], "FAT12   ", 8) == 0) {
        return FileSystemType::FAT12;
    }
    
    // Check for exFAT (name at offset 3, after the 3-byte jump instruction)
    if (memcmp(&buffer[3], "EXFAT   ", 8) == 0) {
        return FileSystemType::exFAT;
    }
    
    // Check for ext2/3/4 - superblock is at byte offset 1024 (sector 2 at offset 0)
    // For partitions, it's at start_sector + 2, offset 0, or absolute offset 1024
    uint8_t ext_buffer[512];
    // Try reading at offset 1024 from partition start
    if (read(ext_buffer, start_sector * sector_size_ + 1024, 512).success()) {
        // Check ext magic number: 0xEF53 at offset 56-57
        if (ext_buffer[56] == 0x53 && ext_buffer[57] == 0xEF) {
            // Check feature compat to determine ext2/3/4
            uint32_t feature_compat = *reinterpret_cast<uint32_t*>(&ext_buffer[96]);
            uint32_t feature_incompat = *reinterpret_cast<uint32_t*>(&ext_buffer[96]);
            
            // ext4: has extents feature (EXT4_FEATURE_INCOMPAT_EXTENTS = 0x40)
            if (feature_incompat & 0x40) {
                return FileSystemType::EXT4;
            }
            
            // ext3: has journal feature but not extents
            if (feature_compat & 0x04) {
                return FileSystemType::EXT3;
            }
            
            // ext2: neither
            return FileSystemType::EXT2;
        }
    }
    
    // Check for swap (signature at offset 4086 = sector 8, offset 54)
    uint8_t swap_buffer[512];
    if (read(swap_buffer, start_sector * 512 + 4086 - 54, 512).success()) {
        if (memcmp(&swap_buffer[54], "SWAP-SPACE", 10) == 0 ||
            memcmp(&swap_buffer[54], "SWAPSPACE2", 10) == 0) {
            return FileSystemType::Swap;
        }
    }
    
    return FileSystemType::Unknown;
}

DeviceInfo DiskIO::getDeviceInfo() {
    DeviceInfo info;
    info.path = device_path_;
    info.size = device_size_;
    info.geometry = geometry_;
    
    // Try to get model
    #ifdef __linux__
    std::string model_path = "/sys/block/" + utils::getDeviceName(device_path_) + "/device/model";
    int model_fd = ::open(model_path.c_str(), O_RDONLY);
    if (model_fd >= 0) {
        char model[256];
        ssize_t n = ::read(model_fd, model, sizeof(model) - 1);
        if (n > 0) {
            model[n] = '\0';
            info.model = utils::trim(std::string(model));
        }
        ::close(model_fd);
    }
    
    // Check if removable
    std::string removable_path = "/sys/block/" + utils::getDeviceName(device_path_) + "/removable";
    int rem_fd = ::open(removable_path.c_str(), O_RDONLY);
    if (rem_fd >= 0) {
        char rem[2];
        if (::read(rem_fd, rem, 1) == 1) {
            info.removable = (rem[0] == '1');
        }
        ::close(rem_fd);
    }
    
    // Check if SSD
    std::string rotational_path = "/sys/block/" + utils::getDeviceName(device_path_) + "/queue/rotational";
    int rot_fd = ::open(rotational_path.c_str(), O_RDONLY);
    if (rot_fd >= 0) {
        char rot[2];
        if (::read(rot_fd, rot, 1) == 1) {
            info.ssd = (rot[0] == '0');
        }
        ::close(rot_fd);
    }
    #endif
    
    return info;
}

// DeviceEnumerator implementation

std::vector<DeviceInfo> DeviceEnumerator::enumerateDevices() {
    std::vector<DeviceInfo> devices;
    
    #ifdef __linux__
    // Enumerate /sys/block
    DIR* dir = opendir("/sys/block");
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_name[0] == '.') continue;
            
            std::string name = entry->d_name;
            // Filter to block devices (sdX, nvmeX, vdX, etc.)
            if (name.find("sd") == 0 || name.find("nvme") == 0 || 
                name.find("vd") == 0 || name.find("hd") == 0) {
                
                std::string path = "/dev/" + name;
                if (isValidDevice(path)) {
                    try {
                        auto disk = DiskIO::openReadOnly(path);
                        if (disk) {
                            devices.push_back(disk->getDeviceInfo());
                        }
                    } catch (...) {
                        // Skip devices we can't open
                    }
                }
            }
        }
        closedir(dir);
    }
    #endif
    
    return devices;
}

std::vector<DeviceInfo> DeviceEnumerator::enumerateRemovableDevices() {
    auto all_devices = enumerateDevices();
    std::vector<DeviceInfo> removable;
    
    for (const auto& dev : all_devices) {
        if (dev.removable) {
            removable.push_back(dev);
        }
    }
    
    return removable;
}

std::vector<DeviceInfo> DeviceEnumerator::enumerateFixedDevices() {
    auto all_devices = enumerateDevices();
    std::vector<DeviceInfo> fixed;
    
    for (const auto& dev : all_devices) {
        if (!dev.removable) {
            fixed.push_back(dev);
        }
    }
    
    return fixed;
}

bool DeviceEnumerator::isValidDevice(const std::string& path) {
    return utils::isValidDevicePath(path);
}

DeviceInfo DeviceEnumerator::getDeviceInfo(const std::string& path) {
    auto disk = DiskIO::openReadOnly(path);
    if (!disk) {
        throw DeviceNotFoundException(path);
    }
    return disk->getDeviceInfo();
}

} // namespace opm
