#include "opm/raid.hpp"
#include "opm/disk_io.hpp"
#include <cstring>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cstdio>

namespace opm {

namespace {

// mdadm superblock version 1.x magic
constexpr uint64_t MD_SB_MAGIC = 0xA92B4EF9ULL;
// mdadm superblock 0.90 magic
constexpr uint32_t MD_SB_MAGIC_OLD = 0x92A4B89C;

RaidLevel raidLevelFromName(const std::string& n) {
    if (n == "raid0" || n == "0") return RaidLevel::Raid0;
    if (n == "raid1" || n == "1") return RaidLevel::Raid1;
    if (n == "raid4" || n == "4") return RaidLevel::Raid4;
    if (n == "raid5" || n == "5") return RaidLevel::Raid5;
    if (n == "raid6" || n == "6") return RaidLevel::Raid6;
    if (n == "raid10" || n == "10") return RaidLevel::Raid10;
    if (n == "linear") return RaidLevel::Linear;
    return RaidLevel::Unknown;
}

RaidStatus raidStatusFromFlags(const std::string& flags) {
    if (flags.find("resync") != std::string::npos ||
        flags.find("recover") != std::string::npos) {
        return RaidStatus::Resyncing;
    }
    if (flags.find("failed") != std::string::npos) {
        return RaidStatus::Failed;
    }
    if (flags.find("degraded") != std::string::npos ||
        flags.find("F") != std::string::npos) {
        return RaidStatus::Degraded;
    }
    return RaidStatus::Active;
}

// Read a device and look for an mdadm superblock. Returns true + fills the
// array name when found.
bool scanForMDSuperblock(const std::string& path, std::string& array_name) {
    auto disk = DiskIO::openReadOnly(path);
    if (!disk || !disk->isOpen()) return false;
    uint64_t sectors = disk->sectorCount();
    if (sectors == 0) return false;

    // v1.0 superblock: 8 KiB from the end of the device
    if (sectors > 32) {
        std::vector<uint8_t> block(1024, 0);
        uint64_t read_sector = sectors - 16;
        if (disk->readSector(block.data(), read_sector).success()) {
            uint64_t magic = 0;
            std::memcpy(&magic, block.data(), 8);
            if (magic == MD_SB_MAGIC) {
                // v1.x superblock layout: magic(8) major(4) minor(4) ...
                // set name at offset 320 (64 bytes, NUL-terminated)
                char name[65] = {0};
                std::memcpy(name, block.data() + 320, 64);
                array_name = name;
                return true;
            }
        }
    }
    return false;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Detection
// ---------------------------------------------------------------------------

std::vector<RaidArray> detectRaidArrays() {
    std::vector<RaidArray> arrays;

#ifdef __linux__
    // Primary source: /proc/mdstat (real, authoritative for software RAID)
    std::ifstream mdstat("/proc/mdstat");
    std::string line;
    RaidArray current;
    bool in_array = false;

    while (std::getline(mdstat, line)) {
        // A new array starts with "mdX : active raid1 sda1 sdb1..."
        if (!line.empty() && line[0] == 'm') {
            if (in_array) arrays.push_back(current);
            current = RaidArray();
            in_array = false;

            size_t colon = line.find(':');
            if (colon == std::string::npos) continue;
            current.name = line.substr(0, colon);
            // "active raid5 sda1 sdb1 ..." or "inactive"
            std::string rest = line.substr(colon + 1);
            std::stringstream ss(rest);
            std::string word;
            ss >> word;  // active/inactive
            if (word == "inactive") {
                current.status = RaidStatus::Unknown;
            } else {
                current.status = RaidStatus::Active;
            }
            ss >> word;  // raid level
            if (word.find("raid") == 0) {
                current.level = raidLevelFromName(word.substr(4));
            } else {
                current.level = raidLevelFromName(word);
            }
            // remaining words on this line are component devices
            std::vector<std::string> comps;
            while (ss >> word) comps.push_back(word);
            for (const auto& c : comps) {
                if (c.find("/dev/") == 0 || c.find("sd") == 0 ||
                    c.find("hd") == 0 || c.find("nvme") == 0 ||
                    c.find("vd") == 0) {
                    RaidComponent comp;
                    comp.device_path = c;
                    comp.is_active = true;
                    current.components.push_back(comp);
                    current.total_drives++;
                }
            }
            current.device_path = "/dev/" + current.name;
            in_array = true;
        } else if (in_array) {
            // Continuation lines carry flags/status ("     5054976 blocks ...")
            std::stringstream ss(line);
            std::string word;
            while (ss >> word) {
                if (word == "resync" || word == "recover") {
                    current.status = RaidStatus::Resyncing;
                } else if (word == "degraded") {
                    current.status = RaidStatus::Degraded;
                }
            }
        }
    }
    if (in_array) arrays.push_back(current);

    // Complement with a raw superblock scan of block devices (covers arrays
    // that are not assembled / not listed in /proc/mdstat).
    const std::string sys_block = "/sys/block";
    if (std::filesystem::exists(sys_block)) {
        for (const auto& entry : std::filesystem::directory_iterator(sys_block)) {
            std::string name = entry.path().filename().string();
            if (name.find("loop") == 0 || name.find("ram") == 0 ||
                name.find("dm-") == 0 || name.find("md") == 0) {
                continue;
            }
            std::string dev = "/dev/" + name;
            std::string array_name;
            if (scanForMDSuperblock(dev, array_name)) {
                // Skip if /proc/mdstat already reported this array
                bool known = false;
                for (const auto& a : arrays) {
                    if (a.components.empty()) continue;
                    for (const auto& c : a.components) {
                        if (c.device_path == dev) { known = true; break; }
                    }
                    if (known) break;
                }
                if (!known) {
                    RaidArray arr;
                    arr.name = array_name.empty() ? "(unassigned)" : array_name;
                    arr.status = RaidStatus::Unknown;
                    RaidComponent comp;
                    comp.device_path = dev;
                    arr.components.push_back(comp);
                    arr.total_drives = 1;
                    arrays.push_back(arr);
                }
            }
        }
    }
#endif

    return arrays;
}

Result getRaidArrayInfo(const std::string& device_path, RaidArray& array) {
    for (const auto& a : detectRaidArrays()) {
        if (a.device_path == device_path || ("/dev/" + a.name) == device_path) {
            array = a;
            return Result::ok();
        }
    }
    return Result::error("No RAID array found at " + device_path);
}

bool isRaidMember(const std::string& device_path) {
    for (const auto& a : detectRaidArrays()) {
        for (const auto& c : a.components) {
            if (c.device_path == device_path) return true;
        }
    }
    return false;
}

std::vector<RaidArray> getRaidArraysForDevice(const std::string& device_path) {
    std::vector<RaidArray> result;
    for (const auto& a : detectRaidArrays()) {
        for (const auto& c : a.components) {
            if (c.device_path == device_path) {
                result.push_back(a);
                break;
            }
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// Operations - delegated to mdadm with honest errors.
// ---------------------------------------------------------------------------

namespace {
Result runMdadm(const std::string& args) {
    int rc = std::system(("mdadm " + args + " 2>/dev/null").c_str());
    if (rc != 0) {
        return Result::error("mdadm failed (exit " + std::to_string(rc) +
                             "): mdadm " + args);
    }
    return Result::ok();
}
} // namespace

Result createRaidArray(const std::string& name, RaidLevel level,
                       const std::vector<std::string>& devices,
                       uint64_t chunk_size) {
    if (devices.empty()) return Result::error("No devices provided");
    std::string level_name;
    switch (level) {
        case RaidLevel::Raid0: level_name = "0"; break;
        case RaidLevel::Raid1: level_name = "1"; break;
        case RaidLevel::Raid5: level_name = "5"; break;
        case RaidLevel::Raid6: level_name = "6"; break;
        case RaidLevel::Raid10: level_name = "10"; break;
        default: return Result::error("Unsupported RAID level for creation");
    }
    std::string cmd = "--create /dev/" + name + " --level=" + level_name +
                      " --chunk=" + std::to_string(chunk_size / 1024);
    for (const auto& d : devices) cmd += " " + d;
    return runMdadm(cmd);
}

Result deleteRaidArray(const std::string& name) {
    return runMdadm("--stop /dev/" + name);
}

Result addDeviceToRaid(const std::string& array_name,
                       const std::string& device_path, bool as_spare) {
    std::string cmd = "--add /dev/" + array_name + " " + device_path;
    if (as_spare) cmd += " --spare";
    return runMdadm(cmd);
}

Result removeDeviceFromRaid(const std::string& array_name,
                            const std::string& device_path) {
    return runMdadm("--remove /dev/" + array_name + " " + device_path);
}

Result replaceRaidDevice(const std::string& array_name,
                         const std::string& failed_device,
                         const std::string& new_device) {
    Result r = runMdadm("--remove /dev/" + array_name + " " + failed_device);
    if (r.failed()) return r;
    return runMdadm("--add /dev/" + array_name + " " + new_device);
}

Result startRaidArray(const std::string& name) {
    return runMdadm("--assemble /dev/" + name);
}

Result stopRaidArray(const std::string& name) {
    return runMdadm("--stop /dev/" + name);
}

Result resyncRaidArray(const std::string& name) {
    return runMdadm("--assemble --force /dev/" + name);
}

Result checkRaidStatus(const std::string& name, RaidStatus& status) {
    for (const auto& a : detectRaidArrays()) {
        if (a.name == name || ("/dev/" + a.name) == name) {
            status = a.status;
            return Result::ok();
        }
    }
    return Result::error("Array not found: " + name);
}

Result growRaidArray(const std::string& name,
                     const std::vector<std::string>& new_devices) {
    std::string cmd = "--grow /dev/" + name;
    for (const auto& d : new_devices) cmd += " " + d;
    return runMdadm(cmd);
}

Result shrinkRaidArray(const std::string& name,
                       const std::vector<std::string>& devices_to_remove) {
    std::string cmd = "--grow /dev/" + name + " --raid-devices=1";
    for (const auto& d : devices_to_remove) cmd += " --remove " + d;
    return runMdadm(cmd);
}

Result changeRaidLevel(const std::string& name, RaidLevel /*new_level*/) {
    return Result::error(
        "Changing RAID level requires a reshape; run mdadm --grow manually "
        "with the desired --level");
}

Result saveRaidConfig(const std::string& config_path) {
    return runMdadm("--detail --scan > " + config_path);
}

Result loadRaidConfig(const std::string& config_path) {
    return runMdadm("--assemble --scan --config=" + config_path);
}

std::vector<RaidEvent> getRaidEvents(const std::string& /*array_name*/) {
    // mdadm does not persist events by default; events are only observable
    // live. Return empty (no fake data).
    return {};
}

Result setupRaidMonitoring(std::function<void(const RaidEvent&)> /*callback*/) {
    return Result::error(
        "RAID monitoring requires the mdadm monitor daemon; run "
        "'mdadm --monitor --daemonise' to enable it");
}

Result readRaidSuperblock(const std::string& device_path,
                          std::vector<uint8_t>& superblock) {
    auto disk = DiskIO::openReadOnly(device_path);
    if (!disk || !disk->isOpen()) {
        return Result::error("Cannot open device: " + device_path);
    }
    uint64_t sectors = disk->sectorCount();
    if (sectors <= 32) {
        return Result::error("Device too small to hold a superblock");
    }
    superblock.resize(4096);
    // v1.0 superblock: 8 KiB from the end
    Result r = disk->read(superblock.data(), (sectors - 16) * 512, 4096);
    if (r.failed()) return r;

    uint64_t magic = 0;
    std::memcpy(&magic, superblock.data(), 8);
    if (magic != MD_SB_MAGIC) {
        return Result::error("No mdadm v1.x superblock found");
    }
    return Result::ok();
}

Result writeRaidSuperblock(const std::string& device_path,
                           const std::vector<uint8_t>& superblock) {
    auto disk = DiskIO::openReadWrite(device_path);
    if (!disk || !disk->isOpen()) {
        return Result::error("Cannot open device read-write: " + device_path);
    }
    uint64_t sectors = disk->sectorCount();
    if (sectors <= 32) {
        return Result::error("Device too small to hold a superblock");
    }
    return disk->write(superblock.data(), (sectors - 16) * 512,
                       superblock.size());
}

} // namespace opm
