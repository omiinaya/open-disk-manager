#include "opm/encryption.hpp"
#include <cstring>
#include <sstream>

namespace opm {

std::string encryptionTypeName(EncryptionType type) {
    switch (type) {
        case EncryptionType::None:       return "None";
        case EncryptionType::BitLocker:  return "BitLocker (FVE-FS)";
        case EncryptionType::LUKS1:      return "LUKS v1";
        case EncryptionType::LUKS2:      return "LUKS v2";
        case EncryptionType::VeraCrypt:  return "VeraCrypt/TrueCrypt";
        default:                         return "Unknown";
    }
}

bool isBitLocker(std::shared_ptr<DiskIO> disk, uint64_t start_sector) {
    if (!disk) return false;
    std::vector<uint8_t> sector(512, 0);
    if (disk->readSector(sector.data(), start_sector).failed()) {
        return false;
    }
    // BitLocker signature: "FVE-FS" at offset 3 (after the 3-byte jump)
    return std::memcmp(sector.data() + 3, "FVE-FS", 6) == 0;
}

bool isLUKS(std::shared_ptr<DiskIO> disk, uint64_t start_sector) {
    if (!disk) return false;
    std::vector<uint8_t> sector(512, 0);
    if (disk->readSector(sector.data(), start_sector).failed()) {
        return false;
    }
    // LUKS magic at offset 0: "LUKS\xba\xbe"
    static const uint8_t magic[] = {'L', 'U', 'K', 'S', 0xba, 0xbe};
    return std::memcmp(sector.data(), magic, 6) == 0;
}

EncryptionType detectEncryption(std::shared_ptr<DiskIO> disk,
                                uint64_t start_sector) {
    if (!disk) return EncryptionType::Unknown;
    std::vector<uint8_t> sector(512, 0);
    if (disk->readSector(sector.data(), start_sector).failed()) {
        return EncryptionType::Unknown;
    }

    // LUKS magic at offset 0: "LUKS\xba\xbe"
    static const uint8_t luks_magic[] = {'L', 'U', 'K', 'S', 0xba, 0xbe};
    if (std::memcmp(sector.data(), luks_magic, 6) == 0) {
        // LUKS v2 uses the same magic; the version field at offset 6 differs:
        // LUKS1 header: version u16 = 1; LUKS2: "LUKS\xba\xbe" + version u16 = 2
        uint16_t version = static_cast<uint16_t>(sector[6]) |
                           (static_cast<uint16_t>(sector[7]) << 8);
        return (version == 2) ? EncryptionType::LUKS2 : EncryptionType::LUKS1;
    }

    // BitLocker signature: "FVE-FS" at offset 3 (after the 3-byte jump)
    if (std::memcmp(sector.data() + 3, "FVE-FS", 6) == 0) {
        return EncryptionType::BitLocker;
    }

    return EncryptionType::None;
}

Result describeEncryption(std::shared_ptr<DiskIO> disk, uint64_t start_sector,
                          std::string& description) {
    if (!disk) {
        return Result::error("Invalid disk");
    }

    EncryptionType type = detectEncryption(disk, start_sector);
    if (type == EncryptionType::None) {
        description = "No encryption signature detected";
        return Result::ok();
    }

    std::ostringstream os;
    os << encryptionTypeName(type);

    if (type == EncryptionType::LUKS1 || type == EncryptionType::LUKS2) {
        std::vector<uint8_t> sector(512, 0);
        if (disk->readSector(sector.data(), start_sector).success()) {
            // LUKS1: key slots start at offset 0x40 (8 slots, 48 bytes each)
            uint32_t active_slots = 0;
            if (type == EncryptionType::LUKS1) {
                for (int i = 0; i < 8; i++) {
                    uint32_t offset = 0x40 + i * 48;
                    // active when the key-material offset field is non-zero
                    uint32_t km_offset = 0;
                    std::memcpy(&km_offset, sector.data() + offset + 8, 4);
                    if (km_offset != 0) active_slots++;
                }
                os << " (" << active_slots << " active key slot(s))";
            }
            // cipher name (LUKS1) at offset 0x38, 32 bytes
            char cipher[33] = {0};
            std::memcpy(cipher, sector.data() + 0x38, 32);
            os << ", cipher=" << cipher;
        }
    } else if (type == EncryptionType::BitLocker) {
        std::vector<uint8_t> sector(512, 0);
        if (disk->readSector(sector.data(), start_sector).success()) {
            // BitLocker version is stored in the FVE metadata volume header
            // (at offset 512+), but the boot sector itself carries the FVE
            // GUID near the end. Report a generic description.
            os << " (encrypted volume; unlock requires Windows tools or a recovery key)";
        }
    }

    description = os.str();
    return Result::ok();
}

} // namespace opm
