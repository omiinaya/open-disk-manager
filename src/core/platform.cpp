#include "opm/platform.hpp"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <unistd.h>

namespace opm {

Platform getCurrentPlatform() {
#if defined(__linux__)
    return Platform::Linux;
#elif defined(_WIN32)
    return Platform::Windows;
#elif defined(__APPLE__)
    return Platform::macOS;
#else
    return Platform::Unknown;
#endif
}

Architecture getCurrentArchitecture() {
#if defined(__x86_64__) || defined(_M_X64)
    return Architecture::x86_64;
#elif defined(__i386__) || defined(_M_IX86)
    return Architecture::x86;
#elif defined(__aarch64__) || defined(_M_ARM64)
    return Architecture::arm64;
#elif defined(__arm__) || defined(_M_ARM)
    return Architecture::arm;
#else
    return Architecture::Unknown;
#endif
}

bool isLinux() { return getCurrentPlatform() == Platform::Linux; }
bool isWindows() { return getCurrentPlatform() == Platform::Windows; }
bool isMacOS() { return getCurrentPlatform() == Platform::macOS; }
bool isUnixLike() { return isLinux() || isMacOS(); }

std::string PlatformPath::toNative(const std::string& path) {
#ifdef _WIN32
    std::string result = path;
    for (auto& c : result) { if (c == '/') c = '\\'; }
    return result;
#else
    return path;
#endif
}

std::string PlatformPath::fromNative(const std::string& path) {
#ifdef _WIN32
    std::string result = path;
    for (auto& c : result) { if (c == '\\') c = '/'; }
    return result;
#else
    return path;
#endif
}

char PlatformPath::separator() {
#ifdef _WIN32
    return '\\';
#else
    return '/';
#endif
}

std::string PlatformPath::join(const std::string& base, const std::string& relative) {
    std::filesystem::path base_path(base);
    std::filesystem::path rel_path(relative);
    return (base_path / rel_path).string();
}

std::string PlatformPath::devicePrefix() {
#ifdef _WIN32
    return "\\\\.\\";
#else
    return "/dev/";
#endif
}

bool PrivilegeManager::hasAdminPrivileges() {
#ifdef _WIN32
    return false;
#else
    return getuid() == 0;
#endif
}

bool PrivilegeManager::isElevated() { return hasAdminPrivileges(); }

Result getSystemInfo(SystemInfo& info) {
    info.os_name = "Unknown";
    info.architecture = getCurrentArchitecture();
    info.total_memory = 0;
    info.cpu_count = 1;
    
#ifdef __linux__
    std::ifstream meminfo("/proc/meminfo");
    if (meminfo.is_open()) {
        std::string line;
        while (std::getline(meminfo, line)) {
            if (line.find("MemTotal:") == 0) {
                std::istringstream iss(line);
                std::string label; uint64_t value; std::string unit;
                iss >> label >> value >> unit;
                info.total_memory = value * 1024;
            }
        }
    }
    
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        info.cpu_count = 0;
        while (std::getline(cpuinfo, line)) {
            if (line.find("processor") == 0) info.cpu_count++;
        }
    }
#endif

    return Result::ok();
}

std::vector<DevicePath> enumerateAllDevices() {
    std::vector<DevicePath> devices;
    
#ifdef __linux__
    for (char c = 'a'; c <= 'z'; ++c) {
        std::string path = "/dev/sd" + std::string(1, c);
        if (std::filesystem::exists(path)) {
            DevicePath dev;
            dev.path = path;
            dev.name = "sd" + std::string(1, c);
            dev.type = DeviceType::Disk;
            dev.removable = false;
            dev.size = 0;
            devices.push_back(dev);
        }
    }
    
    for (int i = 0; i < 10; ++i) {
        std::string path = "/dev/nvme" + std::to_string(i) + "n1";
        if (std::filesystem::exists(path)) {
            DevicePath dev;
            dev.path = path;
            dev.name = "nvme" + std::to_string(i) + "n1";
            dev.type = DeviceType::Disk;
            dev.removable = false;
            dev.size = 0;
            devices.push_back(dev);
        }
    }
#endif
    
    return devices;
}

} // namespace opm
