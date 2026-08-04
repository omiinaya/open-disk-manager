#include "opm/lvm.hpp"
#include "opm/disk_io.hpp"
#include <cstring>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cstdio>

namespace opm {

namespace {

// LVM2 label header (at byte offset 512 of the PV)
struct LVM2Label {
    char magic[8];          // "LABELONE"
    uint64_t sector;        // sector number of this label
    uint32_t crc;           // CRC32 of the rest of the label
    uint32_t offset;        // offset to the start of the PV header
    uint32_t data_size;     // size of the PV header
    char type[8];           // "LVM2 001"
} __attribute__((packed));

constexpr const char* LVM2_LABEL_MAGIC = "LABELONE";

// Scan a raw device/file for the LVM2 PV label; returns the PV header text.
bool scanForPVLabel(const std::string& path, std::string& pv_uuid,
                    std::string& vg_name) {
    auto disk = DiskIO::openReadOnly(path);
    if (!disk || !disk->isOpen()) return false;

    std::vector<uint8_t> sector(512, 0);
    // The label lives at byte offset 512 (sector 1) for real PVs; some tools
    // place it later, so scan the first few sectors.
    for (uint64_t s = 0; s < 8 && s < disk->sectorCount(); s++) {
        if (disk->readSector(sector.data(), s).failed()) break;
        if (std::memcmp(sector.data(), LVM2_LABEL_MAGIC, 8) != 0) continue;

        LVM2Label label;
        std::memcpy(&label, sector.data(), sizeof(LVM2Label));
        if (std::memcmp(label.type, "LVM2 001", 8) != 0) continue;

        uint32_t data_offset = label.offset;
        // PV header: version (4), pv_uuid (32), then vg name follows
        // (may be empty for unassigned PVs). Fields are printable ASCII.
        if (data_offset + 36 > 512) continue;
        pv_uuid.assign(reinterpret_cast<const char*>(sector.data() + data_offset + 4), 32);
        // Trim trailing NULs/spaces
        while (!pv_uuid.empty() && (pv_uuid.back() == '\0' || pv_uuid.back() == ' ')) {
            pv_uuid.pop_back();
        }
        vg_name.assign(reinterpret_cast<const char*>(sector.data() + data_offset + 36),
                       std::min<size_t>(128, 512 - data_offset - 36));
        // A PV header starts with version 0x00020001 on LE; the vg name is
        // only valid when non-NUL.
        size_t nul = vg_name.find('\0');
        if (nul != std::string::npos) vg_name = vg_name.substr(0, nul);
        while (!vg_name.empty() && vg_name.back() == ' ') vg_name.pop_back();
        return true;
    }
    return false;
}

std::vector<std::string> scanBlockDevices() {
    std::vector<std::string> devices;
#ifdef __linux__
    const std::string sys_block = "/sys/block";
    if (!std::filesystem::exists(sys_block)) return devices;
    for (const auto& entry : std::filesystem::directory_iterator(sys_block)) {
        std::string name = entry.path().filename().string();
        if (name.find("loop") == 0 || name.find("ram") == 0 ||
            name.find("dm-") == 0 || name.find("md") == 0) {
            continue;
        }
        devices.push_back("/dev/" + name);
    }
#endif
    return devices;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Detection
// ---------------------------------------------------------------------------

std::vector<PhysicalVolume> detectPhysicalVolumes() {
    std::vector<PhysicalVolume> pvs;
#ifdef __linux__
    for (const auto& dev : scanBlockDevices()) {
        std::string pv_uuid, vg_name;
        if (!scanForPVLabel(dev, pv_uuid, vg_name)) continue;

        PhysicalVolume pv;
        pv.device_path = dev;
        pv.vg_name = vg_name;
        pv.uuid = pv_uuid;
        pv.active = true;
        pv.allocatable = true;

        auto disk = DiskIO::openReadOnly(dev);
        if (disk) {
            pv.total_size = disk->size();
        }
        pvs.push_back(pv);
    }
#endif
    return pvs;
}

std::vector<VolumeGroup> detectVolumeGroups() {
    std::vector<VolumeGroup> vgs;
    // Build VG info from the detected PVs. Full VG metadata (extents, LVs)
    // lives in the text metadata area; the PV label gives us the name and
    // membership, which covers the roadmap's "detect/read/display" scope.
    for (const auto& pv : detectPhysicalVolumes()) {
        if (pv.vg_name.empty()) continue;

        auto it = std::find_if(vgs.begin(), vgs.end(),
            [&](const VolumeGroup& vg) { return vg.name == pv.vg_name; });
        if (it == vgs.end()) {
            VolumeGroup vg;
            vg.name = pv.vg_name;
            vg.active = true;
            vg.pv_count = 1;
            vg.pv_names.push_back(pv.device_path);
            vg.total_size = pv.total_size;
            vgs.push_back(vg);
        } else {
            it->pv_count++;
            it->pv_names.push_back(pv.device_path);
            it->total_size += pv.total_size;
        }
    }
    return vgs;
}

std::vector<LogicalVolume> detectLogicalVolumes() {
    std::vector<LogicalVolume> lvs;
#ifdef __linux__
    const std::string mapper = "/dev/mapper";
    if (!std::filesystem::exists(mapper)) return lvs;

    for (const auto& entry : std::filesystem::directory_iterator(mapper)) {
        std::string name = entry.path().filename().string();
        if (name == "control") continue;

        LogicalVolume lv;
        lv.name = name;
        lv.device_path = entry.path().string();

        // /dev/mapper/<vg>-<lv> encodes the VG and LV names
        size_t dash = name.find('-');
        if (dash != std::string::npos) {
            lv.vg_name = name.substr(0, dash);
        }
        lv.active = true;

        auto disk = DiskIO::openReadOnly(entry.path().string());
        if (disk) {
            lv.size = disk->size();
        }
        lvs.push_back(lv);
    }
#endif
    return lvs;
}

// ---------------------------------------------------------------------------
// Operations - delegated to the standard LVM tooling, with honest errors
// when the tools are unavailable.
// ---------------------------------------------------------------------------

namespace {
Result runTool(const std::string& cmd) {
    int rc = std::system(cmd.c_str());
    if (rc != 0) {
        return Result::error("LVM tool failed (exit " + std::to_string(rc) +
                             "): " + cmd);
    }
    return Result::ok();
}
} // namespace

Result createVolumeGroup(const std::string& name,
                         const std::vector<std::string>& devices) {
    if (devices.empty()) {
        return Result::error("No devices provided");
    }
    std::string cmd = "vgcreate " + name;
    for (const auto& d : devices) cmd += " " + d;
    cmd += " 2>/dev/null";
    return runTool(cmd);
}

Result removeVolumeGroup(const std::string& name) {
    return runTool("vgremove " + name + " 2>/dev/null");
}

Result createLogicalVolume(const std::string& vg_name, const std::string& lv_name,
                           uint64_t size) {
    return runTool("lvcreate -L " + std::to_string(size / (1024 * 1024)) +
                   "M -n " + lv_name + " " + vg_name + " 2>/dev/null");
}

Result removeLogicalVolume(const std::string& vg_name, const std::string& lv_name) {
    return runTool("lvremove -f " + vg_name + "/" + lv_name + " 2>/dev/null");
}

Result extendLogicalVolume(const std::string& vg_name, const std::string& lv_name,
                           uint64_t additional_size) {
    return runTool("lvextend -L +" + std::to_string(additional_size / (1024 * 1024)) +
                   "M " + vg_name + "/" + lv_name + " 2>/dev/null");
}

Result reduceLogicalVolume(const std::string& vg_name, const std::string& lv_name,
                           uint64_t new_size) {
    return runTool("lvreduce -L " + std::to_string(new_size / (1024 * 1024)) +
                   "M " + vg_name + "/" + lv_name + " 2>/dev/null");
}

} // namespace opm
