#pragma once

#include "types.hpp"
#include <vector>
#include <string>

namespace opm {

// ============================================================================
// RAID Types
// ============================================================================

enum class RaidLevel {
    Linear,     // JBOD
    Raid0,      // Striping
    Raid1,      // Mirroring
    Raid4,      // Dedicated parity
    Raid5,      // Distributed parity
    Raid6,      // Dual parity
    Raid10,     // Striped mirrors
    Unknown
};

enum class RaidStatus {
    Active,     // Normal operation
    Degraded,   // One or more drives failed
    Resyncing,  // Resyncing after failure
    Recovering, // Recovering from failure
    Failed,     // Cannot operate
    Unknown
};

// ============================================================================
// RAID Component
// ============================================================================

struct RaidComponent {
    std::string device_path;
    std::string uuid;
    uint64_t size;
    bool is_active;
    bool is_syncing;
    int sync_percentage;
    uint64_t sync_remaining;
    int role;  // Position in array
};

// ============================================================================
// RAID Array
// ============================================================================

struct RaidArray {
    std::string name;
    std::string device_path;
    RaidLevel level;
    RaidStatus status;
    uint64_t total_size;
    uint64_t used_size;
    uint64_t chunk_size;
    std::vector<RaidComponent> components;
    int min_drives;
    int total_drives;
    int active_drives;
    int failed_drives;
    int spare_drives;
    std::string uuid;
    uint32_t bitmap_pages;
    uint32_t bitmap_chunks;
};

// ============================================================================
// RAID Operations
// ============================================================================

// Detect RAID arrays
std::vector<RaidArray> detectRaidArrays();

// Get RAID array info
Result getRaidArrayInfo(const std::string& device_path, RaidArray& array);

// Check if device is part of a RAID array
bool isRaidMember(const std::string& device_path);

// Get RAID arrays containing this device
std::vector<RaidArray> getRaidArraysForDevice(const std::string& device_path);

// Create RAID array
Result createRaidArray(const std::string& name, RaidLevel level,
                       const std::vector<std::string>& devices,
                       uint64_t chunk_size);

// Delete RAID array
Result deleteRaidArray(const std::string& name);

// Add device to RAID array
Result addDeviceToRaid(const std::string& array_name, 
                       const std::string& device_path,
                       bool as_spare = false);

// Remove device from RAID array
Result removeDeviceFromRaid(const std::string& array_name,
                            const std::string& device_path);

// Replace failed device
Result replaceRaidDevice(const std::string& array_name,
                         const std::string& failed_device,
                         const std::string& new_device);

// Start RAID array
Result startRaidArray(const std::string& name);

// Stop RAID array
Result stopRaidArray(const std::string& name);

// Resync RAID array (force rebuild)
Result resyncRaidArray(const std::string& name);

// Check RAID array status
Result checkRaidStatus(const std::string& name, RaidStatus& status);

// Grow RAID array (add more drives)
Result growRaidArray(const std::string& name,
                     const std::vector<std::string>& new_devices);

// Shrink RAID array (remove drives)
Result shrinkRaidArray(const std::string& name,
                       const std::vector<std::string>& devices_to_remove);

// Change RAID level
Result changeRaidLevel(const std::string& name, RaidLevel new_level);

// ============================================================================
// RAID Configuration
// ============================================================================

// Save RAID configuration to mdadm.conf
Result saveRaidConfig(const std::string& config_path = "/etc/mdadm/mdadm.conf");

// Load RAID configuration
Result loadRaidConfig(const std::string& config_path = "/etc/mdadm/mdadm.conf");

// ============================================================================
// RAID Monitoring
// ============================================================================

struct RaidEvent {
    std::string array_name;
    std::string event_type;
    std::string device_path;
    std::string timestamp;
    std::string message;
};

// Monitor RAID events
std::vector<RaidEvent> getRaidEvents(const std::string& array_name = "");

// Set up RAID monitoring
Result setupRaidMonitoring(std::function<void(const RaidEvent&)> callback);

// ============================================================================
// RAID Superblock
// ============================================================================

// Read RAID superblock
Result readRaidSuperblock(const std::string& device_path, 
                          std::vector<uint8_t>& superblock);

// Write RAID superblock
Result writeRaidSuperblock(const std::string& device_path,
                           const std::vector<uint8_t>& superblock);

} // namespace opm
