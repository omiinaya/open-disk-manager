#pragma once

#include "types.hpp"
#include "partition_table.hpp"
#include <vector>
#include <memory>
#include <functional>

namespace opm {

// Forward declarations
class PartitionTable;

// Operation types
enum class OperationType {
    Create,
    Delete,
    Resize,
    Move,
    Format,
    Clone,
    Convert,
};

// Base class for partition operations
class Operation {
public:
    virtual ~Operation() = default;
    
    // Get operation type
    virtual OperationType type() const = 0;
    
    // Get operation description
    virtual std::string description() const = 0;
    
    // Validate the operation before execution
    virtual Result validate(PartitionTable& table) const = 0;
    
    // Execute the operation
    virtual Result execute(PartitionTable& table) = 0;
    
    // Undo the operation (rollback)
    virtual Result undo(PartitionTable& table) = 0;
    
    // Check if operation has been executed
    bool isExecuted() const { return executed_; }
    bool isRolledBack() const { return rolled_back_; }
    
protected:
    bool executed_ = false;
    bool rolled_back_ = false;
};

// Create partition operation
class CreatePartitionOp : public Operation {
public:
    CreatePartitionOp(uint64_t start_sector, uint64_t size_sectors, 
                      PartitionType type, const std::string& name = "");
    
    OperationType type() const override { return OperationType::Create; }
    std::string description() const override;
    Result validate(PartitionTable& table) const override;
    Result execute(PartitionTable& table) override;
    Result undo(PartitionTable& table) override;
    
private:
    uint64_t start_sector_;
    uint64_t size_sectors_;
    PartitionType type_;
    std::string name_;
    int created_partition_number_ = -1;
};

// Delete partition operation
class DeletePartitionOp : public Operation {
public:
    explicit DeletePartitionOp(int partition_number);
    
    OperationType type() const override { return OperationType::Delete; }
    std::string description() const override;
    Result validate(PartitionTable& table) const override;
    Result execute(PartitionTable& table) override;
    Result undo(PartitionTable& table) override;
    
private:
    int partition_number_;
    Partition deleted_partition_;
    bool has_backup_ = false;
};

// Resize partition operation
class ResizePartitionOp : public Operation {
public:
    ResizePartitionOp(int partition_number, uint64_t new_size_sectors);
    
    OperationType type() const override { return OperationType::Resize; }
    std::string description() const override;
    Result validate(PartitionTable& table) const override;
    Result execute(PartitionTable& table) override;
    Result undo(PartitionTable& table) override;
    
private:
    int partition_number_;
    uint64_t new_size_sectors_;
    uint64_t old_start_sector_ = 0;
    uint64_t old_end_sector_ = 0;
};

// Operation queue manager - provides transaction support
class OperationQueue {
public:
    OperationQueue() = default;
    explicit OperationQueue(std::shared_ptr<PartitionTable> table);
    
    // Add operation to queue
    void add(std::unique_ptr<Operation> op);
    
    // Get number of pending operations
    size_t size() const { return operations_.size(); }
    bool empty() const { return operations_.empty(); }
    
    // Clear all operations
    void clear();
    
    // Validate all operations
    Result validate() const;
    
    // Execute all operations (commit)
    Result commit();
    
    // Rollback all executed operations
    Result rollback();
    
    // Get list of operations
    std::vector<std::string> getOperationDescriptions() const;
    
    // Preview what would happen (dry run)
    Result preview(std::vector<std::string>& messages) const;
    
private:
    std::shared_ptr<PartitionTable> table_;
    std::vector<std::unique_ptr<Operation>> operations_;
    size_t executed_count_ = 0;
};

// Transaction guard - RAII for operations
class Transaction {
public:
    explicit Transaction(OperationQueue& queue);
    ~Transaction();
    
    // Prevent copy
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    
    // Allow move
    Transaction(Transaction&&) = default;
    Transaction& operator=(Transaction&&) = default;
    
    // Commit the transaction
    Result commit();
    
    // Rollback (called automatically on destruction if not committed)
    void rollback();
    
    // Check if committed
    bool isCommitted() const { return committed_; }
    
private:
    OperationQueue& queue_;
    bool committed_ = false;
    bool rolled_back_ = false;
};

} // namespace opm
