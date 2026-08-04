#pragma once

#include "types.hpp"
#include <string>
#include <optional>

namespace opm {

// Represents a partition entry
class Partition {
public:
    Partition();
    
    // Getters
    int number() const { return number_; }
    std::string device() const { return device_; }
    std::string name() const { return name_; }
    std::string uuid() const { return uuid_; }
    std::string partitionUuid() const { return partition_uuid_; }
    
    sector_t startSector() const { return start_sector_; }
    sector_t endSector() const { return end_sector_; }
    uint64_t sectorCount() const { return end_sector_ - start_sector_ + 1; }
    uint64_t sizeBytes() const { return sectorCount() * sector_size_; }
    
    PartitionType type() const { return type_; }
    FileSystemType filesystem() const { return filesystem_; }
    bool isBootable() const { return bootable_; }
    bool isPrimary() const { return primary_; }
    bool isExtended() const { return extended_; }
    bool isLogical() const { return logical_; }
    bool isHidden() const { return hidden_; }
    
    // Alignment
    bool isAligned(size_t alignment_sectors = ALIGNMENT_1MB) const;
    
    // State
    bool isMounted() const;
    std::string mountPoint() const;
    
    // Setters (for creation/modification)
    void setStartSector(sector_t start) { start_sector_ = start; }
    void setEndSector(sector_t end) { end_sector_ = end; }
    void setType(PartitionType type) { type_ = type; }
    void setFilesystem(FileSystemType fs) { filesystem_ = fs; }
    void setBootable(bool bootable) { bootable_ = bootable; }
    void setHidden(bool hidden) { hidden_ = hidden; }
    void setName(const std::string& name) { name_ = name; }
    void setUuid(const std::string& uuid) { uuid_ = uuid; }
    void setPartitionUuid(const std::string& uuid) { partition_uuid_ = uuid; }
    
    // Validation
    bool isValid() const;
    bool overlaps(const Partition& other) const;
    
    // Format partition size for display
    std::string formattedSize() const;
    
    // Comparison
    bool operator<(const Partition& other) const {
        return start_sector_ < other.start_sector_;
    }
    
    bool operator==(const Partition& other) const {
        return device_ == other.device_ && number_ == other.number_;
    }
    
private:
    int number_ = 0;
    std::string device_;
    std::string name_;
    std::string uuid_;
    std::string partition_uuid_;
    
    sector_t start_sector_ = 0;
    sector_t end_sector_ = 0;
    uint32_t sector_size_ = 512;
    
    PartitionType type_ = PartitionType::Unknown;
    FileSystemType filesystem_ = FileSystemType::Unknown;
    
    bool bootable_ = false;
    bool primary_ = false;
    bool extended_ = false;
    bool logical_ = false;
    bool hidden_ = false;
    
    // GPT-specific
    uint64_t attributes_ = 0;
    
    friend class PartitionTable;
    friend class MBRTable;
    friend class GPTTable;
};

} // namespace opm
