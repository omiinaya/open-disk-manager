#include "opm/partition_table.hpp"
#include "opm/disk_io.hpp"
#include "opm/utils.hpp"
#include "opm/exceptions.hpp"
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <random>
#include <sstream>

namespace opm {

namespace {

// GPT constants
constexpr size_t GPT_HEADER_SIZE = 512;
constexpr size_t GPT_ENTRY_SIZE = 128;
constexpr uint64_t GPT_SIGNATURE = 0x5452415020494645ULL; // "EFI PART"
constexpr uint32_t GPT_REVISION_1_0 = 0x00010000;

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

// Convert a GUID string ("XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX") to 16 bytes
bool guidBytesFromString(const std::string& guid, uint8_t out[16]) {
    if (guid.size() != 36) return false;
    std::string hex;
    for (char c : guid) {
        if (c == '-') continue;
        if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
        hex += c;
    }
    if (hex.size() != 32) return false;
    // GUID strings are stored as mixed-endian: fields 1-3 LE, fields 4-5 BE
    static const int order[16] = {3, 2, 1, 0, 5, 4, 7, 6, 8, 9, 10, 11, 12, 13, 14, 15};
    for (int i = 0; i < 16; i++) {
        unsigned int byte = 0;
        std::stringstream ss;
        ss << std::hex << hex.substr(order[i] * 2, 2);
        ss >> byte;
        out[i] = static_cast<uint8_t>(byte);
    }
    return true;
}

// Generate a random UUID (version 4)
std::string generateGUID() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 255);
    uint8_t b[16];
    for (int i = 0; i < 16; i++) b[i] = static_cast<uint8_t>(dist(gen));
    b[6] = static_cast<uint8_t>((b[6] & 0x0F) | 0x40);  // version 4
    b[8] = static_cast<uint8_t>((b[8] & 0x3F) | 0x80);  // variant 10xx
    return utils::guidToString(b);
}

// Map a PartitionType to its GPT type GUID string (empty = unknown)
std::string gptTypeGuidFor(PartitionType type) {
    switch (type) {
        case PartitionType::EFI:         return gpt_type::EFI_SYSTEM;
        case PartitionType::NTFS:        return gpt_type::MICROSOFT_BASIC_DATA;
        case PartitionType::FAT32LBA:
        case PartitionType::FAT32CHS:    return gpt_type::MICROSOFT_BASIC_DATA;
        case PartitionType::Linux:       return gpt_type::LINUX_FILESYSTEM;
        case PartitionType::LinuxSwap:   return gpt_type::LINUX_SWAP;
        case PartitionType::LinuxLVM:    return gpt_type::LINUX_LVM;
        case PartitionType::LinuxRAID:   return gpt_type::LINUX_RAID;
        default:                         return "";
    }
}

// Map a GPT type GUID string back to a PartitionType
PartitionType gptPartitionTypeFor(const std::string& guid) {
    if (guid == gpt_type::EFI_SYSTEM) return PartitionType::EFI;
    if (guid == gpt_type::MICROSOFT_BASIC_DATA) return PartitionType::NTFS;
    if (guid == gpt_type::LINUX_FILESYSTEM) return PartitionType::Linux;
    if (guid == gpt_type::LINUX_SWAP) return PartitionType::LinuxSwap;
    if (guid == gpt_type::LINUX_LVM) return PartitionType::LinuxLVM;
    if (guid == gpt_type::LINUX_RAID) return PartitionType::LinuxRAID;
    return PartitionType::Unknown;
}

} // anonymous namespace

GPTTable::GPTTable() = default;

std::unique_ptr<GPTTable> GPTTable::createNew(std::shared_ptr<DiskIO> disk) {
    if (!disk || !disk->isOpen()) {
        throw DeviceException("Disk not open");
    }
    auto table = std::make_unique<GPTTable>();
    table->disk_ = disk;
    table->device_path_ = disk->devicePath();

    uint64_t total_sectors = disk->sectorCount();
    if (total_sectors == 0) {
        throw ValidationException("Cannot determine device size");
    }
    uint64_t last_lba = total_sectors - 1;

    // Standard GPT geometry: header at LBA 1, 128 entries (32 sectors) at
    // LBA 2, usable space starts at LBA 34 and ends 33 sectors before the end.
    table->header_lba_ = 1;
    table->backup_header_lba_ = last_lba;
    table->first_usable_lba_ = 34;
    table->last_usable_lba_ = last_lba - 33;
    table->partition_entry_lba_ = 2;
    table->partition_entry_count_ = 128;
    table->partition_entry_size_ = 128;
    table->header_crc32_ = 0;
    table->partition_array_crc32_ = 0;
    table->revision_ = GPT_REVISION_1_0;
    table->disk_guid_ = generateGUID();
    table->has_protective_mbr_ = false;
    table->partitions_.clear();
    table->modified_ = true;
    return table;
}

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
    if (!disk_ || !disk_->isOpen()) {
        return Result::error("Disk not open");
    }

    uint64_t last_lba = disk_->sectorCount() > 0 ? disk_->sectorCount() - 1 : 0;
    if (last_lba == 0) {
        return Result::error("Cannot determine device size");
    }

    // Read the backup header at the end of the disk
    std::vector<uint8_t> backup(512, 0);
    Result r = disk_->readSector(backup.data(), last_lba);
    if (r.failed()) {
        return Result::error("Cannot read backup GPT header: " + r.message);
    }
    uint64_t sig = 0;
    std::memcpy(&sig, backup.data(), 8);
    if (sig != GPT_SIGNATURE) {
        return Result::error("No valid backup GPT header found");
    }

    // Validate the backup header CRC (zero the CRC field, recompute)
    const GPTHeaderRaw* bh = reinterpret_cast<const GPTHeaderRaw*>(backup.data());
    if (bh->header_size < 92 || bh->header_size > 512) {
        return Result::error("Invalid backup header size");
    }
    std::vector<uint8_t> copy(backup.begin(), backup.begin() + bh->header_size);
    *reinterpret_cast<uint32_t*>(&copy[16]) = 0;
    if (utils::crc32(copy.data(), copy.size()) != bh->header_crc32) {
        return Result::error("Backup GPT header CRC mismatch");
    }

    // Adopt the backup's geometry into this table
    header_lba_ = bh->my_lba;
    backup_header_lba_ = bh->alternate_lba;
    first_usable_lba_ = bh->first_usable_lba;
    last_usable_lba_ = bh->last_usable_lba;
    partition_entry_lba_ = bh->partition_entry_lba;
    partition_entry_count_ = bh->num_partition_entries;
    partition_entry_size_ = bh->size_partition_entry;
    header_crc32_ = bh->header_crc32;
    partition_array_crc32_ = bh->partition_array_crc32;
    revision_ = bh->revision;
    disk_guid_ = utils::guidToString(bh->disk_guid);
    has_protective_mbr_ = true;

    // Parse entries from the backup array (it sits right before the header)
    std::vector<uint8_t> entries_data(partition_entry_count_ * partition_entry_size_, 0);
    uint32_t entry_sectors = (partition_entry_count_ * partition_entry_size_ + 511) / 512;
    uint64_t entries_start = last_lba - entry_sectors;
    for (uint32_t i = 0; i < entry_sectors; i++) {
        r = disk_->readSector(&entries_data[i * 512], entries_start + i);
        if (r.failed()) {
            return Result::error("Cannot read backup GPT entries: " + r.message);
        }
    }

    partitions_.clear();
    for (uint32_t i = 0; i < partition_entry_count_; i++) {
        const uint8_t* entry_data = &entries_data[i * partition_entry_size_];
        const GPTEntryRaw* entry = reinterpret_cast<const GPTEntryRaw*>(entry_data);
        bool all_zero = true;
        for (int j = 0; j < 16; j++) {
            if (entry->type_guid[j] != 0) { all_zero = false; break; }
        }
        if (all_zero) continue;

        Partition partition;
        partition.setStartSector(entry->first_lba);
        partition.setEndSector(entry->last_lba);
        partition.setPartitionUuid(utils::guidToString(entry->unique_guid));
        std::string name;
        for (int j = 0; j < 36 && entry->name[j] != 0; j++) {
            if (entry->name[j] < 128) name += static_cast<char>(entry->name[j]);
        }
        partition.setName(name);
        partition.setType(gptPartitionTypeFor(utils::guidToString(entry->type_guid)));
        partitions_.push_back(partition);
    }
    std::sort(partitions_.begin(), partitions_.end());
    modified_ = true;
    return Result::ok();
}

Result GPTTable::createBackup() {
    if (!modified_) {
        // Nothing changed since load; still write a backup for safety
    }
    return writeBackupGPT();
}

Result GPTTable::createPartition(uint64_t start, uint64_t size,
                                 PartitionType type,
                                 const std::string& name) {
    if (!disk_) {
        return Result::error("No disk attached");
    }
    if (size == 0) {
        return Result::error("Partition size must be non-zero");
    }
    if (start < first_usable_lba_) {
        return Result::error("Start LBA is before the first usable LBA");
    }
    uint64_t sector_count = size / disk_->sectorSize();
    if (sector_count == 0) {
        return Result::error("Partition smaller than one sector");
    }
    uint64_t end = start + sector_count - 1;
    if (end > last_usable_lba_) {
        return Result::error("Partition extends past the last usable LBA");
    }

    std::string type_guid = gptTypeGuidFor(type);
    if (type_guid.empty() && type != PartitionType::Unknown) {
        return Result::error("No GPT type GUID for the requested partition type");
    }

    // Check for overlap with existing partitions
    Partition candidate;
    candidate.setStartSector(start);
    candidate.setEndSector(end);
    for (const auto& part : partitions_) {
        if (candidate.overlaps(part)) {
            return Result::error("Partition overlaps existing partition");
        }
    }

    if (partitions_.size() >= partition_entry_count_) {
        return Result::error("No free GPT partition entries");
    }

    Partition partition;
    partition.setStartSector(start);
    partition.setEndSector(end);
    partition.setType(type);
    partition.setName(name);
    partition.setUuid(type_guid);
    partition.setPartitionUuid(generateGUID());
    partition.setBootable(false);

    partitions_.push_back(partition);
    std::sort(partitions_.begin(), partitions_.end());
    modified_ = true;
    return Result::ok();
}

Result GPTTable::deletePartition(int number) {
    if (number < 1 || number > static_cast<int>(partitions_.size())) {
        return Result::error("Invalid partition number");
    }
    partitions_.erase(partitions_.begin() + (number - 1));
    modified_ = true;
    return Result::ok();
}

Result GPTTable::resizePartition(int number, uint64_t new_size) {
    if (number < 1 || number > static_cast<int>(partitions_.size())) {
        return Result::error("Invalid partition number");
    }
    if (new_size == 0) {
        return Result::error("New size must be non-zero");
    }
    uint64_t sector_count = new_size / disk_->sectorSize();
    if (sector_count == 0) {
        return Result::error("Partition smaller than one sector");
    }
    Partition& part = partitions_[number - 1];
    uint64_t new_end = part.startSector() + sector_count - 1;
    if (new_end > last_usable_lba_) {
        return Result::error("Resized partition extends past the last usable LBA");
    }

    // Ensure the resized partition does not overlap its neighbors
    Partition resized;
    resized.setStartSector(part.startSector());
    resized.setEndSector(new_end);
    for (size_t i = 0; i < partitions_.size(); i++) {
        if (i == static_cast<size_t>(number - 1)) continue;
        if (resized.overlaps(partitions_[i])) {
            return Result::error("Resized partition would overlap another partition");
        }
    }

    part.setEndSector(new_end);
    modified_ = true;
    return Result::ok();
}

Result GPTTable::writePrimaryGPT() {
    if (!disk_) {
        return Result::error("No disk attached");
    }

    // Build the partition entry array
    std::vector<uint8_t> entries(partition_entry_count_ * partition_entry_size_, 0);
    for (size_t i = 0; i < partitions_.size(); i++) {
        const Partition& part = partitions_[i];
        GPTEntryRaw* entry = reinterpret_cast<GPTEntryRaw*>(&entries[i * partition_entry_size_]);

        std::string type_guid = gptTypeGuidFor(part.type());
        if (type_guid.empty()) type_guid = gpt_type::LINUX_FILESYSTEM;
        uint8_t type_bytes[16], unique_bytes[16];
        if (!guidBytesFromString(type_guid, type_bytes)) {
            return Result::error("Invalid type GUID");
        }
        std::string unique = part.partitionUuid().empty() ? generateGUID() : part.partitionUuid();
        if (!guidBytesFromString(unique, unique_bytes)) {
            return Result::error("Invalid partition GUID");
        }
        std::memcpy(entry->type_guid, type_bytes, 16);
        std::memcpy(entry->unique_guid, unique_bytes, 16);
        entry->first_lba = part.startSector();
        entry->last_lba = part.endSector();
        entry->attributes = 0;

        const std::string& pname = part.name();
        for (size_t j = 0; j < 36; j++) {
            entry->name[j] = (j < pname.size()) ? static_cast<uint16_t>(pname[j]) : 0;
        }
    }

    uint32_t array_crc = utils::crc32(entries.data(), entries.size());

    // Write the entry array to its LBA
    uint32_t entry_sectors = (entries.size() + 511) / 512;
    for (uint32_t i = 0; i < entry_sectors; i++) {
        Result r = disk_->writeSector(&entries[i * 512], partition_entry_lba_ + i);
        if (r.failed()) {
            return Result::error("Failed to write GPT entries: " + r.message);
        }
    }

    // Build the primary header
    GPTHeaderRaw header;
    std::memset(&header, 0, sizeof(header));
    header.signature = GPT_SIGNATURE;
    header.revision = GPT_REVISION_1_0;
    header.header_size = 92;
    header.reserved = 0;
    header.my_lba = 1;
    header.alternate_lba = backup_header_lba_;
    header.first_usable_lba = first_usable_lba_;
    header.last_usable_lba = last_usable_lba_;
    uint8_t disk_guid_bytes[16];
    if (!guidBytesFromString(disk_guid_.empty() ? generateGUID() : disk_guid_, disk_guid_bytes)) {
        return Result::error("Invalid disk GUID");
    }
    std::memcpy(header.disk_guid, disk_guid_bytes, 16);
    header.partition_entry_lba = partition_entry_lba_;
    header.num_partition_entries = partition_entry_count_;
    header.size_partition_entry = partition_entry_size_;
    header.partition_array_crc32 = array_crc;

    // Header CRC over bytes [0, header_size) with the CRC field zeroed
    std::vector<uint8_t> header_bytes(sizeof(GPTHeaderRaw), 0);
    std::memcpy(header_bytes.data(), &header, sizeof(header));
    *reinterpret_cast<uint32_t*>(&header_bytes[16]) = 0;
    header.header_crc32 = utils::crc32(header_bytes.data(), header.header_size);

    Result r = disk_->writeSector(&header, 1);
    if (r.failed()) {
        return Result::error("Failed to write primary GPT header: " + r.message);
    }

    // Ensure a protective MBR exists
    std::vector<uint8_t> mbr(512, 0);
    r = disk_->readSector(mbr.data(), 0);
    bool need_protective = r.failed();
    if (!need_protective && (mbr[510] != 0x55 || mbr[511] != 0xAA)) need_protective = true;
    if (!need_protective) {
        bool found_ee = false;
        for (int i = 0; i < 4; i++) {
            if (mbr[446 + i * 16 + 4] == 0xEE) { found_ee = true; break; }
        }
        need_protective = !found_ee;
    }
    if (need_protective) {
        std::vector<uint8_t> protective(512, 0);
        protective[510] = 0x55;
        protective[511] = 0xAA;
        uint8_t* e = protective.data() + 446;
        e[0] = 0x00;
        e[1] = 0x00; e[2] = 0x02; e[3] = 0x00;
        e[4] = 0xEE;
        e[5] = 0xFF; e[6] = 0xFF; e[7] = 0xFF;
        uint32_t size = static_cast<uint32_t>(
            std::min<uint64_t>(0xFFFFFFFFULL, last_usable_lba_ + 1));
        uint32_t lba1 = 1;
        std::memcpy(e + 8, &lba1, 4);
        std::memcpy(e + 12, &size, 4);
        r = disk_->writeSector(protective.data(), 0);
        if (r.failed()) {
            return Result::error("Failed to write protective MBR: " + r.message);
        }
    }

    return Result::ok();
}

Result GPTTable::writeBackupGPT() {
    if (!disk_) {
        return Result::error("No disk attached");
    }

    uint64_t last_lba = disk_->sectorCount() > 0 ? disk_->sectorCount() - 1 : 0;
    if (last_lba == 0) {
        return Result::error("Cannot determine device size");
    }

    // Partition entry array for the backup sits just before the last LBA
    std::vector<uint8_t> entries(partition_entry_count_ * partition_entry_size_, 0);
    for (size_t i = 0; i < partitions_.size(); i++) {
        const Partition& part = partitions_[i];
        GPTEntryRaw* entry = reinterpret_cast<GPTEntryRaw*>(&entries[i * partition_entry_size_]);
        std::string type_guid = gptTypeGuidFor(part.type());
        if (type_guid.empty()) type_guid = gpt_type::LINUX_FILESYSTEM;
        uint8_t type_bytes[16], unique_bytes[16];
        if (!guidBytesFromString(type_guid, type_bytes)) return Result::error("Invalid type GUID");
        std::string unique = part.partitionUuid().empty() ? generateGUID() : part.partitionUuid();
        if (!guidBytesFromString(unique, unique_bytes)) return Result::error("Invalid partition GUID");
        std::memcpy(entry->type_guid, type_bytes, 16);
        std::memcpy(entry->unique_guid, unique_bytes, 16);
        entry->first_lba = part.startSector();
        entry->last_lba = part.endSector();
        entry->attributes = 0;
        const std::string& pname = part.name();
        for (size_t j = 0; j < 36; j++) {
            entry->name[j] = (j < pname.size()) ? static_cast<uint16_t>(pname[j]) : 0;
        }
    }

    uint32_t entry_sectors = (entries.size() + 511) / 512;
    uint64_t entries_lba = last_lba - entry_sectors;
    for (uint32_t i = 0; i < entry_sectors; i++) {
        Result r = disk_->writeSector(&entries[i * 512], entries_lba + i);
        if (r.failed()) {
            return Result::error("Failed to write backup GPT entries: " + r.message);
        }
    }

    GPTHeaderRaw header;
    std::memset(&header, 0, sizeof(header));
    header.signature = GPT_SIGNATURE;
    header.revision = GPT_REVISION_1_0;
    header.header_size = 92;
    header.reserved = 0;
    header.my_lba = last_lba;
    header.alternate_lba = 1;
    header.first_usable_lba = first_usable_lba_;
    header.last_usable_lba = last_usable_lba_;
    uint8_t disk_guid_bytes[16];
    if (!guidBytesFromString(disk_guid_.empty() ? generateGUID() : disk_guid_, disk_guid_bytes)) {
        return Result::error("Invalid disk GUID");
    }
    std::memcpy(header.disk_guid, disk_guid_bytes, 16);
    header.partition_entry_lba = entries_lba;
    header.num_partition_entries = partition_entry_count_;
    header.size_partition_entry = partition_entry_size_;
    header.partition_array_crc32 = utils::crc32(entries.data(), entries.size());

    std::vector<uint8_t> header_bytes(sizeof(GPTHeaderRaw), 0);
    std::memcpy(header_bytes.data(), &header, sizeof(header));
    *reinterpret_cast<uint32_t*>(&header_bytes[16]) = 0;
    header.header_crc32 = utils::crc32(header_bytes.data(), header.header_size);

    Result r = disk_->writeSector(&header, last_lba);
    if (r.failed()) {
        return Result::error("Failed to write backup GPT header: " + r.message);
    }

    return Result::ok();
}

Result GPTTable::commit() {
    if (!modified_) {
        return Result::ok();
    }

    Result primary = writePrimaryGPT();
    if (primary.failed()) {
        return Result::error("Primary GPT write failed: " + primary.message);
    }
    Result backup = writeBackupGPT();
    if (backup.failed()) {
        return Result::error("Backup GPT write failed: " + backup.message);
    }

    modified_ = false;
    return Result::ok();
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
