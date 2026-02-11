#pragma once

#include "types.hpp"
#include <string>
#include <vector>

namespace opm {

// ============================================================================
// Platform Detection
// ============================================================================

enum class Platform {
    Linux,
    Windows,
    macOS,
    Unknown
};

enum class Architecture {
    x86_64,
    x86,
    arm64,
    arm,
    Unknown
};

// Get current platform
Platform getCurrentPlatform();
Architecture getCurrentArchitecture();

// Platform-specific checks
bool isLinux();
bool isWindows();
bool isMacOS();
bool isUnixLike();

// ============================================================================
// Path Utilities
// ============================================================================

class PlatformPath {
public:
    // Convert path to native format
    static std::string toNative(const std::string& path);
    
    // Convert path from native format
    static std::string fromNative(const std::string& path);
    
    // Get path separator
    static char separator();
    
    // Join paths
    static std::string join(const std::string& base, const std::string& relative);
    
    // Get absolute path
    static std::string absolute(const std::string& path);
    
    // Get device path prefix (e.g., "/dev/" on Linux, "\\\\.\\" on Windows)
    static std::string devicePrefix();
    
    // Format device path for current platform
    static std::string formatDevicePath(const std::string& device);
};

// ============================================================================
// Privilege Management
// ============================================================================

class PrivilegeManager {
public:
    // Check if running with admin/root privileges
    static bool hasAdminPrivileges();
    
    // Check if running with elevated privileges (Windows UAC)
    static bool isElevated();
    
    // Request elevation (re-launch with admin rights)
    static Result requestElevation(const std::vector<std::string>& args);
    
    // Drop privileges (for security after opening device)
    static Result dropPrivileges();
};

// ============================================================================
// System Information
// ============================================================================

struct SystemInfo {
    std::string os_name;
    std::string os_version;
    std::string kernel_version;
    Architecture architecture;
    uint64_t total_memory;
    uint64_t free_memory;
    uint32_t cpu_count;
    std::string cpu_model;
};

Result getSystemInfo(SystemInfo& info);

// ============================================================================
// Device Enumeration (Cross-Platform)
// ============================================================================

struct DevicePath {
    std::string path;
    std::string name;
    std::string description;
    DeviceType type;
    bool removable;
    uint64_t size;
};

std::vector<DevicePath> enumerateAllDevices();
std::vector<DevicePath> enumeratePhysicalDevices();
std::vector<DevicePath> enumerateRemovableDevices();

// ============================================================================
// Windows-Specific
// ============================================================================

#ifdef _WIN32

struct WindowsDeviceInfo {
    std::string device_path;
    std::string friendly_name;
    std::string vendor;
    std::string product;
    std::string serial_number;
    uint32_t device_number;
    bool removable;
    bool usb;
};

std::vector<WindowsDeviceInfo> enumerateWindowsDevices();
Result getWindowsDeviceInfo(const std::string& device_path, WindowsDeviceInfo& info);

#endif

// ============================================================================
// macOS-Specific
// ============================================================================

#ifdef __APPLE__

struct MacDeviceInfo {
    std::string device_path;
    std::string bsd_name;
    std::string volume_name;
    std::string filesystem;
    bool removable;
    bool ejectable;
    bool usb;
};

std::vector<MacDeviceInfo> enumerateMacDevices();
Result getMacDeviceInfo(const std::string& device_path, MacDeviceInfo& info);

#endif

} // namespace opm
