#pragma once

#include "types.hpp"
#include <string>
#include <memory>
#include <vector>

namespace opm {

// Low-level disk I/O operations
class DiskIO {
public:
    // Factory methods
    static std::shared_ptr<DiskIO> open(const std::string& device_path, bool readwrite = false);
    static std::shared_ptr<DiskIO> openReadOnly(const std::string& device_path);
    static std::shared_ptr<DiskIO> openReadWrite(const std::string& device_path);
    
    // Destructor
    virtual ~DiskIO();
    
    // Device information
    std::string devicePath() const { return device_path_; }
    std::string deviceName() const { return device_name_; }
    uint64_t size() const { return device_size_; }
    bool isReadOnly() const { return readonly_; }
    bool isOpen() const { return fd_ >= 0; }
    uint32_t sectorSize() const { return sector_size_; }
    uint64_t sectorCount() const { return device_size_ / sector_size_; }
    
    // Geometry (if available)
    bool hasGeometry() const { return has_geometry_; }
    DiskGeometry geometry() const { return geometry_; }
    
    // Basic I/O
    Result read(void* buffer, uint64_t offset, size_t size);
    Result write(const void* buffer, uint64_t offset, size_t size);
    
    // Sector-aligned I/O
    Result readSectors(void* buffer, uint64_t sector, uint32_t count);
    Result writeSectors(const void* buffer, uint64_t sector, uint32_t count);
    
    // Read/write single sector
    Result readSector(void* buffer, uint64_t sector);
    Result writeSector(const void* buffer, uint64_t sector);
    
    // Flush changes
    Result flush();
    Result sync();
    
    // Device control
    Result lock();
    Result unlock();
    bool isLocked() const { return locked_; }
    
    // Scan device
    Result rescanPartitions();
    
    // Close device
    void close();
    
    // Detect file system on partition
    FileSystemType detectFilesystem(uint64_t start_sector);
    
    // SMART data (if available)
    bool supportsSMART() const;
    Result readSMART(void* data);
    
    // TRIM support
    bool supportsTRIM() const;
    Result trim(uint64_t start, uint64_t count);
    
    // Get device info
    DeviceInfo getDeviceInfo();
    
    // Platform-specific
    #ifdef __linux__
    int fileDescriptor() const { return fd_; }
    #endif
    
protected:
    DiskIO();
    
private:
    std::string device_path_;
    std::string device_name_;
    int fd_ = -1;
    bool readonly_ = true;
    bool locked_ = false;
    uint64_t device_size_ = 0;
    uint32_t sector_size_ = 512;
    bool has_geometry_ = false;
    DiskGeometry geometry_;
    
    // Platform-specific helpers
    Result openDevice();
    Result detectDeviceSize();
    Result detectSectorSize();
    Result detectGeometry();
    
    #ifdef __linux__
    int device_fd_ = -1;
    #endif
    
    #ifdef _WIN32
    void* handle_ = nullptr;
    #endif
    
    #ifdef __APPLE__
    int device_fd_ = -1;
    #endif
};

// Device enumeration
class DeviceEnumerator {
public:
    static std::vector<DeviceInfo> enumerateDevices();
    static std::vector<DeviceInfo> enumerateRemovableDevices();
    static std::vector<DeviceInfo> enumerateFixedDevices();
    
    static bool isValidDevice(const std::string& path);
    static DeviceInfo getDeviceInfo(const std::string& path);
    
private:
    #ifdef __linux__
    static std::vector<DeviceInfo> enumerateLinuxDevices();
    #endif
    
    #ifdef _WIN32
    static std::vector<DeviceInfo> enumerateWindowsDevices();
    #endif
    
    #ifdef __APPLE__
    static std::vector<DeviceInfo> enumerateMacDevices();
    #endif
};

} // namespace opm
