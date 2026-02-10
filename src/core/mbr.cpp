#include "opm/partition_table.hpp"
#include "opm/disk_io.hpp"
#include "opm/utils.hpp"
#include "opm/exceptions.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>

namespace opm {

// MBR constants
constexpr size_t MBR_SIZE = 512;
constexpr size_t PARTITION_ENTRY_OFFSET = 446;
constexpr size_t PARTITION_ENTRY_SIZE = 16;
constexpr size_t PARTITION_ENTRY_COUNT = 4;
constexpr uint16_t MBR_SIGNATURE = 0xAA55;

// MBR partition entry structure (packed)
struct MBRPartitionEntryRaw {
    uint8_t status;           // Bootable flag
    uint8_t start_head;       // Starting head
    uint8_t start_sector;     // Starting sector (bits 0-5)
    uint8_t start_cylinder;   // Starting cylinder
    uint8_t type;             // Partition type
    uint8_t end_head;         // Ending head
    uint8_t end_sector;       // Ending sector
    uint8_t end_cylinder;     // Ending cylinder
    uint32_t start_lba;       // Starting LBA (little-endian)
    uint32_t sector_count;    // Sector count (little-endian)
} __attribute__((packed));

// Helper to read little-endian values
template<typename T>
T readLE(const uint8_t* data) {
    T result = 0;
    for (size_t i = 0; i < sizeof(T); i++) {
        result |= static_cast<T>(data[i]) << (i * 8);
    }
    return result;
}

MBRTable::MBRTable() = default;

MBRTable::MBRTable(std::shared_ptr<DiskIO> disk) {
    disk_ = disk;
    device_path_ = disk->devicePath();
    loadFromDisk();
}

void MBRTable::loadFromDisk() {
    if (!disk_ || !disk_->isOpen()) {
        throw DeviceException("Disk not open");
    }
    
    // Read MBR sector
    uint8_t mbr[MBR_SIZE];
    auto result = disk_->readSector(mbr, 0);
    if (result.failed()) {
        throw ReadException("Failed to read MBR: " + result.message);
    }
    
    // Check signature
    uint16_t signature = readLE<uint16_t>(&mbr[510]);
    if (signature != MBR_SIGNATURE) {
        // Not a valid MBR
        return;
    }
    
    // Read disk signature (bytes 440-443)
    disk_signature_ = readLE<uint32_t>(&mbr[440]);
    
    // Copy boot code
    std::memcpy(boot_code_, mbr, 440);
    
    // Parse partition entries
    partitions_.clear();
    mbr_entries_.clear();
    extended_start_ = 0;
    
    for (size_t i = 0; i < PARTITION_ENTRY_COUNT; i++) {
        const uint8_t* entry_data = &mbr[PARTITION_ENTRY_OFFSET + (i * PARTITION_ENTRY_SIZE)];
        MBRPartitionEntry entry;
        
        entry.status = entry_data[0];
        entry.start_head = entry_data[1];
        entry.start_sector = (entry_data[2] & 0x3F);
        entry.start_cylinder = ((entry_data[2] & 0xC0) << 2) | entry_data[3];
        entry.type = entry_data[4];
        entry.end_head = entry_data[5];
        entry.end_sector = (entry_data[6] & 0x3F);
        entry.end_cylinder = ((entry_data[6] & 0xC0) << 2) | entry_data[7];
        entry.start_lba = readLE<uint32_t>(&entry_data[8]);
        entry.sector_count = readLE<uint32_t>(&entry_data[12]);
        
        mbr_entries_.push_back(entry);
        
        // Create Partition object for non-empty entries
        if (entry.type != 0x00) {
            Partition partition;
            partition.setStartSector(entry.start_lba);
            partition.setEndSector(entry.start_lba + entry.sector_count - 1);
            partition.setType(static_cast<PartitionType>(entry.type));
            partition.setBootable(entry.status == 0x80);
            
            // Detect file system
            auto fs_type = disk_->detectFilesystem(entry.start_lba);
            partition.setFilesystem(fs_type);
            
            // Set partition number
            // This is simplified - actual numbering depends on extended partitions
            
            partitions_.push_back(partition);
            
            // Check for extended partition
            if (isExtendedPartitionType(entry.type)) {
                extended_start_ = entry.start_lba;
            }
        }
    }
    
    // Load logical partitions from extended partition
    if (extended_start_ > 0) {
        loadExtendedPartitions();
    }
    
    // Sort partitions by start sector
    std::sort(partitions_.begin(), partitions_.end());
    
    // Assign partition numbers
    for (size_t i = 0; i < partitions_.size(); i++) {
        // This is a simplified approach
        // Real implementation needs to handle primary vs extended properly
    }
}

void MBRTable::loadExtendedPartitions() {
    uint64_t current_ebr = extended_start_;
    int logical_num = 5; // Logical partitions start at 5
    
    while (current_ebr > 0) {
        // Read Extended Boot Record
        uint8_t ebr[MBR_SIZE];
        auto result = disk_->readSector(ebr, current_ebr);
        if (result.failed()) {
            break;
        }
        
        // Check signature
        uint16_t signature = readLE<uint16_t>(&ebr[510]);
        if (signature != MBR_SIGNATURE) {
            break;
        }
        
        // Parse first entry (logical partition)
        const uint8_t* entry1 = &ebr[PARTITION_ENTRY_OFFSET];
        uint8_t type1 = entry1[4];
        
        if (type1 != 0x00) {
            Partition partition;
            uint32_t start_lba = readLE<uint32_t>(&entry1[8]);
            uint32_t sector_count = readLE<uint32_t>(&entry1[12]);
            
            partition.setStartSector(current_ebr + start_lba);
            partition.setEndSector(current_ebr + start_lba + sector_count - 1);
            partition.setType(static_cast<PartitionType>(type1));
            partition.setBootable(entry1[0] == 0x80);
            
            auto fs_type = disk_->detectFilesystem(partition.startSector());
            partition.setFilesystem(fs_type);
            
            partitions_.push_back(partition);
        }
        
        // Parse second entry (next EBR link)
        const uint8_t* entry2 = &ebr[PARTITION_ENTRY_OFFSET + PARTITION_ENTRY_SIZE];
        uint8_t type2 = entry2[4];
        
        if (type2 == 0x05 || type2 == 0x0F) {
            uint32_t next_ebr_relative = readLE<uint32_t>(&entry2[8]);
            current_ebr = extended_start_ + next_ebr_relative;
        } else {
            current_ebr = 0; // No more extended partitions
        }
        
        logical_num++;
    }
}

bool MBRTable::isExtendedPartitionType(uint8_t type) const {
    return type == 0x05 || type == 0x0F || type == 0x85;
}

bool MBRTable::isValid() const {
    if (partitions_.empty()) {
        // Empty partition table is still valid
        return true;
    }
    
    // Check for overlapping partitions
    for (size_t i = 0; i < partitions_.size(); i++) {
        for (size_t j = i + 1; j < partitions_.size(); j++) {
            if (partitions_[i].overlaps(partitions_[j])) {
                return false;
            }
        }
    }
    
    return true;
}

std::vector<Partition> MBRTable::getPartitions() const {
    return partitions_;
}

int MBRTable::getPartitionCount() const {
    return static_cast<int>(partitions_.size());
}

uint64_t MBRTable::getTotalSpace() const {
    if (!disk_) return 0;
    return disk_->size();
}

Result MBRTable::validate() const {
    std::vector<std::string> errors;
    
    if (!isValid()) {
        errors.push_back("Invalid partition table: overlapping partitions detected");
    }
    
    // Check for gaps (optional - not necessarily an error)
    // Check alignment
    for (const auto& part : partitions_) {
        if (!part.isAligned()) {
            // Warning only, not an error
        }
    }
    
    // Check extended partition validity
    if (hasExtendedPartition()) {
        // Verify extended partition chain
    }
    
    return errors.empty() ? Result::ok() : Result::error("Validation failed");
}

bool MBRTable::hasErrors() const {
    return !errors_.empty();
}

std::vector<std::string> MBRTable::getErrors() const {
    return errors_;
}

bool MBRTable::hasExtendedPartition() const {
    return extended_start_ > 0;
}

Result MBRTable::createPartition(uint64_t start, uint64_t size,
                                  PartitionType type,
                                  const std::string& name) {
    // Check alignment
    if (!utils::isAligned(start, ALIGNMENT_1MB)) {
        return Result::error("Start sector must be aligned to 1MB boundary");
    }
    
    uint64_t sector_count = size / disk_->sectorSize();
    if (sector_count < ALIGNMENT_1MB) {
        return Result::error("Partition must be at least 1MB");
    }
    
    // Find free partition slot
    int free_slot = -1;
    for (size_t i = 0; i < PARTITION_ENTRY_COUNT; i++) {
        if (mbr_entries_[i].type == 0x00) {
            free_slot = static_cast<int>(i);
            break;
        }
    }
    
    if (free_slot == -1) {
        return Result::error("No free partition slots available");
    }
    
    // Create partition entry
    auto& entry = mbr_entries_[free_slot];
    entry.status = 0x00;  // Not bootable by default
    entry.type = static_cast<uint8_t>(type);
    entry.start_lba = static_cast<uint32_t>(start);
    entry.sector_count = static_cast<uint32_t>(sector_count);
    
    // Calculate CHS (simplified - LBA mode is what actually matters)
    entry.start_head = 0;
    entry.start_sector = 0x01;
    entry.start_cylinder = 0;
    entry.end_head = 0xFE;
    entry.end_sector = 0x3F;
    entry.end_cylinder = 0xFF;
    
    // Create partition object
    Partition partition;
    partition.setStartSector(start);
    partition.setEndSector(start + sector_count - 1);
    partition.setType(type);
    partition.setName(name);
    partition.setBootable(false);
    
    partitions_.push_back(partition);
    
    // Sort partitions by start sector
    std::sort(partitions_.begin(), partitions_.end());
    
    modified_ = true;
    return Result::ok();
}

Result MBRTable::deletePartition(int number) {
    if (number < 1 || number > static_cast<int>(partitions_.size())) {
        return Result::error("Invalid partition number");
    }
    
    // Find partition in our list
    int partition_index = -1;
    int mbr_entry_index = -1;
    
    for (size_t i = 0; i < partitions_.size(); i++) {
        if (partitions_[i].number() == number) {
            partition_index = static_cast<int>(i);
            break;
        }
    }
    
    if (partition_index == -1) {
        return Result::error("Partition not found");
    }
    
    // Find the corresponding MBR entry
    uint64_t start = partitions_[partition_index].startSector();
    for (size_t i = 0; i < PARTITION_ENTRY_COUNT; i++) {
        if (mbr_entries_[i].start_lba == start) {
            mbr_entry_index = static_cast<int>(i);
            break;
        }
    }
    
    if (mbr_entry_index == -1) {
        return Result::error("MBR entry not found for partition");
    }
    
    // Clear the MBR entry
    auto& entry = mbr_entries_[mbr_entry_index];
    entry.status = 0x00;
    entry.type = 0x00;
    entry.start_lba = 0;
    entry.sector_count = 0;
    entry.start_head = 0;
    entry.start_sector = 0;
    entry.start_cylinder = 0;
    entry.end_head = 0;
    entry.end_sector = 0;
    entry.end_cylinder = 0;
    
    // Remove from partition list
    partitions_.erase(partitions_.begin() + partition_index);
    
    modified_ = true;
    return Result::ok();
}

Result MBRTable::resizePartition(int number, uint64_t new_size) {
    if (number < 1 || number > static_cast<int>(partitions_.size())) {
        return Result::error("Invalid partition number");
    }
    
    // Find partition
    int partition_index = -1;
    int mbr_entry_index = -1;
    
    for (size_t i = 0; i < partitions_.size(); i++) {
        if (partitions_[i].number() == number) {
            partition_index = static_cast<int>(i);
            break;
        }
    }
    
    if (partition_index == -1) {
        return Result::error("Partition not found");
    }
    
    // Find the corresponding MBR entry
    uint64_t start = partitions_[partition_index].startSector();
    for (size_t i = 0; i < PARTITION_ENTRY_COUNT; i++) {
        if (mbr_entries_[i].start_lba == start) {
            mbr_entry_index = static_cast<int>(i);
            break;
        }
    }
    
    if (mbr_entry_index == -1) {
        return Result::error("MBR entry not found for partition");
    }
    
    uint64_t new_sector_count = new_size / disk_->sectorSize();
    
    // Update MBR entry
    auto& entry = mbr_entries_[mbr_entry_index];
    entry.sector_count = static_cast<uint32_t>(new_sector_count);
    
    // Update partition object
    partitions_[partition_index].setEndSector(
        partitions_[partition_index].startSector() + new_sector_count - 1
    );
    
    modified_ = true;
    return Result::ok();
}

Result MBRTable::commit() {
    if (!modified_) {
        return Result::ok();
    }
    
    if (!disk_ || !disk_->isOpen()) {
        return Result::error("Disk not open");
    }
    
    if (disk_->isReadOnly()) {
        return Result::error("Disk is read-only");
    }
    
    // Read existing MBR
    uint8_t mbr[MBR_SIZE];
    auto result = disk_->readSector(mbr, 0);
    if (result.failed()) {
        return Result::error("Failed to read MBR: " + result.message);
    }
    
    // Preserve boot code and disk signature
    std::memcpy(mbr, boot_code_, 440);
    // Write disk signature
    mbr[440] = (disk_signature_ >> 0) & 0xFF;
    mbr[441] = (disk_signature_ >> 8) & 0xFF;
    mbr[442] = (disk_signature_ >> 16) & 0xFF;
    mbr[443] = (disk_signature_ >> 24) & 0xFF;
    mbr[444] = 0x00;  // Reserved
    mbr[445] = 0x00;  // Reserved
    
    // Write partition entries
    for (size_t i = 0; i < PARTITION_ENTRY_COUNT; i++) {
        const auto& entry = mbr_entries_[i];
        uint8_t* entry_data = &mbr[PARTITION_ENTRY_OFFSET + (i * PARTITION_ENTRY_SIZE)];
        
        entry_data[0] = entry.status;
        entry_data[1] = entry.start_head;
        entry_data[2] = entry.start_sector | ((entry.start_cylinder >> 2) & 0xC0);
        entry_data[3] = entry.start_cylinder & 0xFF;
        entry_data[4] = entry.type;
        entry_data[5] = entry.end_head;
        entry_data[6] = entry.end_sector | ((entry.end_cylinder >> 2) & 0xC0);
        entry_data[7] = entry.end_cylinder & 0xFF;
        
        // Little-endian LBA and sector count
        entry_data[8] = (entry.start_lba >> 0) & 0xFF;
        entry_data[9] = (entry.start_lba >> 8) & 0xFF;
        entry_data[10] = (entry.start_lba >> 16) & 0xFF;
        entry_data[11] = (entry.start_lba >> 24) & 0xFF;
        
        entry_data[12] = (entry.sector_count >> 0) & 0xFF;
        entry_data[13] = (entry.sector_count >> 8) & 0xFF;
        entry_data[14] = (entry.sector_count >> 16) & 0xFF;
        entry_data[15] = (entry.sector_count >> 24) & 0xFF;
    }
    
    // Write signature
    mbr[510] = 0x55;
    mbr[511] = 0xAA;
    
    // Write MBR back to disk
    result = disk_->writeSector(mbr, 0);
    if (result.failed()) {
        return Result::error("Failed to write MBR: " + result.message);
    }
    
    // Flush changes
    result = disk_->flush();
    if (result.failed()) {
        return Result::error("Failed to flush changes: " + result.message);
    }
    
    modified_ = false;
    return Result::ok();
}

void MBRTable::revert() {
    if (!modified_) {
        return;
    }
    
    // Reload from disk
    loadFromDisk();
    modified_ = false;
}

Result MBRTable::convertTo(TableType type) {
    if (type == TableType::GPT) {
        // Convert MBR to GPT
        return Result::error("Conversion not implemented");
    }
    return Result::error("Cannot convert to this type");
}

} // namespace opm
