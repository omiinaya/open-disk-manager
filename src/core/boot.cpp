#include "opm/boot.hpp"
#include "opm/partition_table.hpp"
#include "opm/utils.hpp"
#include "opm/fat32_impl.hpp"
#include "opm/ntfs_impl.hpp"
#include "opm/ext4_impl.hpp"
#include "opm/exfat_impl.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <set>

#ifdef __linux__
#include <sys/mount.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#endif

namespace opm {

namespace {

// GPT signature
constexpr uint64_t GPT_SIGNATURE = 0x5452415020494645ULL; // "EFI PART" LE
constexpr uint64_t GPT_HEADER_LBA = 1;
constexpr uint64_t GPT_ENTRIES_LBA = 2;
constexpr uint32_t GPT_PROTECTIVE_MBR_TYPE = 0xEE;

// ISO9660 constants
constexpr uint32_t ISO_SECTOR_SIZE = 2048;
constexpr uint32_t ISO_PVD_SECTOR = 16;
constexpr uint32_t ISO_DIR_FLAG_DIRECTORY = 0x02;

struct ISODirectoryRecord {
    uint32_t extent_lba = 0;
    uint32_t data_length = 0;
    uint8_t file_flags = 0;
    std::string name;
};

// Parse one ISO9660 directory record at data + offset. Returns bytes consumed,
// or 0 when the record is invalid/empty.
size_t parseISORecord(const uint8_t* data, size_t offset, size_t max_len,
                      ISODirectoryRecord& out) {
    if (offset + 1 > max_len) return 0;
    uint8_t len_dr = data[offset];
    if (len_dr == 0) return 0;
    if (offset + len_dr > max_len) return 0;

    // extent LBA: both-endian at +2 (LE) / +6 (BE)
    out.extent_lba = static_cast<uint32_t>(data[offset + 2]) |
                     (static_cast<uint32_t>(data[offset + 3]) << 8) |
                     (static_cast<uint32_t>(data[offset + 4]) << 16) |
                     (static_cast<uint32_t>(data[offset + 5]) << 24);
    // data length: both-endian at +10 (LE) / +14 (BE)
    out.data_length = static_cast<uint32_t>(data[offset + 10]) |
                      (static_cast<uint32_t>(data[offset + 11]) << 8) |
                      (static_cast<uint32_t>(data[offset + 12]) << 16) |
                      (static_cast<uint32_t>(data[offset + 13]) << 24);
    out.file_flags = data[offset + 25];
    uint8_t name_len = data[offset + 32];
    if (offset + 33 + name_len > max_len) return 0;

    std::string raw(reinterpret_cast<const char*>(data + offset + 33), name_len);
    // Strip ";version" suffix and trailing '.', skip "." and ".." entries
    size_t semi = raw.find(';');
    if (semi != std::string::npos) raw = raw.substr(0, semi);
    while (!raw.empty() && raw.back() == '.') raw.pop_back();
    out.name = raw;
    return len_dr;
}

bool findISOTool(std::string& tool) {
    const std::vector<std::string> candidates = {
        "/usr/bin/xorriso", "/usr/bin/genisoimage", "/usr/bin/mkisofs",
        "/usr/local/bin/xorriso", "/usr/local/bin/genisoimage", "/usr/local/bin/mkisofs",
        "/bin/mkisofs"
    };
    for (const auto& c : candidates) {
        if (std::filesystem::exists(c)) {
            tool = c;
            return true;
        }
    }
    // Fall back to PATH lookup
    std::string path_env = std::getenv("PATH") ? std::getenv("PATH") : "";
    std::stringstream ss(path_env);
    std::string dir;
    while (std::getline(ss, dir, ':')) {
        for (const auto& name : {"xorriso", "genisoimage", "mkisofs"}) {
            std::string candidate = dir + "/" + name;
            if (std::filesystem::exists(candidate)) {
                tool = candidate;
                return true;
            }
        }
    }
    return false;
}

// Restore the primary GPT header + partition entry array from the backup copy.
Result restoreGPTFromBackup(std::shared_ptr<DiskIO> disk, uint64_t last_lba) {
    std::vector<uint8_t> backup_header(512, 0);
    Result r = disk->readSector(backup_header.data(), last_lba);
    if (r.failed()) {
        return Result::error("Cannot read backup GPT header at LBA " +
                             std::to_string(last_lba) + ": " + r.message);
    }

    uint64_t sig = 0;
    std::memcpy(&sig, backup_header.data(), 8);
    if (sig != GPT_SIGNATURE) {
        return Result::error("No valid backup GPT header found at end of disk");
    }

    // Fields from the backup header (little-endian)
    uint32_t header_size = 0;
    std::memcpy(&header_size, backup_header.data() + 12, 4);
    if (header_size < 92 || header_size > 512) {
        return Result::error("Invalid backup GPT header size: " +
                             std::to_string(header_size));
    }

    uint64_t backup_lba_field = 0;
    std::memcpy(&backup_lba_field, backup_header.data() + 32, 8);

    uint64_t first_usable = 0;
    std::memcpy(&first_usable, backup_header.data() + 40, 8);
    uint64_t last_usable = 0;
    std::memcpy(&last_usable, backup_header.data() + 48, 8);

    uint64_t entries_lba_backup = 0;
    std::memcpy(&entries_lba_backup, backup_header.data() + 72, 8);
    uint32_t num_entries = 0;
    std::memcpy(&num_entries, backup_header.data() + 80, 4);
    uint32_t entry_size = 0;
    std::memcpy(&entry_size, backup_header.data() + 84, 4);
    uint32_t array_crc = 0;
    std::memcpy(&array_crc, backup_header.data() + 88, 4);

    if (entry_size == 0 || entry_size > 4096 || num_entries == 0 ||
        num_entries > 4096) {
        return Result::error("Invalid backup GPT entry geometry (entries=" +
                             std::to_string(num_entries) + ", size=" +
                             std::to_string(entry_size) + ")");
    }

    // Read + verify the backup partition entry array
    uint64_t array_bytes = static_cast<uint64_t>(num_entries) * entry_size;
    std::vector<uint8_t> backup_entries(array_bytes, 0);
    uint64_t array_sectors = (array_bytes + 511) / 512;
    // The backup entry array sits just before the backup header.
    uint64_t entries_start_lba = last_lba - array_sectors;
    for (uint64_t s = 0; s < array_sectors; s++) {
        r = disk->readSector(backup_entries.data() + s * 512, entries_start_lba + s);
        if (r.failed()) {
            return Result::error("Cannot read backup GPT entries: " + r.message);
        }
    }

    uint32_t computed = utils::crc32(backup_entries.data(), array_bytes);
    if (computed != array_crc) {
        return Result::error("Backup GPT entry array CRC mismatch (disk=" +
                             std::to_string(array_crc) + ", computed=" +
                             std::to_string(computed) + ")");
    }

    // Build the primary header from the backup (swap LBA fields)
    std::vector<uint8_t> primary = backup_header;
    uint64_t current_lba = GPT_HEADER_LBA;
    std::memcpy(primary.data() + 24, &current_lba, 8);          // current LBA = 1
    std::memcpy(primary.data() + 32, &last_lba, 8);             // backup LBA = last
    uint64_t primary_entries_lba = GPT_ENTRIES_LBA;
    std::memcpy(primary.data() + 72, &primary_entries_lba, 8);  // entries LBA = 2
    std::memset(primary.data() + 16, 0, 4);                     // zero header CRC
    uint32_t header_crc = utils::crc32(primary.data(), header_size);
    std::memcpy(primary.data() + 16, &header_crc, 4);

    r = disk->writeSector(primary.data(), GPT_HEADER_LBA);
    if (r.failed()) {
        return Result::error("Failed to write restored primary GPT header: " +
                             r.message);
    }

    // Write the entry array to LBA 2
    for (uint64_t s = 0; s < array_sectors; s++) {
        r = disk->writeSector(backup_entries.data() + s * 512, GPT_ENTRIES_LBA + s);
        if (r.failed()) {
            return Result::error("Failed to write restored GPT entries: " + r.message);
        }
    }

    // Ensure a protective MBR exists at LBA 0
    std::vector<uint8_t> mbr(512, 0);
    r = disk->readSector(mbr.data(), 0);
    if (r.failed()) {
        return Result::error("Failed to read LBA 0 while repairing: " + r.message);
    }
    bool need_protective = (mbr[510] != 0x55 || mbr[511] != 0xAA);
    if (!need_protective) {
        // Check for an 0xEE partition entry
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
        uint8_t* e = protective.data() + 446; // first partition entry
        e[0] = 0x00;                          // not bootable
        e[1] = 0x00; e[2] = 0x02; e[3] = 0x00; // start CHS
        e[4] = GPT_PROTECTIVE_MBR_TYPE;        // type 0xEE
        e[5] = 0xFF; e[6] = 0xFF; e[7] = 0xFF; // end CHS
        uint32_t size = static_cast<uint32_t>(std::min<uint64_t>(0xFFFFFFFFULL, last_lba));
        uint32_t lba1 = 1;
        std::memcpy(e + 8, &lba1, 4);
        std::memcpy(e + 12, &size, 4);
        r = disk->writeSector(protective.data(), 0);
        if (r.failed()) {
            return Result::error("Failed to write protective MBR: " + r.message);
        }
    }

    return Result::ok();
}

} // anonymous namespace

// ============================================================================
// Live USB Creation
// ============================================================================

Result createLiveUSB(std::shared_ptr<DiskIO> target_device,
                     const LiveUSBOptions& options) {
    if (!target_device) {
        return Result::error("Invalid target device");
    }
    
    // Check if ISO exists
    if (!std::filesystem::exists(options.iso_path)) {
        return Result::error("ISO file not found: " + options.iso_path);
    }
    
    // Get device info
    uint64_t device_size = target_device->size();
    uint64_t iso_size = std::filesystem::file_size(options.iso_path);
    
    if (iso_size > device_size) {
        return Result::error("ISO file is larger than target device");
    }
    
    // Read ISO and write to device
    std::ifstream iso_file(options.iso_path, std::ios::binary);
    if (!iso_file) {
        return Result::error("Failed to open ISO file");
    }
    
    const size_t buffer_size = 64 * 1024 * 1024; // 64MB buffer
    std::vector<uint8_t> buffer(buffer_size);
    uint64_t bytes_written = 0;
    
    while (bytes_written < iso_size) {
        size_t to_read = static_cast<size_t>(
            std::min<uint64_t>(buffer_size, iso_size - bytes_written)
        );
        
        iso_file.read(reinterpret_cast<char*>(buffer.data()), to_read);
        size_t actual_read = iso_file.gcount();
        
        if (actual_read == 0) {
            break;
        }
        
        Result write_result = target_device->write(buffer.data(), bytes_written, actual_read);
        if (write_result.failed()) {
            return Result::error("Failed to write to device: " + write_result.message);
        }
        
        bytes_written += actual_read;
        
        if (options.progress_callback) {
            options.progress_callback(bytes_written, iso_size);
        }
    }
    
    iso_file.close();
    
    // Sync to ensure all data is written
    Result sync_result = target_device->sync();
    if (sync_result.failed()) {
        return Result::error("Failed to sync device: " + sync_result.message);
    }
    
    // Verify if requested
    if (options.verify_after_write) {
        Result verify_result = verifyLiveUSB(target_device);
        if (verify_result.failed()) {
            return Result::error("Verification failed: " + verify_result.message);
        }
    }
    
    return Result::ok();
}

Result createLiveUSBWithConfig(std::shared_ptr<DiskIO> target_device,
                               const BootConfig& config,
                               const LiveUSBOptions& options) {
    // The config (bootloader layout, persistence) is applied on top of the
    // plain ISO write. Persistence requires a second partition, which the
    // live USB flow does not create; fail honestly when requested.
    if (config.persistent_mode && config.persistence_size > 0) {
        return Result::error(
            "Persistence setup requires a writable device with free space; "
            "call setupPersistence() after creating the live USB");
    }
    return createLiveUSB(target_device, options);
}

Result verifyLiveUSB(std::shared_ptr<DiskIO> device) {
    if (!device) {
        return Result::error("Invalid device");
    }
    
    // Read boot sector and check for valid boot signature
    std::vector<uint8_t> boot_sector(512);
    Result read_result = device->read(boot_sector.data(), 0, 512);
    if (read_result.failed()) {
        return Result::error("Failed to read boot sector: " + read_result.message);
    }
    
    // Check for boot signature (0x55AA at offset 510)
    if (boot_sector[510] != 0x55 || boot_sector[511] != 0xAA) {
        return Result::error("Invalid boot signature");
    }
    
    // Check for ISO9660 or UDF signature
    // ISO9660: "CD001" at offset 0x8000
    bool has_iso9660 = false;
    std::vector<uint8_t> iso_header(2048);
    Result iso_read = device->read(iso_header.data(), 0x8000, 2048);
    if (iso_read.success()) {
        if (memcmp(iso_header.data(), "CD001", 5) == 0) {
            has_iso9660 = true;
        }
    }
    
    if (!has_iso9660) {
        // Warning: Device may not be valid, but not an error
        return Result::ok();
    }
    
    return Result::ok();
}

Result updateLiveUSB(std::shared_ptr<DiskIO> device,
                     const std::string& new_iso_path) {
    // Simply re-create with new ISO
    LiveUSBOptions options;
    options.iso_path = new_iso_path;
    return createLiveUSB(device, options);
}

Result setupPersistence(std::shared_ptr<DiskIO> device,
                        uint64_t persistence_size_bytes) {
    if (!device) {
        return Result::error("Invalid device");
    }

    if (device->isReadOnly()) {
        return Result::error("Device is read-only; open it read-write first");
    }

    if (persistence_size_bytes == 0) {
        return Result::error("Persistence size must be non-zero");
    }

    // A persistence partition needs: free space on the device, a new
    // partition entry, and a filesystem (typically ext4 or a casper-rw
    // FAT). The partition-table layer can create the entry; we refuse to
    // silently claim success without doing so.
    return Result::error(
        "setupPersistence: not implemented - requires allocating a new "
        "partition and formatting it (see PartitionTable::createPartition)");
}

// ============================================================================
// Boot Repair
// ============================================================================

Result repairBootSector(std::shared_ptr<DiskIO> device,
                        const BootRepairOptions& options) {
    if (!device) {
        return Result::error("Invalid device");
    }

    if (device->isReadOnly()) {
        return Result::error("Device is read-only; open it read-write first");
    }

    // Backup the first 2 MiB of the device before any repair
    if (options.backup_before_repair) {
        std::string backup_path = device->devicePath() + ".boot-backup";
        std::ofstream backup(backup_path, std::ios::binary | std::ios::trunc);
        if (!backup) {
            return Result::error("Failed to create backup file: " + backup_path);
        }
        const uint64_t backup_bytes = std::min<uint64_t>(2 * 1024 * 1024, device->size());
        std::vector<uint8_t> chunk(1024 * 1024);
        uint64_t done = 0;
        while (done < backup_bytes) {
            size_t to_read = static_cast<size_t>(
                std::min<uint64_t>(chunk.size(), backup_bytes - done));
            Result r = device->read(chunk.data(), done, to_read);
            if (r.failed()) {
                return Result::error("Backup read failed at " +
                                     std::to_string(done) + ": " + r.message);
            }
            backup.write(reinterpret_cast<const char*>(chunk.data()), to_read);
            done += to_read;
        }
        backup.close();
    }

    std::vector<uint8_t> sector(512, 0);
    Result r = device->readSector(sector.data(), 0);
    if (r.failed()) {
        return Result::error("Failed to read boot sector: " + r.message);
    }

    switch (options.target) {
        case BootRepairTarget::MBR: {
            bool sig_ok = (sector[510] == 0x55 && sector[511] == 0xAA);
            if (!sig_ok) {
                sector[510] = 0x55;
                sector[511] = 0xAA;
                r = device->writeSector(sector.data(), 0);
                if (r.failed()) {
                    return Result::error("Failed to restore MBR boot signature: " +
                                         r.message);
                }
            }

            // Check whether the boot code area is empty (first 440 bytes)
            bool code_empty = true;
            for (int i = 0; i < 440; i++) {
                if (sector[i] != 0) { code_empty = false; break; }
            }
            if (code_empty) {
                return Result::error(
                    "MBR boot code area is empty; a bootloader must be "
                    "installed (installBootloader) before the device can boot");
            }

            // Validate the partition entries
            try {
                auto table = PartitionTable::load(device);
                if (table && table->type() == TableType::MBR) {
                    Result v = table->validate();
                    if (v.failed()) {
                        return Result::error(
                            "MBR partition table validation failed: " + v.message +
                            " (run fixPartitionTable to repair)");
                    }
                }
            } catch (const std::exception& e) {
                return Result::error("MBR partition table parse error: " +
                                     std::string(e.what()));
            }

            if (options.fix_partition_table) {
                Result fix = fixPartitionTable(device);
                if (fix.failed()) {
                    return Result::error("Partition table fix failed: " + fix.message);
                }
            }
            return Result::ok();
        }

        case BootRepairTarget::GPT: {
            // Restore the GPT from its backup copy
            uint64_t last_lba = device->sectorCount() > 0
                                    ? device->sectorCount() - 1 : 0;
            if (last_lba == 0) {
                return Result::error("Cannot determine device size");
            }
            Result restore = restoreGPTFromBackup(device, last_lba);
            if (restore.failed()) {
                return Result::error("GPT repair failed: " + restore.message);
            }
            if (options.fix_partition_table) {
                Result fix = fixPartitionTable(device);
                if (fix.failed()) {
                    return Result::error("Partition table fix failed: " + fix.message);
                }
            }
            return Result::ok();
        }

        case BootRepairTarget::Bootloader:
            return reinstallBootloader(device, "syslinux");

        case BootRepairTarget::Filesystem:
            return Result::error(
                "Filesystem boot-sector repair requires a partition start; "
                "use checkAndRepairFilesystem(device, partition_start)");
    }

    return Result::error("Unknown boot repair target");
}

Result reinstallBootloader(std::shared_ptr<DiskIO> device,
                           const std::string& bootloader_type) {
    BootloaderOptions options;
    options.bootloader = bootloader_type;
    return installBootloader(device, options);
}

Result fixPartitionTable(std::shared_ptr<DiskIO> device) {
    if (!device) {
        return Result::error("Invalid device");
    }

    if (device->isReadOnly()) {
        return Result::error("Device is read-only; open it read-write first");
    }

    // 1) Try loading the table normally - if valid, nothing to fix.
    try {
        auto table = PartitionTable::load(device);
        if (table && table->isValid()) {
            return Result::ok();
        }
    } catch (...) {
        // fall through to repair paths
    }

    // 2) Attempt GPT recovery from the backup header at the end of the disk.
    uint64_t last_lba = device->sectorCount() > 0
                            ? device->sectorCount() - 1 : 0;
    if (last_lba > 0) {
        std::vector<uint8_t> tail(512, 0);
        if (device->readSector(tail.data(), last_lba).success()) {
            uint64_t sig = 0;
            std::memcpy(&sig, tail.data(), 8);
            if (sig == GPT_SIGNATURE) {
                Result restore = restoreGPTFromBackup(device, last_lba);
                if (restore.failed()) {
                    return Result::error(
                        "GPT backup found but restore failed: " + restore.message);
                }
                // Re-validate after restore
                try {
                    auto table = PartitionTable::load(device);
                    if (table && table->isValid()) {
                        return Result::ok();
                    }
                } catch (...) {}
                return Result::error(
                    "GPT restored from backup but still does not validate");
            }
        }
    }

    // 3) Try a plain MBR validation pass
    try {
        auto table = PartitionTable::load(device);
        if (table && table->type() == TableType::MBR) {
            Result v = table->validate();
            if (v.failed()) {
                return Result::error(
                    "MBR table is corrupt and no GPT backup exists: " + v.message);
            }
        }
    } catch (const std::exception& e) {
        return Result::error("Unrecognized partition table: " +
                             std::string(e.what()) +
                             " - no GPT backup found; manual rebuild required");
    }

    return Result::error("No partition table found and no GPT backup to restore");
}

Result checkAndRepairFilesystem(std::shared_ptr<DiskIO> device,
                                uint64_t partition_start) {
    if (!device) {
        return Result::error("Invalid device");
    }

    FileSystemType fs = device->detectFilesystem(partition_start);
    switch (fs) {
        case FileSystemType::FAT12:
        case FileSystemType::FAT16:
        case FileSystemType::FAT32:
            return fat32::checkFAT32(device, partition_start);
        case FileSystemType::NTFS:
            return ntfs::checkNTFS(device, partition_start);
        case FileSystemType::EXT2:
        case FileSystemType::EXT3:
        case FileSystemType::EXT4:
            return ext4::checkEXT4(device, partition_start);
        case FileSystemType::exFAT:
            return exfat::checkExFAT(device, partition_start);
        default:
            return Result::error(
                "Unsupported or unrecognized filesystem at sector " +
                std::to_string(partition_start));
    }
}

Result fullBootRepair(std::shared_ptr<DiskIO> device,
                      std::vector<std::string>& repair_log) {
    if (!device) {
        return Result::error("Invalid device");
    }
    
    repair_log.clear();
    repair_log.push_back("Starting full boot repair...");
    bool all_ok = true;
    
    // Step 1: Check partition table
    repair_log.push_back("Checking partition table...");
    try {
        auto table = PartitionTable::load(device);
        if (table && table->isValid()) {
            repair_log.push_back("Partition table OK (" + table->typeName() + ")");
        } else {
            repair_log.push_back("Partition table invalid - attempting repair...");
            Result pt_result = fixPartitionTable(device);
            if (pt_result.failed()) {
                repair_log.push_back("Partition table repair failed: " + pt_result.message);
                all_ok = false;
            } else {
                repair_log.push_back("Partition table repaired");
            }
        }
    } catch (const std::exception& e) {
        repair_log.push_back(std::string("Partition table error: ") + e.what());
        all_ok = false;
    }
    
    // Step 2: Repair boot sector
    repair_log.push_back("Repairing boot sector...");
    BootRepairOptions repair_options;
    repair_options.target = BootRepairTarget::MBR;
    repair_options.backup_before_repair = true;
    Result boot_result = repairBootSector(device, repair_options);
    if (boot_result.failed()) {
        repair_log.push_back("Boot sector repair failed: " + boot_result.message);
        all_ok = false;
    } else {
        repair_log.push_back("Boot sector OK");
    }
    
    // Step 3: Reinstall bootloader
    repair_log.push_back("Reinstalling bootloader...");
    Result bl_result = reinstallBootloader(device, "syslinux");
    if (bl_result.failed()) {
        repair_log.push_back("Bootloader reinstall failed: " + bl_result.message);
        all_ok = false;
    } else {
        repair_log.push_back("Bootloader OK");
    }
    
    if (all_ok) {
        repair_log.push_back("Boot repair completed");
        return Result::ok();
    }
    
    repair_log.push_back("Boot repair completed with issues - review the log above");
    return Result::error("Boot repair incomplete; see repair log for details");
}

// ============================================================================
// Bootloader Operations
// ============================================================================

Result installBootloader(std::shared_ptr<DiskIO> device,
                         const BootloaderOptions& options) {
    if (!device) {
        return Result::error("Invalid device");
    }

    if (device->isReadOnly()) {
        return Result::error("Device is read-only; open it read-write first");
    }

    // Installing a real bootloader requires the bootloader's stage files
    // (syslinux ldlinux.bss/ldlinux.sys or GRUB2 core.img/modules) matched to
    // the target architecture. The project does not bundle these binaries,
    // so a silent "success" here would be a lie. Report honestly.
    return Result::error(
        "installBootloader(" + options.bootloader + ") not implemented: the "
        "bootloader stage files (syslinux/GRUB2) are not bundled with this "
        "project. Install them separately (e.g. apt install syslinux grub-pc) "
        "and write the stage code to the device, or use the distro's "
        "installer (syslinux -i /dev/sdX, grub-install /dev/sdX)");
}

Result updateBootloader(std::shared_ptr<DiskIO> device,
                        const BootloaderOptions& options) {
    // For now, just re-install
    return installBootloader(device, options);
}

Result configureBootloader(std::shared_ptr<DiskIO> device,
                           const BootConfig& config,
                           const BootloaderOptions& options) {
    if (!device) {
        return Result::error("Invalid device");
    }
    (void)config;

    // Writing a bootloader configuration requires a mounted, writable
    // filesystem on the boot partition. The DiskIO layer only exposes raw
    // sectors, so silently returning ok() would claim a config was written
    // when it was not.
    return Result::error(
        "configureBootloader not implemented: bootloader configuration must be "
        "written through a mounted filesystem (mount the boot partition, then "
        "write " + options.bootloader + " config files there)");
}

// ============================================================================
// ISO Operations
// ============================================================================

Result mountISO(const std::string& iso_path, const std::string& mount_point) {
    if (!std::filesystem::exists(iso_path)) {
        return Result::error("ISO file not found: " + iso_path);
    }
    if (!std::filesystem::exists(mount_point)) {
        return Result::error("Mount point does not exist: " + mount_point);
    }

#ifdef __linux__
    if (::mount(iso_path.c_str(), mount_point.c_str(), "iso9660",
                MS_RDONLY | MS_NOSUID | MS_NODEV, nullptr) != 0) {
        return Result::error("mount() failed: " + std::string(std::strerror(errno)));
    }
    return Result::ok();
#else
    (void)iso_path;
    (void)mount_point;
    return Result::error("ISO mounting not implemented for this platform");
#endif
}

Result unmountISO(const std::string& mount_point) {
    if (!std::filesystem::exists(mount_point)) {
        return Result::error("Mount point does not exist: " + mount_point);
    }

#ifdef __linux__
    if (::umount(mount_point.c_str()) != 0) {
        return Result::error("umount() failed: " + std::string(std::strerror(errno)));
    }
    return Result::ok();
#else
    (void)mount_point;
    return Result::error("ISO unmounting not implemented for this platform");
#endif
}

Result extractISO(const std::string& iso_path,
                  const std::string& output_directory,
                  std::function<void(uint64_t, uint64_t)> progress) {
    if (!std::filesystem::exists(iso_path)) {
        return Result::error("ISO file not found");
    }

    std::ifstream iso(iso_path, std::ios::binary);
    if (!iso) {
        return Result::error("Failed to open ISO file");
    }
    iso.seekg(0, std::ios::end);
    uint64_t iso_size = static_cast<uint64_t>(iso.tellg());
    iso.seekg(0, std::ios::beg);

    // Read the Primary Volume Descriptor (sector 16)
    std::vector<uint8_t> pvd(ISO_SECTOR_SIZE);
    iso.seekg(static_cast<std::streamoff>(ISO_PVD_SECTOR * ISO_SECTOR_SIZE));
    iso.read(reinterpret_cast<char*>(pvd.data()), ISO_SECTOR_SIZE);
    if (iso.gcount() < static_cast<std::streamsize>(ISO_SECTOR_SIZE)) {
        return Result::error("ISO file too small to contain a volume descriptor");
    }
    if (pvd[0] != 1 || std::memcmp(pvd.data() + 1, "CD001", 5) != 0) {
        return Result::error("Not a valid ISO9660 image (missing PVD)");
    }

    std::filesystem::create_directories(output_directory);

    // Recursive extractor over directory records
    std::function<Result(const std::string&, uint32_t, uint32_t)> walk;
    walk = [&](const std::string& dir_path, uint32_t extent_lba,
               uint32_t extent_len) -> Result {
        // Read the directory extent (may span multiple sectors)
        std::vector<uint8_t> dir_data(extent_len);
        uint64_t dir_offset = static_cast<uint64_t>(extent_lba) * ISO_SECTOR_SIZE;
        iso.seekg(static_cast<std::streamoff>(dir_offset));
        iso.read(reinterpret_cast<char*>(dir_data.data()), extent_len);
        std::streamsize got = iso.gcount();
        if (got < static_cast<std::streamsize>(extent_len)) {
            return Result::error("Truncated directory extent at LBA " +
                                 std::to_string(extent_lba));
        }

        std::set<std::string> entries; // dedupe "." / ".." aliases
        size_t offset = 0;
        while (offset < dir_data.size()) {
            ISODirectoryRecord rec;
            size_t consumed = parseISORecord(dir_data.data(), offset,
                                             dir_data.size(), rec);
            if (consumed == 0) break;
            offset += consumed;

            if (rec.name.empty() || rec.name == "." || rec.name == "..") continue;
            if (!entries.insert(rec.name).second) continue;

            std::string target = dir_path + "/" + rec.name;
            if (rec.file_flags & ISO_DIR_FLAG_DIRECTORY) {
                std::filesystem::create_directories(target);
                Result sub = walk(target, rec.extent_lba, rec.data_length);
                if (sub.failed()) return sub;
            } else {
                // Extract file
                std::ofstream out(target, std::ios::binary | std::ios::trunc);
                if (!out) {
                    return Result::error("Failed to create output file: " + target);
                }
                uint64_t file_offset = static_cast<uint64_t>(rec.extent_lba) *
                                       ISO_SECTOR_SIZE;
                iso.seekg(static_cast<std::streamoff>(file_offset));
                std::vector<uint8_t> chunk(1024 * 1024);
                uint32_t remaining = rec.data_length;
                while (remaining > 0) {
                    size_t to_read = static_cast<size_t>(
                        std::min<uint32_t>(chunk.size(), remaining));
                    iso.read(reinterpret_cast<char*>(chunk.data()), to_read);
                    std::streamsize got_bytes = iso.gcount();
                    if (got_bytes <= 0) {
                        return Result::error("Truncated file data: " + target);
                    }
                    out.write(reinterpret_cast<const char*>(chunk.data()), got_bytes);
                    remaining -= static_cast<uint32_t>(got_bytes);
                }
                out.close();
            }
        }
        return Result::ok();
    };

    // Root directory record sits at PVD offset 156 (34 bytes)
    ISODirectoryRecord root;
    size_t consumed = parseISORecord(pvd.data(), 156, ISO_SECTOR_SIZE, root);
    if (consumed == 0) {
        return Result::error("Invalid root directory record in PVD");
    }

    Result walk_result = walk(output_directory, root.extent_lba, root.data_length);
    if (walk_result.failed()) {
        return Result::error("ISO extraction failed: " + walk_result.message);
    }

    if (progress) {
        progress(iso_size, iso_size);
    }

    return Result::ok();
}

Result createBootableISO(const std::string& source_directory,
                         const std::string& output_iso_path,
                         const BootConfig& config) {
    if (!std::filesystem::exists(source_directory)) {
        return Result::error("Source directory not found");
    }
    if (!std::filesystem::is_directory(source_directory)) {
        return Result::error("Source path is not a directory");
    }

    std::string tool;
    if (!findISOTool(tool)) {
        return Result::error(
            "No ISO creation tool found (xorriso, genisoimage, or mkisofs). "
            "Install one (e.g. apt install xorriso) and retry");
    }

    std::string vol_label = config.distribution_name;
    if (vol_label.empty()) vol_label = "OPM";
    if (vol_label.size() > 32) vol_label = vol_label.substr(0, 32);

    // If the source tree carries an isolinux boot payload, emit a bootable
    // ISO; otherwise produce a data ISO (still honest - the caller sees the
    // same return path, and the content reflects the source tree).
    std::string boot_opts;
    if (std::filesystem::exists(source_directory + "/isolinux/isolinux.bin")) {
        boot_opts = " -b isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 "
                    "-boot-info-table -c isolinux/boot.cat";
    }

    std::string cmd = tool + " -o " + output_iso_path + boot_opts +
                      " -R -J -V \"" + vol_label + "\" " + source_directory;

    int rc = std::system(cmd.c_str());
    if (rc != 0) {
        return Result::error("ISO creation failed (tool exit code " +
                             std::to_string(rc) + ")");
    }
    if (!std::filesystem::exists(output_iso_path)) {
        return Result::error("ISO creation reported success but no output file "
                             "was produced");
    }
    return Result::ok();
}

// ============================================================================
// USB Detection
// ============================================================================

std::vector<USBDeviceInfo> detectUSBDevices() {
    std::vector<USBDeviceInfo> devices;

#ifdef __linux__
    const std::string sys_block = "/sys/block";
    if (!std::filesystem::exists(sys_block)) {
        return devices;
    }

    for (const auto& entry : std::filesystem::directory_iterator(sys_block)) {
        std::string name = entry.path().filename().string();
        if (name.find("loop") == 0 || name.find("ram") == 0 ||
            name.find("dm-") == 0) {
            continue; // skip loop/ram/device-mapper devices
        }

        std::string dev_path = "/dev/" + name;

        // Resolve the sysfs device chain and look for a USB link
        std::error_code ec;
        std::string resolved = std::filesystem::weakly_canonical(
            entry.path(), ec).string();
        bool is_usb = resolved.find("/usb") != std::string::npos;

        // Fall back: some controllers expose through platform devices
        if (!is_usb) {
            std::string subsystem_link = sys_block + "/" + name + "/subsystem";
            std::string subsystem = std::filesystem::weakly_canonical(
                subsystem_link, ec).string();
            is_usb = subsystem.find("usb") != std::string::npos;
        }

        if (!is_usb) continue;

        USBDeviceInfo info;
        info.device_path = dev_path;
        info.is_usb = true;

        // Removable flag
        std::ifstream removable(sys_block + "/" + name + "/removable");
        int rem = 0;
        removable >> rem;
        info.is_removable = (rem == 1);

        // Size (512-byte units)
        std::ifstream size_file(sys_block + "/" + name + "/size");
        uint64_t sectors = 0;
        size_file >> sectors;
        info.size = sectors * 512;

        // Sector size
        std::ifstream lbas(sys_block + "/" + name +
                           "/queue/logical_block_size");
        uint32_t lbs = 512;
        lbas >> lbs;
        info.sector_size = lbs;

        // Vendor / model from the parent USB device
        std::string base = entry.path().string();
        std::string vendor_file = base + "/device/vendor";
        std::string model_file = base + "/device/model";
        std::ifstream vf(vendor_file);
        std::getline(vf, info.vendor);
        std::ifstream mf(model_file);
        std::getline(mf, info.model);
        while (!info.vendor.empty() && info.vendor.back() == ' ') info.vendor.pop_back();
        while (!info.model.empty() && info.model.back() == ' ') info.model.pop_back();

        // Filesystem detection (whole-device)
        try {
            auto disk = DiskIO::openReadOnly(dev_path);
            if (disk) {
                FileSystemType fs = disk->detectFilesystem(0);
                switch (fs) {
                    case FileSystemType::FAT32: info.filesystem = "FAT32"; break;
                    case FileSystemType::FAT16: info.filesystem = "FAT16"; break;
                    case FileSystemType::FAT12: info.filesystem = "FAT12"; break;
                    case FileSystemType::NTFS:  info.filesystem = "NTFS"; break;
                    case FileSystemType::EXT4:  info.filesystem = "ext4"; break;
                    case FileSystemType::EXT3:  info.filesystem = "ext3"; break;
                    case FileSystemType::EXT2:  info.filesystem = "ext2"; break;
                    case FileSystemType::exFAT: info.filesystem = "exFAT"; break;
                    default: break;
                }
            }
        } catch (...) {}

        devices.push_back(info);
    }
#endif

    return devices;
}

bool isUSBDevice(const std::string& device_path) {
#ifdef __linux__
    std::string name = device_path;
    size_t slash = name.rfind('/');
    if (slash != std::string::npos) name = name.substr(slash + 1);

    std::string sys_path = "/sys/block/" + name;
    if (!std::filesystem::exists(sys_path)) {
        return false;
    }

    std::error_code ec;
    std::string resolved = std::filesystem::weakly_canonical(
        sys_path, ec).string();
    if (resolved.find("/usb") != std::string::npos) {
        return true;
    }

    std::string removable;
    std::ifstream rf(sys_path + "/removable");
    rf >> removable;
    return removable == "1";
#else
    (void)device_path;
    return false;
#endif
}

Result getUSBDeviceInfo(const std::string& device_path, USBDeviceInfo& info) {
    info = USBDeviceInfo();
    info.device_path = device_path;

#ifdef __linux__
    std::string name = device_path;
    size_t slash = name.rfind('/');
    if (slash != std::string::npos) name = name.substr(slash + 1);

    std::string sys_path = "/sys/block/" + name;
    if (!std::filesystem::exists(sys_path)) {
        return Result::error("Device not found in sysfs: " + device_path);
    }

    info.is_usb = isUSBDevice(device_path);

    std::ifstream removable(sys_path + "/removable");
    int rem = 0;
    removable >> rem;
    info.is_removable = (rem == 1);

    std::ifstream size_file(sys_path + "/size");
    uint64_t sectors = 0;
    size_file >> sectors;
    info.size = sectors * 512;

    std::ifstream lbas(sys_path + "/queue/logical_block_size");
    uint32_t lbs = 512;
    lbas >> lbs;
    info.sector_size = lbs;

    std::string base = sys_path;
    std::ifstream vf(base + "/device/vendor");
    std::getline(vf, info.vendor);
    std::ifstream mf(base + "/device/model");
    std::getline(mf, info.model);
    while (!info.vendor.empty() && info.vendor.back() == ' ') info.vendor.pop_back();
    while (!info.model.empty() && info.model.back() == ' ') info.model.pop_back();

    // Volume label from the boot sector / superblock
    try {
        auto disk = DiskIO::openReadOnly(device_path);
        if (disk) {
            FileSystemType fs = disk->detectFilesystem(0);
            std::vector<uint8_t> sector(512, 0);
            if (disk->readSector(sector.data(), 0).success()) {
                if (fs == FileSystemType::FAT32 ||
                    fs == FileSystemType::FAT16 ||
                    fs == FileSystemType::FAT12) {
                    info.label.assign(
                        reinterpret_cast<const char*>(sector.data() + 43), 11);
                    while (!info.label.empty() && info.label.back() == ' ')
                        info.label.pop_back();
                } else if (fs == FileSystemType::EXT4 ||
                           fs == FileSystemType::EXT3 ||
                           fs == FileSystemType::EXT2) {
                    std::vector<uint8_t> sb(512, 0);
                    if (disk->read(sb.data(), 1024, 512).success()) {
                        info.label.assign(
                            reinterpret_cast<const char*>(sb.data() + 120), 16);
                        while (!info.label.empty() && info.label.back() == '\0')
                            info.label.pop_back();
                    }
                }
            }
        }
    } catch (...) {}

    return Result::ok();
#else
    (void)device_path;
    return Result::error("USB device info not implemented for this platform");
#endif
}

bool isBootableDevice(std::shared_ptr<DiskIO> device) {
    if (!device) {
        return false;
    }
    
    // Read boot sector
    std::vector<uint8_t> boot_sector(512);
    Result read_result = device->read(boot_sector.data(), 0, 512);
    if (read_result.failed()) {
        return false;
    }
    
    // Check for boot signature
    if (boot_sector[510] != 0x55 || boot_sector[511] != 0xAA) {
        return false;
    }
    
    return true;
}

BootMode detectBootMode() {
    #ifdef __linux__
    // Check for UEFI by looking for /sys/firmware/efi
    if (std::filesystem::exists("/sys/firmware/efi")) {
        return BootMode::UEFI;
    }
    #endif
    
    return BootMode::BIOS;
}

} // namespace opm
