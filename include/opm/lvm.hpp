#pragma once

#include "types.hpp"
#include <vector>
#include <string>

namespace opm {

enum class VolumeType {
    Linear, Striped, Mirrored, Raid0, Raid1, Raid4, Raid5,
    Raid6, Raid10, Thin, ThinPool, Snapshot, Cache, CachePool, Unknown
};

struct PhysicalVolume {
    std::string device_path;
    std::string vg_name;
    std::string uuid;
    uint64_t total_size;
    uint64_t free_size;
    uint64_t extent_size;
    uint64_t extent_count;
    uint64_t free_extent_count;
    bool active;
    bool allocatable;
    int pe_start;
};

struct VolumeGroup {
    std::string name;
    std::string uuid;
    uint64_t total_size;
    uint64_t free_size;
    uint64_t extent_size;
    uint64_t extent_count;
    uint64_t free_extent_count;
    int pv_count;
    int lv_count;
    int snap_count;
    std::vector<std::string> pv_names;
    bool active;
    bool clustered;
    bool exportable;
    bool partial;
};

struct LogicalVolume {
    std::string name;
    std::string vg_name;
    std::string device_path;
    std::string uuid;
    VolumeType type;
    uint64_t size;
    uint64_t extents;
    bool active;
    bool open;
    int stripe_count;
    uint64_t stripe_size;
};

// Detection
std::vector<PhysicalVolume> detectPhysicalVolumes();
std::vector<VolumeGroup> detectVolumeGroups();
std::vector<LogicalVolume> detectLogicalVolumes();

// Operations
Result createVolumeGroup(const std::string& name, const std::vector<std::string>& devices);
Result removeVolumeGroup(const std::string& name);
Result createLogicalVolume(const std::string& vg_name, const std::string& lv_name, uint64_t size);
Result removeLogicalVolume(const std::string& vg_name, const std::string& lv_name);
Result extendLogicalVolume(const std::string& vg_name, const std::string& lv_name, uint64_t additional_size);
Result reduceLogicalVolume(const std::string& vg_name, const std::string& lv_name, uint64_t new_size);

} // namespace opm
