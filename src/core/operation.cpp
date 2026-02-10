#include "opm/operation.hpp"
#include "opm/partition_table.hpp"
#include "opm/disk_io.hpp"
#include "opm/utils.hpp"
#include "opm/exceptions.hpp"
#include <sstream>

namespace opm {

// CreatePartitionOp implementation
CreatePartitionOp::CreatePartitionOp(uint64_t start_sector, uint64_t size_sectors,
                                      PartitionType type, const std::string& name)
    : start_sector_(start_sector), size_sectors_(size_sectors), type_(type), name_(name) {}

std::string CreatePartitionOp::description() const {
    std::ostringstream oss;
    oss << "Create partition at sector " << start_sector_ 
        << " with size " << size_sectors_ << " sectors"
        << " (type 0x" << std::hex << static_cast<int>(type_) << std::dec << ")";
    return oss.str();
}

Result CreatePartitionOp::validate(PartitionTable& table) const {
    // Check if start is aligned
    if (!utils::isAligned(start_sector_, ALIGNMENT_1MB)) {
        return Result::error("Start sector is not aligned to 1MB boundary");
    }
    
    // Check if partition fits within disk
    if (start_sector_ + size_sectors_ > table.diskIO()->sectorCount()) {
        return Result::error("Partition extends beyond disk");
    }
    
    // Check for overlaps with existing partitions
    uint64_t end_sector = start_sector_ + size_sectors_ - 1;
    for (const auto& part : table.getPartitions()) {
        if ((start_sector_ >= part.startSector() && start_sector_ <= part.endSector()) ||
            (end_sector >= part.startSector() && end_sector <= part.endSector()) ||
            (start_sector_ < part.startSector() && end_sector > part.endSector())) {
            return Result::error("Partition overlaps with existing partition " + 
                               std::to_string(part.number()));
        }
    }
    
    // Check if size is reasonable
    if (size_sectors_ < 2048) {
        return Result::error("Partition size must be at least 1MB");
    }
    
    return Result::ok();
}

Result CreatePartitionOp::execute(PartitionTable& table) {
    if (executed_) {
        return Result::error("Operation already executed");
    }
    
    auto result = table.createPartition(start_sector_, size_sectors_ * table.diskIO()->sectorSize(),
                                       type_, name_);
    if (result.failed()) {
        return result;
    }
    
    executed_ = true;
    return Result::ok();
}

Result CreatePartitionOp::undo(PartitionTable& table) {
    if (!executed_ || rolled_back_) {
        return Result::error("Operation not executed or already rolled back");
    }
    
    // Find and delete the partition we just created
    auto partitions = table.getPartitions();
    for (const auto& part : partitions) {
        if (part.startSector() == start_sector_) {
            auto result = table.deletePartition(part.number());
            if (result.failed()) {
                return result;
            }
            rolled_back_ = true;
            return Result::ok();
        }
    }
    
    return Result::error("Could not find partition to undo");
}

// DeletePartitionOp implementation
DeletePartitionOp::DeletePartitionOp(int partition_number)
    : partition_number_(partition_number) {}

std::string DeletePartitionOp::description() const {
    std::ostringstream oss;
    oss << "Delete partition " << partition_number_;
    return oss.str();
}

Result DeletePartitionOp::validate(PartitionTable& table) const {
    auto part = table.getPartition(partition_number_);
    if (!part.has_value()) {
        return Result::error("Partition " + std::to_string(partition_number_) + " not found");
    }
    
    // Check if partition is mounted
    if (part->isMounted()) {
        return Result::error("Partition is mounted");
    }
    
    return Result::ok();
}

Result DeletePartitionOp::execute(PartitionTable& table) {
    if (executed_) {
        return Result::error("Operation already executed");
    }
    
    auto part = table.getPartition(partition_number_);
    if (!part.has_value()) {
        return Result::error("Partition not found");
    }
    
    // Backup partition info for undo
    deleted_partition_ = part.value();
    has_backup_ = true;
    
    auto result = table.deletePartition(partition_number_);
    if (result.failed()) {
        return result;
    }
    
    executed_ = true;
    return Result::ok();
}

Result DeletePartitionOp::undo(PartitionTable& table) {
    if (!executed_ || rolled_back_ || !has_backup_) {
        return Result::error("Cannot undo: no backup available");
    }
    
    // Recreate the partition
    auto result = table.createPartition(
        deleted_partition_.startSector(),
        deleted_partition_.sizeBytes(),
        deleted_partition_.type(),
        deleted_partition_.name()
    );
    
    if (result.failed()) {
        return result;
    }
    
    rolled_back_ = true;
    return Result::ok();
}

// ResizePartitionOp implementation
ResizePartitionOp::ResizePartitionOp(int partition_number, uint64_t new_size_sectors)
    : partition_number_(partition_number), new_size_sectors_(new_size_sectors) {}

std::string ResizePartitionOp::description() const {
    std::ostringstream oss;
    oss << "Resize partition " << partition_number_ 
        << " to " << new_size_sectors_ << " sectors";
    return oss.str();
}

Result ResizePartitionOp::validate(PartitionTable& table) const {
    auto part = table.getPartition(partition_number_);
    if (!part.has_value()) {
        return Result::error("Partition " + std::to_string(partition_number_) + " not found");
    }
    
    // Check if partition is mounted
    if (part->isMounted()) {
        return Result::error("Cannot resize mounted partition");
    }
    
    uint64_t current_size = part->sectorCount();
    if (new_size_sectors_ == current_size) {
        return Result::error("New size is same as current size");
    }
    
    if (new_size_sectors_ < 2048) {
        return Result::error("New size must be at least 1MB");
    }
    
    // For shrinking, check if there's data in the area to be removed
    if (new_size_sectors_ < current_size) {
        // TODO: Check filesystem usage
    }
    
    // For growing, check if there's free space after partition
    if (new_size_sectors_ > current_size) {
        uint64_t new_end = part->startSector() + new_size_sectors_ - 1;
        for (const auto& other : table.getPartitions()) {
            if (other.number() != partition_number_ && 
                other.startSector() > part->endSector() &&
                other.startSector() <= new_end) {
                return Result::error("Not enough free space after partition");
            }
        }
    }
    
    return Result::ok();
}

Result ResizePartitionOp::execute(PartitionTable& table) {
    if (executed_) {
        return Result::error("Operation already executed");
    }
    
    auto part = table.getPartition(partition_number_);
    if (!part.has_value()) {
        return Result::error("Partition not found");
    }
    
    // Backup old bounds
    old_start_sector_ = part->startSector();
    old_end_sector_ = part->endSector();
    
    auto result = table.resizePartition(partition_number_, new_size_sectors_ * table.diskIO()->sectorSize());
    if (result.failed()) {
        return result;
    }
    
    executed_ = true;
    return Result::ok();
}

Result ResizePartitionOp::undo(PartitionTable& table) {
    if (!executed_ || rolled_back_) {
        return Result::error("Cannot undo: operation not executed");
    }
    
    // Restore original size
    uint64_t old_size = (old_end_sector_ - old_start_sector_ + 1) * table.diskIO()->sectorSize();
    auto result = table.resizePartition(partition_number_, old_size);
    
    if (result.failed()) {
        return result;
    }
    
    rolled_back_ = true;
    return Result::ok();
}

// OperationQueue implementation
OperationQueue::OperationQueue(std::shared_ptr<PartitionTable> table)
    : table_(table) {}

void OperationQueue::add(std::unique_ptr<Operation> op) {
    operations_.push_back(std::move(op));
}

void OperationQueue::clear() {
    operations_.clear();
    executed_count_ = 0;
}

Result OperationQueue::validate() const {
    if (!table_) {
        return Result::error("No partition table associated");
    }
    
    for (const auto& op : operations_) {
        auto result = op->validate(*table_);
        if (result.failed()) {
            return result;
        }
    }
    
    return Result::ok();
}

Result OperationQueue::commit() {
    if (!table_) {
        return Result::error("No partition table associated");
    }
    
    // Validate first
    auto validation = validate();
    if (validation.failed()) {
        return validation;
    }
    
    // Execute operations
    for (size_t i = executed_count_; i < operations_.size(); i++) {
        auto result = operations_[i]->execute(*table_);
        if (result.failed()) {
            // Rollback on failure
            rollback();
            return Result::error("Operation failed: " + result.message + 
                               ". Rolled back " + std::to_string(i) + " operations.");
        }
        executed_count_++;
    }
    
    return table_->commit();
}

Result OperationQueue::rollback() {
    if (!table_) {
        return Result::error("No partition table associated");
    }
    
    // Rollback in reverse order
    for (int i = static_cast<int>(executed_count_) - 1; i >= 0; i--) {
        operations_[i]->undo(*table_);
    }
    
    executed_count_ = 0;
    return Result::ok();
}

std::vector<std::string> OperationQueue::getOperationDescriptions() const {
    std::vector<std::string> descriptions;
    for (const auto& op : operations_) {
        descriptions.push_back(op->description());
    }
    return descriptions;
}

Result OperationQueue::preview(std::vector<std::string>& messages) const {
    if (!table_) {
        return Result::error("No partition table associated");
    }
    
    messages.clear();
    messages.push_back("Operation Preview:");
    messages.push_back("==================");
    
    for (size_t i = 0; i < operations_.size(); i++) {
        auto result = operations_[i]->validate(*table_);
        std::string status = result.success() ? "[OK]" : "[FAIL]";
        messages.push_back(std::to_string(i + 1) + ". " + status + " " + 
                          operations_[i]->description());
        if (result.failed()) {
            messages.push_back("   Error: " + result.message);
        }
    }
    
    return Result::ok();
}

// Transaction implementation
Transaction::Transaction(OperationQueue& queue) : queue_(queue) {}

Transaction::~Transaction() {
    if (!committed_ && !rolled_back_) {
        rollback();
    }
}

Result Transaction::commit() {
    if (committed_) {
        return Result::error("Transaction already committed");
    }
    
    auto result = queue_.commit();
    if (result.success()) {
        committed_ = true;
    }
    
    return result;
}

void Transaction::rollback() {
    if (!committed_ && !rolled_back_) {
        queue_.rollback();
        rolled_back_ = true;
    }
}

} // namespace opm
