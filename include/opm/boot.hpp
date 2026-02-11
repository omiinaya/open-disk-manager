#pragma once

#include "types.hpp"
#include "disk_io.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace opm {

// ============================================================================
// Boot Configuration
// ============================================================================

struct BootConfig {
    std::string distribution_name = "OPM";
    std::string kernel_version;
    std::string initramfs_version;
    std::string bootloader = "syslinux"; // or "grub2"
    bool include_firmware = true;
    bool include_tools = true;
    bool persistent_mode = false;  // Enable persistence
    uint64_t persistence_size = 0;   // Persistence partition size in bytes
};

// ============================================================================
// Live USB Operations
// ============================================================================

struct LiveUSBOptions {
    std::string iso_path;           // Path to source ISO
    std::string label = "OPM-LIVE";
    bool verify_after_write = true;
    bool compress = false;
    std::function<void(uint64_t, uint64_t)> progress_callback;
};

// Create Live USB from ISO
Result createLiveUSB(std::shared_ptr<DiskIO> target_device,
                     const LiveUSBOptions& options);

// Create Live USB with custom configuration
Result createLiveUSBWithConfig(std::shared_ptr<DiskIO> target_device,
                               const BootConfig& config,
                               const LiveUSBOptions& options);

// Verify Live USB integrity
Result verifyLiveUSB(std::shared_ptr<DiskIO> device);

// Update Live USB
Result updateLiveUSB(std::shared_ptr<DiskIO> device,
                     const std::string& new_iso_path);

// Make partition persistent
Result setupPersistence(std::shared_ptr<DiskIO> device,
                        uint64_t persistence_size_bytes);

// ============================================================================
// Boot Repair Operations
// ============================================================================

enum class BootRepairTarget {
    MBR,
    GPT,
    Bootloader,
    Filesystem
};

struct BootRepairOptions {
    BootRepairTarget target = BootRepairTarget::MBR;
    bool reinstall_bootloader = false;
    bool fix_partition_table = false;
    bool check_filesystem = true;
    bool backup_before_repair = true;
};

// Repair boot sector
Result repairBootSector(std::shared_ptr<DiskIO> device,
                        const BootRepairOptions& options);

// Reinstall bootloader
Result reinstallBootloader(std::shared_ptr<DiskIO> device,
                           const std::string& bootloader_type);

// Fix partition table
Result fixPartitionTable(std::shared_ptr<DiskIO> device);

// Check and repair filesystem boot sector
Result checkAndRepairFilesystem(std::shared_ptr<DiskIO> device,
                                uint64_t partition_start);

// Full boot repair (diagnose and fix common issues)
Result fullBootRepair(std::shared_ptr<DiskIO> device,
                      std::vector<std::string>& repair_log);

// ============================================================================
// Bootloader Operations
// ============================================================================

struct BootloaderOptions {
    std::string bootloader = "syslinux"; // syslinux, grub2, systemd-boot
    std::string target_arch = "x86_64";
    bool uefi_support = true;
    bool legacy_bios_support = true;
    std::string theme;
};

// Install bootloader
Result installBootloader(std::shared_ptr<DiskIO> device,
                         const BootloaderOptions& options);

// Update bootloader
Result updateBootloader(std::shared_ptr<DiskIO> device,
                        const BootloaderOptions& options);

// Configure bootloader
Result configureBootloader(std::shared_ptr<DiskIO> device,
                           const BootConfig& config,
                           const BootloaderOptions& options);

// ============================================================================
// ISO Operations
// ============================================================================

// Mount ISO file
Result mountISO(const std::string& iso_path, const std::string& mount_point);

// Unmount ISO
Result unmountISO(const std::string& mount_point);

// Extract ISO contents
Result extractISO(const std::string& iso_path,
                  const std::string& output_directory,
                  std::function<void(uint64_t, uint64_t)> progress = nullptr);

// Create bootable ISO from directory
Result createBootableISO(const std::string& source_directory,
                         const std::string& output_iso_path,
                         const BootConfig& config);

// ============================================================================
// USB Detection and Info
// ============================================================================

struct USBDeviceInfo {
    std::string device_path;
    std::string vendor;
    std::string model;
    uint64_t size = 0;
    uint32_t sector_size = 0;
    bool is_removable = false;
    bool is_usb = false;
    std::string filesystem;
    std::string label;
};

// Detect USB devices
std::vector<USBDeviceInfo> detectUSBDevices();

// Check if device is a USB device
bool isUSBDevice(const std::string& device_path);

// Get USB device info
Result getUSBDeviceInfo(const std::string& device_path, USBDeviceInfo& info);

// Check if device is bootable
bool isBootableDevice(std::shared_ptr<DiskIO> device);

// Get boot mode (UEFI/BIOS)
enum class BootMode {
    Unknown,
    BIOS,
    UEFI
};

BootMode detectBootMode();

} // namespace opm
