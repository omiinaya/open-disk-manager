#pragma once

#include "types.hpp"
#include "disk_io.hpp"
#include <string>
#include <memory>

namespace opm {

// ============================================================================
// Encryption Detection
// ============================================================================

enum class EncryptionType {
    None,       // Not encrypted (or unknown)
    BitLocker,  // Windows BitLocker (FVE-FS)
    LUKS1,      // Linux LUKS v1
    LUKS2,      // Linux LUKS v2
    VeraCrypt,  // VeraCrypt/TrueCrypt (no signature - detection heuristic)
    Unknown
};

// Convert an EncryptionType to a human-readable string
std::string encryptionTypeName(EncryptionType type);

// Detect the encryption scheme on a partition starting at the given sector.
// Reads the partition's first sectors and matches known signatures.
EncryptionType detectEncryption(std::shared_ptr<DiskIO> disk,
                                uint64_t start_sector);

// BitLocker specific: returns true when the boot sector carries the
// "FVE-FS" signature (Windows BitLocker encrypted volume).
bool isBitLocker(std::shared_ptr<DiskIO> disk, uint64_t start_sector);

// LUKS specific: returns true when the LUKS magic ("LUKS\xba\xbe") is found.
bool isLUKS(std::shared_ptr<DiskIO> disk, uint64_t start_sector);

// Fill a short human-readable description of the encryption state
// (scheme + key-slot count for LUKS, version for BitLocker where available).
Result describeEncryption(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                          std::string& description);

} // namespace opm
