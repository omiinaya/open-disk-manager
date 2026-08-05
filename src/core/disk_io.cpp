#include "opm/disk_io.hpp"
#include "opm/exceptions.hpp"
#include "opm/utils.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#ifdef __linux__
#include <sys/ioctl.h>
#include <dirent.h>
#include <linux/fs.h>
#include <linux/hdreg.h>
#include <linux/nvme_ioctl.h>
#endif
#include <cstring>
#include <cerrno>

#ifdef _WIN32
// ---- Windows portability shims -------------------------------------------------
// MSVC and older MinGW-w64 toolchains do not provide POSIX pread/pwrite.
// Implement them with ReadFile/WriteFile + OVERLAPPED on the CRT fd.
#ifdef _MSC_VER
#include <BaseTsd.h>
#ifndef SSIZE_T
typedef SSIZE_T ssize_t;
#endif
#else
#include <unistd.h>
#endif
#include <io.h>
#include <windows.h>

static ssize_t opm_pread(int fd, void* buf, size_t count, uint64_t offset) {
    HANDLE h = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
    if (h == INVALID_HANDLE_VALUE) return -1;
    OVERLAPPED ov = {};
    ov.Offset = static_cast<DWORD>(offset & 0xFFFFFFFFu);
    ov.OffsetHigh = static_cast<DWORD>(offset >> 32);
    DWORD n = 0;
    if (!ReadFile(h, buf, static_cast<DWORD>(count), &n, &ov)) return -1;
    return static_cast<ssize_t>(n);
}

static ssize_t opm_pwrite(int fd, const void* buf, size_t count, uint64_t offset) {
    HANDLE h = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
    if (h == INVALID_HANDLE_VALUE) return -1;
    OVERLAPPED ov = {};
    ov.Offset = static_cast<DWORD>(offset & 0xFFFFFFFFu);
    ov.OffsetHigh = static_cast<DWORD>(offset >> 32);
    DWORD n = 0;
    if (!WriteFile(h, buf, static_cast<DWORD>(count), &n, &ov)) return -1;
    return static_cast<ssize_t>(n);
}

#define pread opm_pread
#define pwrite opm_pwrite
#endif  // _WIN32

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
    int flags = readonly_ ? O_RDONLY : O_RDWR;

    // Open buffered first so we can inspect the inode type.
    int plain_fd = ::open(device_path_.c_str(), flags);
    if (plain_fd < 0) {
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

    struct stat st;
    bool block_device = false;
    if (fstat(plain_fd, &st) == 0 && S_ISBLK(st.st_mode)) {
        block_device = true;
    }

#if defined(__linux__) && defined(O_DIRECT)
    if (block_device) {
        // Block device: try O_DIRECT, but verify it actually works. Some
        // filesystems accept the O_DIRECT open yet fail every IO with EINVAL.
        ::close(plain_fd);
        fd_ = ::open(device_path_.c_str(), flags | O_DIRECT);
        if (fd_ < 0) {
            fd_ = ::open(device_path_.c_str(), flags);
        }
        if (fd_ >= 0) {
            static std::vector<uint8_t> aligned_pool(4096 + 512, 0);
            uint8_t* aligned = aligned_pool.data() +
                (512 - (reinterpret_cast<uintptr_t>(aligned_pool.data()) % 512));
            ssize_t n = ::pread(fd_, aligned, 512, 0);
            if (n < 0 && errno == EINVAL) {
                ::close(fd_);
                fd_ = ::open(device_path_.c_str(), flags);  // buffered fallback
            }
        }
    } else
#endif
    {
        // Regular files (image-file testing) and other types always use
        // buffered IO - O_DIRECT here fails on misaligned caller buffers.
        fd_ = plain_fd;
    }

    if (fd_ < 0) {
        return Result::error("Failed to open device: " +
                             std::string(strerror(errno)));
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
#endif

    // Regular-file fallback (virtual disk images used in tests) — works
    // on every platform.
    struct stat st;
    if (fstat(fd_, &st) == 0 && S_ISREG(st.st_mode)) {
        device_size_ = static_cast<uint64_t>(st.st_size);
        return Result::ok();
    }
    
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
    
    ssize_t result = pread(fd_, buffer, size, offset);
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
    
    ssize_t result = pwrite(fd_, buffer, size, offset);
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

// ============================================================================
// TRIM (BLKDISCARD) support
// ============================================================================

bool DiskIO::supportsTRIM() const {
#ifdef __linux__
    if (fd_ < 0) return false;
    // A discard of zero bytes is accepted on TRIM-capable devices; errno is
    // set to EOPNOTSUPP/EINVAL otherwise. Probe conservatively.
    uint64_t range[2] = {0, 0};
    int ret = ioctl(fd_, BLKDISCARD, &range);
    return ret == 0 || (ret < 0 && errno != EOPNOTSUPP && errno != ENOTTY && errno != EINVAL);
#else
    return false;
#endif
}

Result DiskIO::trim(uint64_t start, uint64_t count) {
#ifdef __linux__
    if (fd_ < 0) return Result::error("Device not open");
    uint64_t range[2] = {start * sector_size_, count * sector_size_};
    if (ioctl(fd_, BLKDISCARD, &range) < 0) {
        return Result::error("BLKDISCARD failed: " + std::string(strerror(errno)));
    }
    return Result::ok();
#else
    (void)start; (void)count;
    return Result::error("TRIM is only supported on Linux block devices");
#endif
}

// ============================================================================
// SMART access
// ============================================================================

bool DiskIO::supportsSMART() const {
#ifdef __linux__
    if (fd_ < 0) return false;
    return ioctl(fd_, BLKSSZGET) == 0;  // block device present; SMART probe is best-effort
#else
    return false;
#endif
}

Result DiskIO::readSMART(void* data) {
#ifdef __linux__
    if (fd_ < 0) return Result::error("Device not open");
    if (!data) return Result::error("Invalid SMART buffer");
    // HDIO_GET_IDENTITY is ATA-specific; NVMe uses a different path. We expose
    // what the kernel gives us and report when it is unavailable.
    uint8_t id[512];
    if (ioctl(fd_, HDIO_GET_IDENTITY, id) < 0) {
        return Result::error("SMART identity unavailable for this device: " +
                             std::string(strerror(errno)));
    }
    std::memcpy(data, id, 512);
    return Result::ok();
#else
    (void)data;
    return Result::error("SMART is only supported on Linux block devices");
#endif
}

Result DiskIO::readNvmeSMART(void* data) {
#ifdef __linux__
    if (fd_ < 0) return Result::error("Device not open");
    if (!data) return Result::error("Invalid SMART buffer");
#if defined(__linux__) && __has_include(<linux/nvme_ioctl.h>)
    // NVMe Get Log Page (admin command opcode 0x02), Log Page ID 0x02 =
    // SMART/Health Information. The 512-byte log is the standard structure
    // every NVMe device must support (NVMe 1.0+).
    // Note: nvme_admin_cmd is a typedef-macro of nvme_passthru_cmd whose data
    // fields are addr + data_len (not the older prp1/prp2 layout).
    struct nvme_admin_cmd cmd;
    std::memset(&cmd, 0, sizeof(cmd));
    cmd.opcode = 0x02;          // Get Log Page
    cmd.nsid = 0;               // controller-wide log
    // CDW10: bits 7:0 = LID (0x02 SMART), bits 15:8 = NUMDL (num dwords - 1)
    cmd.cdw10 = 0x02 | ((512 / 4 - 1) << 8);
    cmd.cdw11 = 0;              // no specific lid offset
    cmd.timeout_ms = 3000;
    cmd.addr = reinterpret_cast<uint64_t>(data);
    cmd.data_len = 512;
    if (ioctl(fd_, NVME_IOCTL_ADMIN_CMD, &cmd) < 0) {
        return Result::error("NVMe SMART log unavailable: " +
                             std::string(strerror(errno)));
    }
    return Result::ok();
#else
    (void)data;
    return Result::error("NVMe SMART requires linux/nvme_ioctl.h");
#endif
#else
    (void)data;
    return Result::error("SMART is only supported on Linux block devices");
#endif
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
    
    // Check for swap: v1 "SWAPSPACE2" magic at byte 4088 of the first
    // 4096-byte page (10 bytes, spans into the second page), v0 "SWAP-SPACE"
    // at byte 4086.
    uint8_t swap_page[8192];
    if (read(swap_page, start_sector * sector_size_, 8192).success()) {
        if (memcmp(&swap_page[4088], "SWAPSPACE2", 10) == 0 ||
            memcmp(&swap_page[4086], "SWAP-SPACE", 10) == 0) {
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
