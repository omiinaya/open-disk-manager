#include "opm/partition_table.hpp"
#include "opm/disk_io.hpp"
#include "opm/exceptions.hpp"
#include <cstring>

namespace opm {

// Factory implementation
std::unique_ptr<PartitionTable> PartitionTable::load(const std::string& device_path) {
    auto disk = DiskIO::openReadOnly(device_path);
    if (!disk) {
        throw DeviceNotFoundException(device_path);
    }
    return load(disk);
}

std::unique_ptr<PartitionTable> PartitionTable::load(std::shared_ptr<DiskIO> disk) {
    if (!disk || !disk->isOpen()) {
        throw DeviceException("Disk not open");
    }
    
    // Read first sector to detect partition table type
    uint8_t sector[512];
    auto result = disk->readSector(sector, 0);
    if (result.failed()) {
        throw ReadException("Failed to read first sector");
    }
    
    // Check for MBR signature
    uint16_t mbr_signature = *reinterpret_cast<uint16_t*>(&sector[510]);
    
    if (mbr_signature == 0xAA55) {
        // Could be MBR or protective MBR for GPT
        // Check for GPT at sector 1
        uint8_t gpt_sector[512];
        result = disk->readSector(gpt_sector, 1);
        
        if (result.success()) {
            // Check for "EFI PART" signature
            uint64_t gpt_signature = *reinterpret_cast<uint64_t*>(gpt_sector);
            if (gpt_signature == 0x5452415020494645ULL) {
                // This is GPT
                try {
                    return std::make_unique<GPTTable>(disk);
                } catch (const std::exception&) {
                    // Corrupt GPT header - fall through to recovery/repair
                    // paths rather than throwing to the caller.
                    return nullptr;
                }
            }
        }
        
        // Check first partition entry for GPT protective (0xEE)
        uint8_t partition_type = sector[446 + 4]; // First partition entry type
        if (partition_type == 0xEE) {
            // GPT protective MBR - but the primary header may be corrupt.
            try {
                return std::make_unique<GPTTable>(disk);
            } catch (const std::exception&) {
                return nullptr;
            }
        }
        
        // Regular MBR
        return std::make_unique<MBRTable>(disk);
    }
    
    // Could be other formats (APM, etc.) - not implemented yet
    return nullptr;
}

std::unique_ptr<PartitionTable> PartitionTable::create(std::shared_ptr<DiskIO> disk,
                                                       TableType type) {
    switch (type) {
        case TableType::MBR:
            return MBRTable::createNew(disk);
        case TableType::GPT:
            return GPTTable::createNew(disk);
        default:
            throw ValidationException("Cannot create a partition table of this type");
    }
}

// Default implementations for base class
std::optional<Partition> PartitionTable::getPartition(int number) const {
    auto parts = getPartitions();
    if (number > 0 && number <= static_cast<int>(parts.size())) {
        return parts[number - 1];
    }
    return std::nullopt;
}

uint64_t PartitionTable::getFreeSpace() const {
    uint64_t used = getUsedSpace();
    uint64_t total = getTotalSpace();
    return total > used ? total - used : 0;
}

uint64_t PartitionTable::getUsedSpace() const {
    uint64_t used = 0;
    for (const auto& part : getPartitions()) {
        used += part.sizeBytes();
    }
    return used;
}

bool PartitionTable::canConvertTo([[maybe_unused]] TableType type) const {
    return false; // Base implementation
}

Result PartitionTable::convertTo([[maybe_unused]] TableType type) {
    return Result::error("Conversion not supported");
}

} // namespace opm
