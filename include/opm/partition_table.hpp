#pragma once

#include "types.hpp"
#include "partition.hpp"
#include <vector>
#include <memory>
#include <optional>

namespace opm {

// Forward declarations
class DiskIO;

// Partition table type
enum class TableType {
    Unknown,
    MBR,
    GPT,
    APM,      // Apple Partition Map
    BSD,      // BSD disklabel
    Sun,      // Sun partition table
    SGI,      // SGI partition table
    Hybrid,   // MBR + GPT
};

// Abstract base class for partition tables
class PartitionTable {
public:
    virtual ~PartitionTable() = default;
    
    // Factory method to detect and load partition table
    static std::unique_ptr<PartitionTable> load(const std::string& device_path);
    static std::unique_ptr<PartitionTable> load(std::shared_ptr<DiskIO> disk);
    
    // Pure virtual methods
    virtual TableType type() const = 0;
    virtual std::string typeName() const = 0;
    virtual bool isValid() const = 0;
    
    // Partition access
    virtual std::vector<Partition> getPartitions() const = 0;
    virtual std::optional<Partition> getPartition(int number) const;
    virtual int getPartitionCount() const = 0;
    
    // Partition operations
    virtual Result createPartition(uint64_t start, uint64_t size, 
                                   PartitionType type,
                                   const std::string& name = "") = 0;
    virtual Result deletePartition(int number) = 0;
    virtual Result resizePartition(int number, uint64_t new_size) = 0;
    
    // Information
    virtual uint64_t getFreeSpace() const;
    virtual uint64_t getUsedSpace() const;
    virtual uint64_t getTotalSpace() const = 0;
    
    // Validation
    virtual Result validate() const = 0;
    virtual bool hasErrors() const = 0;
    virtual std::vector<std::string> getErrors() const = 0;
    
    // Disk information
    std::string devicePath() const { return device_path_; }
    std::shared_ptr<DiskIO> diskIO() const { return disk_; }
    
    // Is the table modified (needs write)
    bool isModified() const { return modified_; }
    void markModified() { modified_ = true; }
    void clearModified() { modified_ = false; }
    
    // Write changes to disk
    virtual Result commit() = 0;
    
    // Revert changes
    virtual void revert() = 0;
    
    // Conversion
    virtual bool canConvertTo(TableType type) const;
    virtual Result convertTo(TableType type) = 0;
    
protected:
    std::string device_path_;
    std::shared_ptr<DiskIO> disk_;
    bool modified_ = false;
    std::vector<Partition> partitions_;
    std::vector<std::string> errors_;
};

// MBR partition table
class MBRTable : public PartitionTable {
public:
    MBRTable();
    explicit MBRTable(std::shared_ptr<DiskIO> disk);
    
    TableType type() const override { return TableType::MBR; }
    std::string typeName() const override { return "MBR"; }
    bool isValid() const override;
    
    std::vector<Partition> getPartitions() const override;
    int getPartitionCount() const override;
    
    Result createPartition(uint64_t start, uint64_t size,
                          PartitionType type,
                          const std::string& name = "") override;
    Result deletePartition(int number) override;
    Result resizePartition(int number, uint64_t new_size) override;
    
    uint64_t getTotalSpace() const override;
    
    Result validate() const override;
    bool hasErrors() const override;
    std::vector<std::string> getErrors() const override;
    
    Result commit() override;
    void revert() override;
    
    Result convertTo(TableType type) override;
    
    // MBR-specific
    bool hasExtendedPartition() const;
    std::vector<Partition> getLogicalPartitions() const;
    uint32_t getDiskSignature() const { return disk_signature_; }
    
private:
    void loadFromDisk();
    void parsePartitionEntry(const uint8_t* entry, int index);
    bool isExtendedPartitionType(uint8_t type) const;
    
    // MBR data
    uint32_t disk_signature_ = 0;
    uint8_t boot_code_[440] = {};
    
    // Extended partition handling
    uint64_t extended_start_ = 0;
    void loadExtendedPartitions();
    
    struct MBRPartitionEntry {
        uint8_t status;
        uint8_t start_head;
        uint8_t start_sector;
        uint8_t start_cylinder;
        uint8_t type;
        uint8_t end_head;
        uint8_t end_sector;
        uint8_t end_cylinder;
        uint32_t start_lba;
        uint32_t sector_count;
    };
    
    std::vector<MBRPartitionEntry> mbr_entries_;
};

// GPT partition table
class GPTTable : public PartitionTable {
public:
    GPTTable();
    explicit GPTTable(std::shared_ptr<DiskIO> disk);
    
    TableType type() const override { return TableType::GPT; }
    std::string typeName() const override { return "GPT"; }
    bool isValid() const override;
    
    std::vector<Partition> getPartitions() const override;
    int getPartitionCount() const override;
    
    Result createPartition(uint64_t start, uint64_t size,
                          PartitionType type,
                          const std::string& name = "") override;
    Result deletePartition(int number) override;
    Result resizePartition(int number, uint64_t new_size) override;
    
    uint64_t getTotalSpace() const override;
    
    Result validate() const override;
    bool hasErrors() const override;
    std::vector<std::string> getErrors() const override;
    
    Result commit() override;
    void revert() override;
    
    Result convertTo(TableType type) override;
    
    // GPT-specific
    std::string getDiskGuid() const { return disk_guid_; }
    uint64_t getFirstUsableLBA() const { return first_usable_lba_; }
    uint64_t getLastUsableLBA() const { return last_usable_lba_; }
    uint32_t getPartitionEntrySize() const { return partition_entry_size_; }
    uint32_t getPartitionEntryCount() const { return partition_entry_count_; }
    
    bool isUEFISystem() const;
    bool hasProtectiveMBR() const;
    
    // Backup GPT
    Result restoreFromBackup();
    Result createBackup();
    
private:
    void loadFromDisk();
    void parseGPTHeader(const uint8_t* data);
    void parsePartitionEntries();
    Result writePrimaryGPT();
    Result writeBackupGPT();
    
    // GPT header
    uint64_t header_lba_ = 0;
    uint64_t backup_header_lba_ = 0;
    uint64_t first_usable_lba_ = 0;
    uint64_t last_usable_lba_ = 0;
    std::string disk_guid_;
    uint64_t partition_entry_lba_ = 0;
    uint32_t partition_entry_count_ = 0;
    uint32_t partition_entry_size_ = 0;
    uint32_t header_crc32_ = 0;
    uint32_t partition_array_crc32_ = 0;
    
    uint32_t revision_ = 0;
    
    bool has_protective_mbr_ = false;
    
    // Calculate CRC32
    static uint32_t calculateCRC32(const uint8_t* data, size_t length);
};

} // namespace opm
