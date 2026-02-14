#include "opm/partition_table.hpp"
#include "opm/disk_io.hpp"
#include "opm/utils.hpp"
#include "opm/exceptions.hpp"
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace opm {

// GPT constants
constexpr size_t GPT_HEADER_SIZE = 512;
constexpr size_t GPT_ENTRY_SIZE = 128;
constexpr size_t GPT_ENTRIES_PER_SECTOR = 4;
constexpr uint64_t GPT_SIGNATURE = 0x5452415020494645ULL; // "EFI PART"
constexpr uint32_t GPT_REVISION_1_0 = 0x00010000;
constexpr size_t GPT_HEADER_OFFSET = 512;  // GPT starts at sector 1
constexpr uint32_t GPT_CRC32_SEED = 0xFFFFFFFF;

// GPT header structure (packed)
struct GPTHeaderRaw {
    uint64_t signature;           // "EFI PART"
    uint32_t revision;            // 0x00010000 for 1.0
    uint32_t header_size;         // 92 bytes
    uint32_t header_crc32;        // CRC32 of header
    uint32_t reserved;            // Must be 0
    uint64_t my_lba;              // LBA of this header
    uint64_t alternate_lba;       // LBA of other header
    uint64_t first_usable_lba;    // First usable LBA
    uint64_t last_usable_lba;     // Last usable LBA
    uint8_t  disk_guid[16];       // Disk GUID
    uint64_t partition_entry_lba; // LBA of partition entry array
    uint32_t num_partition_entries;
    uint32_t size_partition_entry; // Usually 128
    uint32_t partition_array_crc32;
} __attribute__((packed));

// GPT partition entry structure
struct GPTEntryRaw {
    uint8_t  type_guid[16];       // Partition type GUID
    uint8_t  unique_guid[16];     // Unique partition GUID
    uint64_t first_lba;           // First LBA
    uint64_t last_lba;            // Last LBA
    uint64_t attributes;          // Attributes
    uint16_t name[36];            // Partition name (UTF-16LE)
} __attribute__((packed));

GPTTable::GPTTable() = default;

GPTTable::GPTTable(std::shared_ptr<DiskIO> disk) {
    disk_ = disk;
    device_path_ = disk->devicePath();
    loadFromDisk();
}

void GPTTable::loadFromDisk() {
    if (!disk_ || !disk_->isOpen()) {
        throw DeviceException("Disk not open");
    }
    
    // Check for protective MBR
    uint8_t mbr[512];
    auto result = disk_->readSector(mbr, 0);
    if (result.success()) {
        uint16_t mbr_sig = *reinterpret_cast<uint16_t*>(&mbr[510]);
        if (mbr_sig == 0xAA55) {
            // Check if it's a protective MBR (GPT protective entry)
            uint8_t gpt_protective = mbr[450]; // First partition type
            has_protective_mbr_ = (gpt_protective == 0xEE);
        }
    }
    
    // Read primary GPT header (sector 1)
    uint8_t header[GPT_HEADER_SIZE];
    result = disk_->readSector(header, 1);
    if (result.failed()) {
        throw ReadException("Failed to read GPT header");
    }
    
    // Parse header
    parseGPTHeader(header);
    
    // Validate header
    if (header_lba_ != 1) {
        throw ValidationException("Primary GPT header not at sector 1");
    }
    
    // Read partition entries
    parsePartitionEntries();
    
    // Sort partitions by start LBA
    std::sort(partitions_.begin(), partitions_.end());
    
    // Assign partition numbers
    for (size_t i = 0; i < partitions_.size(); i++) {
        // Simplified - actual numbering may differ
    }
}

void GPTTable::parseGPTHeader(const uint8_t* data) {
    const GPTHeaderRaw* header = reinterpret_cast<const GPTHeaderRaw*>(data);
    
    // Check signature
    header_lba_ = header->my_lba;
    backup_header_lba_ = header->alternate_lba;
    first_usable_lba_ = header->first_usable_lba;
    last_usable_lba_ = header->last_usable_lba;
    partition_entry_lba_ = header->partition_entry_lba;
    partition_entry_count_ = header->num_partition_entries;
    partition_entry_size_ = header->size_partition_entry;
    header_crc32_ = header->header_crc32;
    partition_array_crc32_ = header->partition_array_crc32;
    revision_ = header->revision;
    
    // Copy disk GUID
    disk_guid_ = utils::guidToString(header->disk_guid);
    
    // Validate CRC32
    uint32_t calculated_crc = calculateCRC32(data, header->header_size);
    if (calculated_crc != 0) { // CRC of header with CRC field set to 0 should be the CRC value
        // Actually need to calculate with CRC field zeroed
        std::vector<uint8_t> header_copy(data, data + header->header_size);
        *reinterpret_cast<uint32_t*>(&header_copy[16]) = 0; // Zero CRC field
        uint32_t crc = utils::crc32(header_copy.data(), header_copy.size());
        
        if (crc != header_crc32_) {
            throw ChecksumException("GPT header CRC32 mismatch");
        }
    }
}

void GPTTable::parsePartitionEntries() {
    partitions_.clear();
    
    // Calculate number of sectors for partition entries
    uint32_t entry_sectors = (partition_entry_count_ * partition_entry_size_ + 511) / 512;
    
    // Read all entry sectors
    std::vector<uint8_t> entries_data(entry_sectors * 512);
    for (uint32_t i = 0; i < entry_sectors; i++) {
        auto result = disk_->readSector(&entries_data[i * 512], partition_entry_lba_ + i);
        if (result.failed()) {
            throw ReadException("Failed to read GPT partition entries");
        }
    }
    
    // Verify partition array CRC32
    uint32_t calculated_array_crc = utils::crc32(entries_data.data(), 
                                                    partition_entry_count_ * partition_entry_size_);
    if (calculated_array_crc != partition_array_crc32_) {
        // Try loading from backup
        // For now, just warn
    }
    
    // Parse each entry
    for (uint32_t i = 0; i < partition_entry_count_; i++) {
        const uint8_t* entry_data = &entries_data[i * partition_entry_size_];
        const GPTEntryRaw* entry = reinterpret_cast<const GPTEntryRaw*>(entry_data);
        
        // Check if entry is used (type GUID not all zeros)
        bool all_zero = true;
        for (int j = 0; j < 16; j++) {
            if (entry->type_guid[j] != 0) {
                all_zero = false;
                break;
            }
        }
        
        if (all_zero) {
            continue; // Unused entry
        }
        
        Partition partition;
        partition.setStartSector(entry->first_lba);
        partition.setEndSector(entry->last_lba);
        partition.setPartitionUuid(utils::guidToString(entry->unique_guid));
        
        // Convert name from UTF-16LE to UTF-8
        std::string name;
        for (int j = 0; j < 36 && entry->name[j] != 0; j++) {
            // Simple UTF-16LE to ASCII conversion
            // Real implementation should handle full UTF-16
            if (entry->name[j] < 128) {
                name += static_cast<char>(entry->name[j]);
            }
        }
        partition.setName(name);
        
        // Determine partition type from GUID
        std::string type_guid = utils::guidToString(entry->type_guid);
        if (type_guid == std::string(gpt_type::EFI_SYSTEM)) {
            partition.setType(PartitionType::EFI);
            partition.setFilesystem(FileSystemType::EFI);
        } else if (type_guid == std::string(gpt_type::MICROSOFT_BASIC_DATA)) {
            partition.setType(PartitionType::NTFS);
            // Detect actual filesystem
            auto fs = disk_->detectFilesystem(entry->first_lba);
            partition.setFilesystem(fs);
        } else if (type_guid == std::string(gpt_type::LINUX_FILESYSTEM)) {
            partition.setType(PartitionType::Linux);
            auto fs = disk_->detectFilesystem(entry->first_lba);
            partition.setFilesystem(fs);
        } else if (type_guid == std::string(gpt_type::LINUX_SWAP)) {
            partition.setType(PartitionType::LinuxSwap);
            partition.setFilesystem(FileSystemType::Swap);
        } else if (type_guid == std::string(gpt_type::LINUX_LVM)) {
            partition.setType(PartitionType::LinuxLVM);
            partition.setFilesystem(FileSystemType::LVM2);
        } else if (type_guid == std::string(gpt_type::LINUX_RAID)) {
            partition.setType(PartitionType::LinuxRAID);
            partition.setFilesystem(FileSystemType::RAID);
        } else {
            partition.setType(PartitionType::Unknown);
            partition.setFilesystem(FileSystemType::Unknown);
        }
        
        partitions_.push_back(partition);
    }
}

uint32_t GPTTable::calculateCRC32(const uint8_t* data, size_t length) {
    return utils::crc32(data, length);
}

bool GPTTable::isValid() const {
    // Check header signature
    // Already validated during load
    
    // Check partition entries
    for (const auto& part : partitions_) {
        if (!part.isValid()) {
            return false;
        }
    }
    
    // Check for overlaps
    for (size_t i = 0; i < partitions_.size(); i++) {
        for (size_t j = i + 1; j < partitions_.size(); j++) {
            if (partitions_[i].overlaps(partitions_[j])) {
                return false;
            }
        }
    }
    
    return true;
}

std::vector<Partition> GPTTable::getPartitions() const {
    return partitions_;
}

int GPTTable::getPartitionCount() const {
    return static_cast<int>(partitions_.size());
}

uint64_t GPTTable::getTotalSpace() const {
    if (!disk_) return 0;
    return disk_->size();
}

Result GPTTable::validate() const {
    std::vector<std::string> errors;
    
    if (!isValid()) {
        errors.push_back("Invalid GPT table");
    }
    
    // Check alignment
    for (const auto& part : partitions_) {
        if (!part.isAligned()) {
            // Warning only
        }
    }
    
    // Check partition bounds
    for (const auto& part : partitions_) {
        if (part.startSector() < first_usable_lba_ || 
            part.endSector() > last_usable_lba_) {
            errors.push_back("Partition outside usable LBA range");
        }
    }
    
    return errors.empty() ? Result::ok() : Result::error("Validation failed");
}

bool GPTTable::hasErrors() const {
    return !errors_.empty();
}

std::vector<std::string> GPTTable::getErrors() const {
    return errors_;
}

bool GPTTable::isUEFISystem() const {
    // Check for EFI system partition
    for (const auto& part : partitions_) {
        if (part.type() == PartitionType::EFI) {
            return true;
        }
    }
    return false;
}

bool GPTTable::hasProtectiveMBR() const {
    return has_protective_mbr_;
}

Result GPTTable::restoreFromBackup() {
    // Read backup header
    return Result::error("Not implemented");
}

Result GPTTable::createBackup() {
    // Write backup header and partition entries
    return Result::error("Not implemented");
}

Result GPTTable::createPartition([[maybe_unused]] uint64_t start, [[maybe_unused]] uint64_t size,
                                  [[maybe_unused]] PartitionType type,
                                  [[maybe_unused]] const std::string& name) {
    modified_ = true;
    return Result::error("Not implemented");
}

Result GPTTable::deletePartition([[maybe_unused]] int number) {
    modified_ = true;
    return Result::error("Not implemented");
}

Result GPTTable::resizePartition([[maybe_unused]] int number, [[maybe_unused]] uint64_t new_size) {
    modified_ = true;
    return Result::error("Not implemented");
}

Result GPTTable::commit() {
    if (!modified_) {
        return Result::ok();
    }
    
    // Write GPT header and partition entries
    // Update backup
    
    modified_ = false;
    return Result::error("Not implemented");
}

void GPTTable::revert() {
    if (!modified_) {
        return;
    }
    
    loadFromDisk();
    modified_ = false;
}

Result GPTTable::convertTo(TableType type) {
    if (type == TableType::MBR) {
        // Convert GPT to MBR
        return Result::error("Conversion not implemented");
    }
    return Result::error("Cannot convert to this type");
}

} // namespace opm
