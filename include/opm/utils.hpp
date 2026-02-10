#pragma once

#include "types.hpp"
#include <string>
#include <vector>

namespace opm {
namespace utils {

// Formatting
std::string formatBytes(uint64_t bytes);
std::string formatSector(uint64_t sector, uint32_t sector_size = 512);

// String utilities
std::string trim(const std::string& str);
std::vector<std::string> split(const std::string& str, char delimiter);
bool startsWith(const std::string& str, const std::string& prefix);
bool endsWith(const std::string& str, const std::string& suffix);
std::string toLower(const std::string& str);
std::string toUpper(const std::string& str);

// Conversion
std::string bytesToHex(const std::vector<uint8_t>& bytes);
std::string bytesToHex(const uint8_t* data, size_t size);
std::vector<uint8_t> hexToBytes(const std::string& hex);

// Device utilities
bool isValidDevicePath(const std::string& path);
std::string getDeviceName(const std::string& path);

// CRC32 calculation
uint32_t crc32(const uint8_t* data, size_t length);

// GUID utilities
std::string guidToString(const uint8_t* guid);
void guidFromString(const std::string& str, uint8_t* guid);

// Alignment
inline bool isAligned(uint64_t value, uint64_t alignment) {
    return (value % alignment) == 0;
}

inline uint64_t alignUp(uint64_t value, uint64_t alignment) {
    return ((value + alignment - 1) / alignment) * alignment;
}

inline uint64_t alignDown(uint64_t value, uint64_t alignment) {
    return (value / alignment) * alignment;
}

} // namespace utils
} // namespace opm
