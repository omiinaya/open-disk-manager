#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <memory>
#include <sstream>
#include "opm/partition_table.hpp"
#include "opm/disk_io.hpp"
#include "opm/utils.hpp"
#include "opm/types.hpp"
#include "opm/fat32_impl.hpp"
#include "opm/ntfs_impl.hpp"
#include "opm/ext4_impl.hpp"
#include "opm/exfat_impl.hpp"
#include "opm/encryption.hpp"
#include "opm/lvm.hpp"
#include "opm/raid.hpp"
#include "opm/boot.hpp"
#include "opm/swap.hpp"
#include "opm/recovery.hpp"
#include "opm/undelete.hpp"
#include "opm/security.hpp"
#include "opm/i18n.hpp"
#include "opm/backup.hpp"
#include "opm/schedule.hpp"
#include "opm/tar.hpp"
#include "opm/merge.hpp"
#include "opm/clone.hpp"

namespace opm {
namespace cli {

namespace {

// ---------------------------------------------------------------------------
// Parsing helpers
// ---------------------------------------------------------------------------

bool parseU64(const std::string& s, uint64_t& out) {
    if (s.empty()) return false;
    try {
        size_t pos = 0;
        unsigned long long v = std::stoull(s, &pos, 0);
        if (pos != s.size()) return false;
        out = static_cast<uint64_t>(v);
        return true;
    } catch (...) {
        return false;
    }
}

// Parse a byte size with optional K/M/G/T suffix (e.g. "512M", "10G", "2048")
bool parseSize(const std::string& s, uint64_t& out) {
    if (s.empty()) return false;
    std::string num = s;
    uint64_t mult = 1;
    char suffix = 0;
    char last = s.back();
    switch (last) {
        case 'k': case 'K': mult = 1024ULL; suffix = last; break;
        case 'm': case 'M': mult = 1024ULL * 1024; suffix = last; break;
        case 'g': case 'G': mult = 1024ULL * 1024 * 1024; suffix = last; break;
        case 't': case 'T': mult = 1024ULL * 1024 * 1024 * 1024; suffix = last; break;
        case 'b': case 'B': suffix = last; break;  // explicit bytes
        default: break;
    }
    if (suffix) num = num.substr(0, num.size() - 1);
    uint64_t value = 0;
    if (!parseU64(num, value)) return false;
    out = value * mult;
    return true;
}

PartitionType parsePartitionType(const std::string& name, bool& ok) {
    ok = true;
    if (name == "ntfs") return PartitionType::NTFS;
    if (name == "fat32") return PartitionType::FAT32LBA;
    if (name == "linux") return PartitionType::Linux;
    if (name == "swap" || name == "linux-swap") return PartitionType::LinuxSwap;
    if (name == "efi" || name == "esp") return PartitionType::EFI;
    if (name == "lvm") return PartitionType::LinuxLVM;
    if (name == "raid") return PartitionType::LinuxRAID;
    ok = false;
    return PartitionType::Unknown;
}

std::string fsTypeName(FileSystemType fs) {
    switch (fs) {
        case FileSystemType::FAT12: return "FAT12";
        case FileSystemType::FAT16: return "FAT16";
        case FileSystemType::FAT32: return "FAT32";
        case FileSystemType::exFAT: return "exFAT";
        case FileSystemType::NTFS:  return "NTFS";
        case FileSystemType::EXT2:  return "ext2";
        case FileSystemType::EXT3:  return "ext3";
        case FileSystemType::EXT4:  return "ext4";
        case FileSystemType::Swap:  return "swap";
        case FileSystemType::LVM2:  return "LVM2";
        default:                    return "Unknown";
    }
}

bool openReadWrite(const std::string& device, std::shared_ptr<DiskIO>& disk,
                   std::string& err) {
    disk = DiskIO::openReadWrite(device);
    if (!disk || !disk->isOpen()) {
        err = "Failed to open device read-write: " + device +
              " (are you root?)";
        return false;
    }
    return true;
}

bool loadTable(const std::shared_ptr<DiskIO>& disk,
               std::unique_ptr<PartitionTable>& table, std::string& err) {
    try {
        table = PartitionTable::load(disk);
    } catch (const std::exception& e) {
        err = std::string("Partition table error: ") + e.what();
        return false;
    }
    if (!table) {
        err = "No recognizable partition table found on device";
        return false;
    }
    return true;
}

bool findPartitionByStart(const PartitionTable& table, uint64_t start,
                          int& index) {
    auto parts = table.getPartitions();
    for (size_t i = 0; i < parts.size(); i++) {
        if (parts[i].startSector() == start) {
            index = static_cast<int>(i + 1);
            return true;
        }
    }
    return false;
}

} // anonymous namespace

namespace {
bool dry_run = false;

bool parseDryRun(const std::vector<std::string>& args,
                 std::vector<std::string>& rest) {
    rest.clear();
    for (const auto& a : args) {
        if (a == "--dry-run" || a == "-n") {
            dry_run = true;
        } else {
            rest.push_back(a);
        }
    }
    return !rest.empty();
}
} // anonymous namespace

// ---------------------------------------------------------------------------
// mklabel <device> <mbr|gpt>
// ---------------------------------------------------------------------------
int cmdMklabel(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "Usage: opm mklabel <device> <mbr|gpt>\n";
        return 1;
    }
    TableType type;
    if (args[1] == "mbr") {
        type = TableType::MBR;
    } else if (args[1] == "gpt") {
        type = TableType::GPT;
    } else {
        std::cerr << "Error: unknown label type '" << args[1]
                  << "' (mbr or gpt)\n";
        return 1;
    }

    std::string err;
    std::shared_ptr<DiskIO> disk;
    if (!openReadWrite(args[0], disk, err)) { std::cerr << err << "\n"; return 1; }

    std::unique_ptr<PartitionTable> table;
    try {
        table = PartitionTable::create(disk, type);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    // When creating an MBR table, clear the GPT structures (primary header +
    // entries at sectors 1-33, and the backup at the end) so a stale "EFI
    // PART" signature cannot be misdetected after the label change.
    if (type == TableType::MBR) {
        std::vector<uint8_t> zero(512, 0);
        uint64_t end = std::min<uint64_t>(34, disk->sectorCount());
        for (uint64_t s = 1; s < end; s++) {
            disk->writeSector(zero.data(), s);
        }
        if (disk->sectorCount() > 34) {
            uint64_t last = disk->sectorCount() - 1;
            for (uint64_t s = 0; s < 33 && (last - s) > end; s++) {
                disk->writeSector(zero.data(), last - s);
            }
        }
    }

    Result r = table->commit();
    if (r.failed()) {
        std::cerr << "Error: commit failed: " << r.message << "\n";
        return 1;
    }
    std::cout << "Created new " << table->typeName()
              << " partition table on " << args[0] << "\n";
    return 0;
}

// ---------------------------------------------------------------------------
// create <device> <start> <size> <type> [name]  [--dry-run]
// ---------------------------------------------------------------------------
int cmdCreate(const std::vector<std::string>& args) {
    std::vector<std::string> a;
    if (!parseDryRun(args, a)) {
        std::cerr << "Error: no arguments\n";
        return 1;
    }
    if (a.size() < 4) {
        std::cerr << "Usage: opm create <device> <start_sector> <size> <type> [name] [--dry-run]\n";
        return 1;
    }
    uint64_t start = 0, size = 0;
    if (!parseU64(a[1], start)) {
        std::cerr << "Error: invalid start sector: " << a[1] << "\n";
        return 1;
    }
    if (!parseSize(a[2], size)) {
        std::cerr << "Error: invalid size: " << a[2] << "\n";
        return 1;
    }
    bool ok = false;
    PartitionType type = parsePartitionType(a[3], ok);
    if (!ok) {
        std::cerr << "Error: unknown partition type '" << a[3]
                  << "' (ntfs, fat32, linux, swap, efi, lvm, raid)\n";
        return 1;
    }
    std::string name = a.size() > 4 ? a[4] : "";

    std::string err;
    std::shared_ptr<DiskIO> disk;
    if (!openReadWrite(a[0], disk, err)) { std::cerr << err << "\n"; return 1; }
    std::unique_ptr<PartitionTable> table;
    if (!loadTable(disk, table, err)) {
        // No existing table: initialize a fresh MBR table automatically so
        // "create" works on a brand-new disk.
        std::cout << "No partition table found - initializing new MBR table\n";
        try {
            table = PartitionTable::create(disk, TableType::MBR);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }
    }

    if (dry_run) {
        std::cout << "[DRY RUN] Would create " << table->typeName()
                  << " partition: start=" << start
                  << " size=" << utils::formatBytes(size)
                  << " type=0x" << std::hex
                  << static_cast<int>(type) << std::dec
                  << " name='" << name << "'\n";
        return 0;
    }

    Result r = table->createPartition(start, size, type, name);
    if (r.failed()) {
        std::cerr << "Error: " << r.message << "\n";
        return 1;
    }
    r = table->commit();
    if (r.failed()) {
        std::cerr << "Error: commit failed: " << r.message << "\n";
        return 1;
    }
    std::cout << "Partition created: " << table->typeName()
              << " start=" << start << " size=" << utils::formatBytes(size) << "\n";
    return 0;
}

// ---------------------------------------------------------------------------
// delete <device> <number>  [--dry-run]
// ---------------------------------------------------------------------------
int cmdDelete(const std::vector<std::string>& args) {
    std::vector<std::string> a;
    parseDryRun(args, a);
    if (a.size() < 2) {
        std::cerr << "Usage: opm delete <device> <partition_number> [--dry-run]\n";
        return 1;
    }
    uint64_t number = 0;
    if (!parseU64(a[1], number) || number == 0) {
        std::cerr << "Error: invalid partition number: " << a[1] << "\n";
        return 1;
    }

    std::string err;
    std::shared_ptr<DiskIO> disk;
    if (!openReadWrite(a[0], disk, err)) { std::cerr << err << "\n"; return 1; }
    std::unique_ptr<PartitionTable> table;
    if (!loadTable(disk, table, err)) { std::cerr << err << "\n"; return 1; }

    if (dry_run) {
        auto parts = table->getPartitions();
        if (number > parts.size()) {
            std::cerr << "Error: partition " << number << " does not exist\n";
            return 1;
        }
        std::cout << "[DRY RUN] Would delete partition " << number
                  << " (start=" << parts[number - 1].startSector()
                  << " size=" << utils::formatBytes(
                         parts[number - 1].sectorCount() * disk->sectorSize())
                  << ") from " << table->typeName() << " table\n";
        return 0;
    }

    Result r = table->deletePartition(static_cast<int>(number));
    if (r.failed()) {
        std::cerr << "Error: " << r.message << "\n";
        return 1;
    }
    r = table->commit();
    if (r.failed()) {
        std::cerr << "Error: commit failed: " << r.message << "\n";
        return 1;
    }
    std::cout << "Partition " << number << " deleted from "
              << table->typeName() << " table\n";
    return 0;
}

// ---------------------------------------------------------------------------
// resize <device> <number> <new_size>  [--dry-run]
// ---------------------------------------------------------------------------
int cmdResize(const std::vector<std::string>& args) {
    std::vector<std::string> a;
    parseDryRun(args, a);
    if (a.size() < 3) {
        std::cerr << "Usage: opm resize <device> <partition_number> <new_size> [--dry-run]\n";
        return 1;
    }
    uint64_t number = 0, new_size = 0;
    if (!parseU64(a[1], number) || number == 0) {
        std::cerr << "Error: invalid partition number: " << a[1] << "\n";
        return 1;
    }
    if (!parseSize(a[2], new_size)) {
        std::cerr << "Error: invalid size: " << a[2] << "\n";
        return 1;
    }

    std::string err;
    std::shared_ptr<DiskIO> disk;
    if (!openReadWrite(a[0], disk, err)) { std::cerr << err << "\n"; return 1; }
    std::unique_ptr<PartitionTable> table;
    if (!loadTable(disk, table, err)) { std::cerr << err << "\n"; return 1; }

    if (dry_run) {
        std::cout << "[DRY RUN] Would resize partition " << number << " on "
                  << table->typeName() << " table to "
                  << utils::formatBytes(new_size) << "\n";
        return 0;
    }

    Result r = table->resizePartition(static_cast<int>(number), new_size);
    if (r.failed()) {
        std::cerr << "Error: " << r.message << "\n";
        return 1;
    }
    r = table->commit();
    if (r.failed()) {
        std::cerr << "Error: commit failed: " << r.message << "\n";
        return 1;
    }
    std::cout << "Partition " << number << " resized to "
              << utils::formatBytes(new_size) << "\n";
    return 0;
}

// ---------------------------------------------------------------------------
// move <device> <number> <new_start_sector>
// Copies the partition's data to the new location, then re-creates the
// partition table entry at the new start and removes the old entry.
// ---------------------------------------------------------------------------
int cmdMove(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        std::cerr << "Usage: opm move <device> <partition_number> <new_start_sector>\n";
        return 1;
    }
    uint64_t number = 0, new_start = 0;
    if (!parseU64(args[1], number) || number == 0) {
        std::cerr << "Error: invalid partition number: " << args[1] << "\n";
        return 1;
    }
    if (!parseU64(args[2], new_start)) {
        std::cerr << "Error: invalid start sector: " << args[2] << "\n";
        return 1;
    }

    std::string err;
    std::shared_ptr<DiskIO> disk;
    if (!openReadWrite(args[0], disk, err)) { std::cerr << err << "\n"; return 1; }
    std::unique_ptr<PartitionTable> table;
    if (!loadTable(disk, table, err)) { std::cerr << err << "\n"; return 1; }

    auto parts = table->getPartitions();
    if (number > parts.size()) {
        std::cerr << "Error: partition " << number << " does not exist\n";
        return 1;
    }
    const Partition& part = parts[number - 1];
    uint64_t old_start = part.startSector();
    uint64_t sector_count = part.sectorCount();
    uint64_t size_bytes = sector_count * disk->sectorSize();

    if (new_start == old_start) {
        std::cout << "Partition already starts at sector " << new_start << "\n";
        return 0;
    }

    // Copy data first (read-old -> write-new, chunked)
    const size_t chunk = 1024 * 1024;
    std::vector<uint8_t> buffer(chunk);
    uint64_t remaining = sector_count;
    uint64_t sector = 0;
    while (remaining > 0) {
        uint32_t to_copy = static_cast<uint32_t>(
            std::min<uint64_t>(chunk / disk->sectorSize(), remaining));
        Result r = disk->readSectors(buffer.data(), old_start + sector, to_copy);
        if (r.failed()) {
            std::cerr << "Error: read failed at sector " << (old_start + sector)
                      << ": " << r.message << "\n";
            return 1;
        }
        r = disk->writeSectors(buffer.data(), new_start + sector, to_copy);
        if (r.failed()) {
            std::cerr << "Error: write failed at sector " << (new_start + sector)
                      << ": " << r.message << "\n";
            return 1;
        }
        sector += to_copy;
        remaining -= to_copy;
    }

    // Re-create the entry at the new location, then remove the old one
    Result r = table->createPartition(new_start, size_bytes, part.type(), part.name());
    if (r.failed()) {
        std::cerr << "Error: create at new location failed: " << r.message << "\n";
        return 1;
    }
    int old_index = 0;
    if (!findPartitionByStart(*table, old_start, old_index)) {
        std::cerr << "Error: could not locate original partition entry\n";
        return 1;
    }
    r = table->deletePartition(old_index);
    if (r.failed()) {
        std::cerr << "Error: delete old entry failed: " << r.message << "\n";
        return 1;
    }
    r = table->commit();
    if (r.failed()) {
        std::cerr << "Error: commit failed: " << r.message << "\n";
        return 1;
    }
    std::cout << "Partition moved from sector " << old_start << " to "
              << new_start << " (" << utils::formatBytes(size_bytes) << ")\n";
    return 0;
}

// ---------------------------------------------------------------------------
// format <device> <fs> <start_sector> [size] [label]
// ---------------------------------------------------------------------------
int cmdFormat(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        std::cerr << "Usage: opm format <device> <fs> <start_sector> [size] [label]\n"
                  << "  fs: fat32, ntfs, ext4, exfat\n";
        return 1;
    }
    std::string fs_name = args[1];
    uint64_t start = 0;
    if (!parseU64(args[2], start)) {
        std::cerr << "Error: invalid start sector: " << args[2] << "\n";
        return 1;
    }

    std::string err;
    std::shared_ptr<DiskIO> disk;
    if (!openReadWrite(args[0], disk, err)) { std::cerr << err << "\n"; return 1; }

    uint64_t size_bytes = 0;
    if (args.size() > 3) {
        if (!parseSize(args[3], size_bytes)) {
            std::cerr << "Error: invalid size: " << args[3] << "\n";
            return 1;
        }
    } else {
        // Auto-size from the partition table entry
        std::unique_ptr<PartitionTable> table;
        if (!loadTable(disk, table, err)) { std::cerr << err << "\n"; return 1; }
        int idx = 0;
        if (!findPartitionByStart(*table, start, idx)) {
            std::cerr << "Error: no partition starts at sector " << start
                      << "; provide an explicit size\n";
            return 1;
        }
        auto parts = table->getPartitions();
        size_bytes = parts[idx - 1].sectorCount() * disk->sectorSize();
    }
    std::string label = args.size() > 4 ? args[4] : "";

    Result r;
    if (fs_name == "fat32") {
        r = fat32::formatFAT32Complete(disk, start, size_bytes, label);
    } else if (fs_name == "ntfs") {
        r = ntfs::formatNTFS(disk, start, size_bytes, label);
    } else if (fs_name == "ext4") {
        r = ext4::formatEXT4(disk, start, size_bytes, label);
    } else if (fs_name == "exfat") {
        r = exfat::formatExFAT(disk, start, size_bytes, label);
    } else if (fs_name == "swap") {
        r = formatSwap(disk, start, size_bytes, label);
    } else {
        std::cerr << "Error: unknown filesystem '" << fs_name
                  << "' (fat32, ntfs, ext4, exfat, swap)\n";
        return 1;
    }

    if (r.failed()) {
        std::cerr << "Error: format failed: " << r.message << "\n";
        return 1;
    }
    if (disk->flush().failed()) {
        std::cerr << "Warning: flush failed after format\n";
    }
    std::cout << "Formatted partition at sector " << start << " as " << fs_name
              << " (" << utils::formatBytes(size_bytes) << ")\n";
    return 0;
}

// ---------------------------------------------------------------------------
// check <device> <start_sector>
// ---------------------------------------------------------------------------
int cmdCheck(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "Usage: opm check <device> <start_sector>\n";
        return 1;
    }
    uint64_t start = 0;
    if (!parseU64(args[1], start)) {
        std::cerr << "Error: invalid start sector: " << args[1] << "\n";
        return 1;
    }

    std::string err;
    std::shared_ptr<DiskIO> disk;
    disk = DiskIO::openReadOnly(args[0]);
    if (!disk || !disk->isOpen()) {
        std::cerr << "Error: failed to open device read-only: " << args[0] << "\n";
        return 1;
    }

    FileSystemType fs = disk->detectFilesystem(start);
    if (fs == FileSystemType::Unknown) {
        std::cerr << "Error: no recognizable filesystem at sector " << start << "\n";
        return 1;
    }
    std::cout << "Checking " << fsTypeName(fs) << " at sector " << start << "...\n";

    Result r;
    switch (fs) {
        case FileSystemType::FAT12:
        case FileSystemType::FAT16:
        case FileSystemType::FAT32:
            r = fat32::checkFAT32(disk, start);
            break;
        case FileSystemType::NTFS:
            r = ntfs::checkNTFS(disk, start);
            break;
        case FileSystemType::EXT2:
        case FileSystemType::EXT3:
        case FileSystemType::EXT4:
            r = ext4::checkEXT4(disk, start);
            break;
        case FileSystemType::exFAT:
            r = exfat::checkExFAT(disk, start);
            break;
        case FileSystemType::Swap:
            r = isSwap(disk, start) ? Result::ok()
                                    : Result::error("swap signature missing");
            break;
        default:
            std::cerr << "Error: check not supported for " << fsTypeName(fs) << "\n";
            return 1;
    }

    if (r.failed()) {
        std::cerr << "Check failed: " << r.message << "\n";
        return 1;
    }
    std::cout << "Check passed: no errors found.\n";
    return 0;
}

// ---------------------------------------------------------------------------
// fsinfo <device> <start_sector>
// ---------------------------------------------------------------------------
int cmdFSInfo(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "Usage: opm fsinfo <device> <start_sector>\n";
        return 1;
    }
    uint64_t start = 0;
    if (!parseU64(args[1], start)) {
        std::cerr << "Error: invalid start sector: " << args[1] << "\n";
        return 1;
    }

    std::string err;
    std::shared_ptr<DiskIO> disk;
    disk = DiskIO::openReadOnly(args[0]);
    if (!disk || !disk->isOpen()) {
        std::cerr << "Error: failed to open device read-only: " << args[0] << "\n";
        return 1;
    }

    FileSystemType fs = disk->detectFilesystem(start);
    if (fs == FileSystemType::Unknown) {
        std::cerr << "Error: no recognizable filesystem at sector " << start << "\n";
        return 1;
    }

    std::cout << "Filesystem: " << fsTypeName(fs) << "\n"
              << "Start sector: " << start << "\n";

    if (fs == FileSystemType::FAT32 || fs == FileSystemType::FAT16 ||
        fs == FileSystemType::FAT12) {
        opm::fat32::FAT32BootSector boot_sector;
        opm::fat32::FAT32Layout layout;
        Result r = fat32::getFAT32Info(disk, start, boot_sector, layout);
        if (r.failed()) {
            std::cerr << "Error: " << r.message << "\n";
            return 1;
        }
        std::cout << "Bytes/sector:    " << layout.bytes_per_sector << "\n"
                  << "Sectors/cluster: " << layout.sectors_per_cluster << "\n"
                  << "Reserved sectors: " << layout.reserved_sectors << "\n"
                  << "Number of FATs:  " << layout.num_fats << "\n"
                  << "Sectors/FAT:     " << layout.sectors_per_fat << "\n"
                  << "Total sectors:   " << layout.total_sectors << "\n"
                  << "Root cluster:    " << layout.root_cluster << "\n";
    } else if (fs == FileSystemType::NTFS) {
        std::vector<uint8_t> sector(512, 0);
        disk->readSector(sector.data(), start);
        uint64_t total = 0;
        std::memcpy(&total, sector.data() + 40, 8);
        uint32_t spc = sector[13];
        std::cout << "Bytes/sector:    " << 512 << "\n"
                  << "Sectors/cluster: " << spc << "\n"
                  << "Total sectors:   " << total << "\n"
                  << "Volume size:     " << utils::formatBytes(total * 512) << "\n";
    } else if (fs == FileSystemType::EXT4 || fs == FileSystemType::EXT3 ||
               fs == FileSystemType::EXT2) {
        std::vector<uint8_t> sb(1024, 0);
        disk->read(sb.data(), start * 512 + 1024, 1024);
        uint32_t inodes = 0, blocks = 0, block_size_log = 0;
        std::memcpy(&inodes, sb.data() + 0, 4);
        std::memcpy(&blocks, sb.data() + 4, 4);
        block_size_log = sb[24];
        uint32_t block_size = 1024u << block_size_log;
        char label[17] = {0};
        std::memcpy(label, sb.data() + 120, 16);
        std::cout << "Block size:      " << block_size << "\n"
                  << "Blocks total:    " << blocks << "\n"
                  << "Inodes total:    " << inodes << "\n"
                  << "Volume size:     " << utils::formatBytes(
                         static_cast<uint64_t>(blocks) * block_size) << "\n"
                  << "Label:           '" << label << "'\n";
    } else if (fs == FileSystemType::exFAT) {
        std::vector<uint8_t> sector(512, 0);
        disk->readSector(sector.data(), start);
        // exFAT boot sector fields: volume_length at 72 (8B),
        // bytes_per_sector_shift at 108 (1B), sectors_per_cluster_shift at 109 (1B)
        uint64_t vol_length = 0;
        std::memcpy(&vol_length, sector.data() + 72, 8);
        uint8_t bps_shift = sector[108];
        uint8_t spc_shift = sector[109];
        uint32_t bytes_per_sector = 1u << bps_shift;
        uint32_t sectors_per_cluster = 1u << spc_shift;
        std::cout << "Bytes/sector:    " << bytes_per_sector << "\n"
                  << "Sectors/cluster: " << sectors_per_cluster << "\n"
                  << "Volume sectors:  " << vol_length << "\n"
                  << "Volume size:     " << utils::formatBytes(vol_length * bytes_per_sector) << "\n";
    }

    return 0;
}

// ---------------------------------------------------------------------------
// i18n <locale> [catalog_path] - list locales or load a message catalog
// ---------------------------------------------------------------------------
int cmdI18n(const std::vector<std::string>& args) {
    if (args.empty()) {
        auto locales = i18n::loadedLocales();
        std::cout << "Loaded locales (" << locales.size() << "):";
        for (const auto& l : locales) std::cout << " " << l;
        std::cout << "\nCurrent locale: " << i18n::currentLocale() << "\n";
        return 0;
    }
    if (args.size() < 2) {
        i18n::setLocale(args[0]);
        std::cout << "Locale set to " << args[0] << "\n";
        return 0;
    }
    int loaded = i18n::loadCatalog(args[0], args[1]);
    if (loaded < 0) {
        std::cerr << "Error: cannot read catalog file: " << args[1] << "\n";
        return 1;
    }
    i18n::setLocale(args[0]);
    std::cout << "Loaded " << loaded << " message(s) for locale "
              << args[0] << "\n";
    return 0;
}

// ---------------------------------------------------------------------------
// lvm - list physical volumes, volume groups, logical volumes
// ---------------------------------------------------------------------------
int cmdLVM(const std::vector<std::string>&) {
    auto pvs = detectPhysicalVolumes();
    std::cout << "=== LVM Physical Volumes (" << pvs.size() << ") ===\n";
    for (const auto& pv : pvs) {
        std::cout << "  " << pv.device_path
                  << "  vg=" << (pv.vg_name.empty() ? "(none)" : pv.vg_name)
                  << "  size=" << utils::formatBytes(pv.total_size)
                  << "  uuid=" << pv.uuid << "\n";
    }

    auto vgs = detectVolumeGroups();
    std::cout << "\n=== Volume Groups (" << vgs.size() << ") ===\n";
    for (const auto& vg : vgs) {
        std::cout << "  " << vg.name
                  << "  pvs=" << vg.pv_count
                  << "  size=" << utils::formatBytes(vg.total_size)
                  << "\n";
    }

    auto lvs = detectLogicalVolumes();
    std::cout << "\n=== Logical Volumes (" << lvs.size() << ") ===\n";
    for (const auto& lv : lvs) {
        std::cout << "  " << lv.device_path
                  << "  vg=" << lv.vg_name
                  << "  size=" << utils::formatBytes(lv.size)
                  << "\n";
    }
    return 0;
}

// ---------------------------------------------------------------------------
// raid - list software RAID arrays
// ---------------------------------------------------------------------------
int cmdRAID(const std::vector<std::string>&) {
    auto arrays = detectRaidArrays();
    if (arrays.empty()) {
        std::cout << "No software RAID arrays detected.\n";
        return 0;
    }
    std::cout << "Software RAID arrays (" << arrays.size() << "):\n";
    for (const auto& a : arrays) {
        std::string level;
        switch (a.level) {
            case RaidLevel::Linear: level = "linear"; break;
            case RaidLevel::Raid0: level = "raid0"; break;
            case RaidLevel::Raid1: level = "raid1"; break;
            case RaidLevel::Raid5: level = "raid5"; break;
            case RaidLevel::Raid6: level = "raid6"; break;
            case RaidLevel::Raid10: level = "raid10"; break;
            default: level = "unknown"; break;
        }
        std::cout << "  " << a.name << " (" << a.device_path
                  << ") level=" << level << " drives=" << a.total_drives
                  << " components:";
        for (const auto& c : a.components) {
            std::cout << " " << c.device_path;
        }
        std::cout << "\n";
    }
    return 0;
}

// ---------------------------------------------------------------------------
// cryptinfo <device> <start_sector> - detect encryption on a partition
// ---------------------------------------------------------------------------
int cmdCryptInfo(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "Usage: opm cryptinfo <device> <start_sector>\n";
        return 1;
    }
    uint64_t start = 0;
    if (!parseU64(args[1], start)) {
        std::cerr << "Error: invalid start sector: " << args[1] << "\n";
        return 1;
    }

    auto disk = DiskIO::openReadOnly(args[0]);
    if (!disk || !disk->isOpen()) {
        std::cerr << "Error: failed to open device: " << args[0] << "\n";
        return 1;
    }

    std::string description;
    Result r = describeEncryption(disk, start, description);
    if (r.failed()) {
        std::cerr << "Error: " << r.message << "\n";
        return 1;
    }
    std::cout << "Encryption: " << description << "\n";
    return 0;
}

// ---------------------------------------------------------------------------
// align <device> - report 4K (1MiB) alignment of every partition
// ---------------------------------------------------------------------------
int cmdAlign(const std::vector<std::string>& args) {
    if (args.size() < 1) {
        std::cerr << "Usage: opm align <device>\n";
        return 1;
    }

    auto disk = DiskIO::openReadOnly(args[0]);
    if (!disk || !disk->isOpen()) {
        std::cerr << "Error: failed to open device: " << args[0] << "\n";
        return 1;
    }

    std::unique_ptr<PartitionTable> table;
    try {
        table = PartitionTable::load(disk);
    } catch (...) {}
    if (!table) {
        std::cerr << "Error: no partition table found on " << args[0] << "\n";
        return 1;
    }

    auto parts = table->getPartitions();
    bool all_aligned = true;
    std::cout << "4K alignment report for " << args[0] << " ("
              << table->typeName() << ", " << parts.size() << " partition(s)):\n";
    for (size_t i = 0; i < parts.size(); i++) {
        const auto& p = parts[i];
        bool aligned = p.isAligned();
        if (!aligned) all_aligned = false;
        std::cout << "  Partition " << (i + 1) << " @ sector " << p.startSector()
                  << " (" << p.startSector() * disk->sectorSize() << " B): "
                  << (aligned ? "ALIGNED" : "NOT ALIGNED (start not a 1MiB multiple)")
                  << "\n";
    }
    if (all_aligned) {
        std::cout << "All partitions are 4K/1MiB aligned.\n";
    } else if (args.size() > 1 && args[1] == "--fix") {
        std::cout << "Fixing alignment: moving unaligned partitions to the next\n"
                     "1MiB boundary (data-preserving copy)...\n";
        // Move each unaligned partition right to the next 1MiB boundary.
        // Process in descending start order so later partitions don't collide
        // with the ones being moved.
        auto parts = table->getPartitions();
        std::vector<size_t> unaligned;
        for (size_t i = 0; i < parts.size(); i++) {
            if (!parts[i].isAligned()) unaligned.push_back(i);
        }
        int fixed_count = 0;
        for (size_t idx : unaligned) {
            // Re-fetch since the table changes as we move.
            auto cur = table->getPartitions();
            if (idx >= cur.size()) continue;
            const Partition& p = cur[idx];
            if (p.isAligned()) continue;
            uint64_t aligned_start = utils::alignUp(p.startSector(), ALIGNMENT_1MB);
            uint64_t count = p.sectorCount();
            uint64_t aligned_end = aligned_start + count - 1;
            bool ok = true;
            for (const auto& other : cur) {
                if (other.startSector() == p.startSector()) continue;
                if (aligned_start <= other.endSector() && aligned_end >= other.startSector()) {
                    ok = false;
                    break;
                }
            }
            if (!ok) {
                std::cout << "  Partition " << (idx + 1) << ": cannot align (no free "
                          << "space at sector " << aligned_start << ")\n";
                continue;
            }
            if (aligned_start == p.startSector()) continue;
            // Data-preserving move: chunked copy then re-create the entry.
            const size_t chunk = 1024 * 1024;
            std::vector<uint8_t> buffer(chunk);
            uint64_t remaining = count;
            uint64_t sector = 0;
            bool failed = false;
            while (remaining > 0) {
                uint32_t to_copy = static_cast<uint32_t>(
                    std::min<uint64_t>(chunk / disk->sectorSize(), remaining));
                Result rr = disk->readSectors(buffer.data(), p.startSector() + sector, to_copy);
                if (rr.failed()) { std::cerr << "  read error: " << rr.message << "\n"; failed = true; break; }
                rr = disk->writeSectors(buffer.data(), aligned_start + sector, to_copy);
                if (rr.failed()) { std::cerr << "  write error: " << rr.message << "\n"; failed = true; break; }
                sector += to_copy;
                remaining -= to_copy;
            }
            if (failed) continue;
            uint64_t size_bytes = count * disk->sectorSize();
            Result rr = table->createPartition(aligned_start, size_bytes, p.type(), p.name());
            if (rr.failed()) { std::cerr << "  create failed: " << rr.message << "\n"; continue; }
            int old_index = 0;
            if (!findPartitionByStart(*table, p.startSector(), old_index)) continue;
            rr = table->deletePartition(old_index);
            if (rr.failed()) { std::cerr << "  delete old failed: " << rr.message << "\n"; continue; }
            rr = table->commit();
            if (rr.failed()) { std::cerr << "  commit failed: " << rr.message << "\n"; continue; }
            std::cout << "  Partition " << (idx + 1) << ": moved " << p.startSector()
                      << " -> " << aligned_start << "\n";
            fixed_count++;
        }
        std::cout << "Alignment fix complete: " << fixed_count << " of "
                  << unaligned.size() << " unaligned partition(s) moved.\n";
        return 0;
    } else {
        std::cout << "Unaligned partitions detected. Run 'opm align <device> --fix' to\n"
                  << "move them to a 1MiB boundary (data-preserving).\n";
    }
    return all_aligned ? 0 : 1;
}

// ---------------------------------------------------------------------------
// convert <device> <mbr|gpt> - convert the partition table in place
// ---------------------------------------------------------------------------
int cmdConvert(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "Usage: opm convert <device> <mbr|gpt>\n";
        return 1;
    }
    TableType target;
    if (args[1] == "mbr") {
        target = TableType::MBR;
    } else if (args[1] == "gpt") {
        target = TableType::GPT;
    } else {
        std::cerr << "Error: target must be 'mbr' or 'gpt'\n";
        return 1;
    }

    std::string err;
    std::shared_ptr<DiskIO> disk;
    if (!openReadWrite(args[0], disk, err)) { std::cerr << err << "\n"; return 1; }

    try {
        auto source = PartitionTable::load(disk);
        if (!source) {
            std::cerr << "Error: no partition table found on " << args[0] << "\n";
            return 1;
        }
        if (source->type() == target) {
            std::cout << "Disk already uses a " << source->typeName() << " table.\n";
            return 0;
        }
        std::cout << "Converting " << source->typeName() << " -> "
                  << (target == TableType::MBR ? "MBR" : "GPT") << " ...\n";
        Result r = source->convertTo(target);
        if (r.failed()) {
            std::cerr << "Error: " << r.message << "\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    // Show the result.
    auto reloaded = PartitionTable::load(disk);
    if (reloaded) {
        std::cout << "Converted successfully: " << reloaded->typeName()
                  << " table with " << reloaded->getPartitionCount()
                  << " partition(s).\n";
    } else {
        std::cout << "Converted successfully.\n";
    }
    return 0;
}

// ---------------------------------------------------------------------------
// set-active <device> <number> [on|off] - toggle the MBR bootable flag
// ---------------------------------------------------------------------------
int cmdSetActive(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "Usage: opm set-active <device> <partition_number> [on|off]\n";
        return 1;
    }
    uint64_t number = 0;
    if (!parseU64(args[1], number) || number == 0) {
        std::cerr << "Error: invalid partition number: " << args[1] << "\n";
        return 1;
    }
    bool on = true;
    if (args.size() > 2) {
        if (args[2] == "on") on = true;
        else if (args[2] == "off") on = false;
        else { std::cerr << "Error: flag must be 'on' or 'off'\n"; return 1; }
    }

    std::string err;
    std::shared_ptr<DiskIO> disk;
    if (!openReadWrite(args[0], disk, err)) { std::cerr << err << "\n"; return 1; }
    std::unique_ptr<PartitionTable> table;
    if (!loadTable(disk, table, err)) { std::cerr << err << "\n"; return 1; }

    if (table->type() != TableType::MBR) {
        std::cerr << "Error: the active/bootable flag is an MBR concept; "
                  << "this disk uses " << table->typeName() << ".\n";
        return 1;
    }
    auto mbr = dynamic_cast<MBRTable*>(table.get());
    if (!mbr) { std::cerr << "Error: internal table mismatch\n"; return 1; }

    Result r = mbr->setPartitionBootable(static_cast<int>(number), on);
    if (r.failed()) { std::cerr << "Error: " << r.message << "\n"; return 1; }
    r = mbr->commit();
    if (r.failed()) { std::cerr << "Error: commit failed: " << r.message << "\n"; return 1; }
    std::cout << "Partition " << number << " is now "
              << (on ? "active (bootable)" : "inactive") << ".\n";
    return 0;
}

// ---------------------------------------------------------------------------
// hide <device> <number> | unhide <device> <number> - MBR FAT-family hidden bit
// ---------------------------------------------------------------------------
int cmdHideUnhide(const std::vector<std::string>& args, bool hide) {
    if (args.size() < 2) {
        std::cerr << "Usage: opm " << (hide ? "hide" : "unhide")
                  << " <device> <partition_number>\n";
        return 1;
    }
    uint64_t number = 0;
    if (!parseU64(args[1], number) || number == 0) {
        std::cerr << "Error: invalid partition number: " << args[1] << "\n";
        return 1;
    }

    std::string err;
    std::shared_ptr<DiskIO> disk;
    if (!openReadWrite(args[0], disk, err)) { std::cerr << err << "\n"; return 1; }
    std::unique_ptr<PartitionTable> table;
    if (!loadTable(disk, table, err)) { std::cerr << err << "\n"; return 1; }

    if (table->type() != TableType::MBR) {
        std::cerr << "Error: hiding is an MBR concept; this disk uses "
                  << table->typeName() << ".\n";
        return 1;
    }
    auto mbr = dynamic_cast<MBRTable*>(table.get());
    if (!mbr) { std::cerr << "Error: internal table mismatch\n"; return 1; }

    Result r = mbr->setPartitionHidden(static_cast<int>(number), hide);
    if (r.failed()) { std::cerr << "Error: " << r.message << "\n"; return 1; }
    r = mbr->commit();
    if (r.failed()) { std::cerr << "Error: commit failed: " << r.message << "\n"; return 1; }
    std::cout << "Partition " << number << (hide ? " hidden." : " unhidden.") << "\n";
    return 0;
}

// ---------------------------------------------------------------------------
// label <device> <start_sector> <label> - set the volume label
// ---------------------------------------------------------------------------
int cmdLabel(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        std::cerr << "Usage: opm label <device> <start_sector> <label>\n";
        return 1;
    }
    uint64_t start = 0;
    if (!parseU64(args[1], start)) {
        std::cerr << "Error: invalid start sector: " << args[1] << "\n";
        return 1;
    }
    std::string label = args[2];

    std::string err;
    std::shared_ptr<DiskIO> disk;
    if (!openReadWrite(args[0], disk, err)) { std::cerr << err << "\n"; return 1; }

    FileSystemType fs = disk->detectFilesystem(start);
    Result r;
    switch (fs) {
        case FileSystemType::FAT32:
            r = fat32::setLabel(disk, start, label);
            break;
        case FileSystemType::NTFS:
            r = ntfs::setLabel(disk, start, label);
            break;
        case FileSystemType::EXT2:
        case FileSystemType::EXT3:
        case FileSystemType::EXT4:
            r = ext4::setLabel(disk, start, label);
            break;
        case FileSystemType::exFAT:
            r = exfat::setLabel(disk, start, label);
            break;
        default:
            std::cerr << "Error: no label support for filesystem at sector "
                      << start << "\n";
            return 1;
    }
    if (r.failed()) {
        std::cerr << "Error: " << r.message << "\n";
        return 1;
    }
    std::cout << "Volume label set to '" << label << "' ("
              << fsTypeName(fs) << ").\n";
    return 0;
}

// ---------------------------------------------------------------------------
// recover <device> [--rebuild] - find and optionally rebuild partitions
// ---------------------------------------------------------------------------
int cmdRecover(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "Usage: opm recover <device> [--rebuild]\n";
        return 1;
    }
    bool rebuild = false;
    for (const auto& a : args) {
        if (a == "--rebuild") rebuild = true;
    }

    std::string err;
    std::shared_ptr<DiskIO> disk;
    if (!openReadWrite(args[0], disk, err)) { std::cerr << err << "\n"; return 1; }

    std::cout << "Scanning " << args[0] << " for partition signatures...\n";
    auto candidates = scanForPartitions(disk, 2048);
    if (candidates.empty()) {
        std::cout << "No partitions or filesystem signatures found.\n";
        return 1;
    }

    std::cout << "Found " << candidates.size() << " candidate(s):\n";
    for (size_t i = 0; i < candidates.size(); i++) {
        const auto& c = candidates[i];
        std::cout << "  [" << (i + 1) << "] sector " << c.start_sector
                  << " size=" << utils::formatBytes(c.size_sectors * disk->sectorSize())
                  << " fs=" << fsTypeName(c.fs)
                  << (c.bootable ? " bootable" : "")
                  << (c.from_partition_table ? " (table)" : "")
                  << "\n";
    }

    if (!rebuild) {
        std::cout << "Run 'opm recover <device> --rebuild' to write an MBR table "
                     "from these candidates.\n";
        return 0;
    }

    std::cout << "Rebuilding MBR partition table...\n";
    Result r = rebuildPartitionTable(disk, candidates);
    if (r.failed()) {
        std::cerr << "Error: " << r.message << "\n";
        return 1;
    }
    auto reloaded = PartitionTable::load(disk);
    std::cout << "Rebuilt MBR table with "
              << (reloaded ? reloaded->getPartitionCount() : 0)
              << " partition(s). Data preserved; run 'opm check' to verify.\n";
    return 0;
}

// ---------------------------------------------------------------------------
// undelete <device> <start_sector> [--restore <index>] [--name-char <c>]
// Scans a FAT32 volume for deleted files; with --restore, recovers one.
// ---------------------------------------------------------------------------
int cmdUndelete(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "Usage: opm undelete <device> <start_sector> "
                     "[--restore <index>] [--name-char <c>]\n";
        return 1;
    }
    uint64_t start = 0;
    if (!parseU64(args[1], start)) {
        std::cerr << "Error: invalid start sector: " << args[1] << "\n";
        return 1;
    }
    int restore_index = -1;
    char name_char = '_';
    for (size_t i = 2; i < args.size(); i++) {
        if (args[i] == "--restore" && i + 1 < args.size()) {
            if (!parseU64(args[i + 1], (uint64_t&)restore_index)) {
                std::cerr << "Error: invalid restore index\n";
                return 1;
            }
            restore_index = static_cast<int>(restore_index) - 1;  // 0-based
        }
        if (args[i] == "--name-char" && i + 1 < args.size()) {
            name_char = args[i + 1].empty() ? '_' : args[i + 1][0];
        }
    }

    std::string err;
    std::shared_ptr<DiskIO> disk;
    if (!openReadWrite(args[0], disk, err)) { std::cerr << err << "\n"; return 1; }

    FileSystemType fs = disk->detectFilesystem(start);
    if (fs != FileSystemType::FAT32 && fs != FileSystemType::FAT16 &&
        fs != FileSystemType::FAT12) {
        std::cerr << "Error: undelete requires a FAT volume (found "
                  << fsTypeName(fs) << ")\n";
        return 1;
    }

    auto files = fat32::scanDeletedFiles(disk, start);
    std::cout << "Found " << files.size() << " deleted file(s):\n";
    for (size_t i = 0; i < files.size(); i++) {
        const auto& f = files[i];
        std::cout << "  [" << (i + 1) << "] " << f.name
                  << " size=" << utils::formatBytes(f.size_bytes)
                  << " start_cluster=" << f.start_cluster
                  << (f.parent_cluster ? " (subdir)" : " (root)")
                  << "\n";
    }
    if (files.empty()) {
        std::cout << "Nothing to restore. Data may have been overwritten.\n";
        return 1;
    }
    if (restore_index < 0) {
        std::cout << "Run 'opm undelete <device> <start> --restore <index>' "
                     "to recover a file.\n";
        return 0;
    }
    if (restore_index >= static_cast<int>(files.size())) {
        std::cerr << "Error: restore index out of range\n";
        return 1;
    }

    Result r = fat32::restoreDeletedFile(disk, start, files[restore_index], name_char);
    if (r.failed()) {
        std::cerr << "Error: " << r.message << "\n";
        return 1;
    }
    std::cout << "Restored '" << files[restore_index].name << "' (first char -> '"
              << name_char << "').\n";
    return 0;
}

// ---------------------------------------------------------------------------
// luks <open|close|status> ... — cryptsetup wrappers
// bitlocker unlock <device> [mount] [--recovery <key>]
// boot-repair --uefi [--add <label> <device> <loader>]
// reset-password --windows <sam> [user] | --linux <shadow> <user> [pass]
// ---------------------------------------------------------------------------
int cmdLuks(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "Usage: opm luks <open <device> <name>|close <name>|status <name>>\n";
        return 1;
    }
    if (args[0] == "open") {
        if (args.size() < 3) { std::cerr << "Usage: opm luks open <device> <name>\n"; return 1; }
        Result r = luksOpen(args[1], args[2]);
        if (r.failed()) { std::cerr << "Error: " << r.message << "\n"; return 1; }
        std::cout << "LUKS volume opened as " << args[2] << " (/dev/mapper/" << args[2] << ")\n";
        return 0;
    }
    if (args[0] == "close") {
        Result r = luksClose(args[1]);
        if (r.failed()) { std::cerr << "Error: " << r.message << "\n"; return 1; }
        std::cout << "LUKS mapping " << args[1] << " closed.\n";
        return 0;
    }
    if (args[0] == "status") {
        std::string out;
        Result r = luksStatus(args[1], out);
        if (r.failed()) { std::cerr << "Error: " << r.message << "\n"; return 1; }
        std::cout << out;
        return 0;
    }
    std::cerr << "Error: unknown luks subcommand '" << args[0] << "'\n";
    return 1;
}

int cmdBitlocker(const std::vector<std::string>& args) {
    if (args.empty() || args[0] != "unlock") {
        std::cerr << "Usage: opm bitlocker unlock <device> [mount_dir] [--recovery <key>]\n";
        return 1;
    }
    if (args.size() < 2) {
        std::cerr << "Usage: opm bitlocker unlock <device> [mount_dir] [--recovery <key>]\n";
        return 1;
    }
    std::string device = args[1];
    std::string mount = "/mnt/bitlocker";
    std::string recovery;
    for (size_t i = 2; i < args.size(); i++) {
        if (args[i] == "--recovery" && i + 1 < args.size()) {
            recovery = args[i + 1];
            i++;
        } else if (!args[i].empty() && args[i][0] != '-') {
            mount = args[i];
        }
    }
    Result r = bitlockerUnlock(device, mount, recovery);
    if (r.failed()) { std::cerr << "Error: " << r.message << "\n"; return 1; }
    std::cout << "BitLocker volume unlocked; mount with: mount -o loop "
              << mount << "/dislocker-file /mnt/win\n";
    return 0;
}

int cmdBootRepair(const std::vector<std::string>& args) {
    bool uefi = false;
    bool add = false;
    std::vector<std::string> add_args;
    for (const auto& a : args) {
        if (a == "--uefi") uefi = true;
        else if (a == "--add") add = true;
        else add_args.push_back(a);
    }
    if (!uefi) {
        std::cerr << "Usage: opm boot-repair --uefi [--add <label> <device> <loader>]\n";
        return 1;
    }
    if (add) {
        if (add_args.size() < 3) {
            std::cerr << "Usage: opm boot-repair --uefi --add <label> <device> <loader>\n";
            return 1;
        }
        Result r = uefiAddEntry(add_args[0], add_args[1], add_args[2]);
        if (r.failed()) { std::cerr << "Error: " << r.message << "\n"; return 1; }
        std::cout << "UEFI boot entry '" << add_args[0] << "' created.\n";
        return 0;
    }
    std::string out;
    Result r = uefiListEntries(out);
    if (r.failed()) { std::cerr << "Error: " << r.message << "\n"; return 1; }
    std::cout << out;
    return 0;
}

int cmdResetPassword(const std::vector<std::string>& args) {
    if (args.empty() || args[0] == "--help" || args[0] == "-h") {
        std::cerr << "Usage:\n"
                  << "  opm reset-password --windows <sam_hive> [user]\n"
                  << "  opm reset-password --linux <shadow_file> <user> [password]\n";
        return 1;
    }
    if (args[0] == "--windows") {
        if (args.size() < 2) {
            std::cerr << "Usage: opm reset-password --windows <sam_hive> [user]\n";
            return 1;
        }
        std::string user = args.size() > 2 ? args[2] : "Administrator";
        Result r = windowsResetPassword(args[1], user);
        if (r.failed()) { std::cerr << "Error: " << r.message << "\n"; return 1; }
        std::cout << "Windows password reset prepared for '" << user
                  << "' (follow the chntpw prompts).\n";
        return 0;
    }
    if (args[0] == "--linux") {
        if (args.size() < 3) {
            std::cerr << "Usage: opm reset-password --linux <shadow_file> <user> [password]\n";
            return 1;
        }
        std::string password = args.size() > 3 ? args[3] : "opm-reset";
        Result r = resetLinuxPassword(args[1], args[2], password);
        if (r.failed()) { std::cerr << "Error: " << r.message << "\n"; return 1; }
        std::cout << "Password for '" << args[2] << "' reset in " << args[1] << ".\n";
        return 0;
    }
    std::cerr << "Error: unknown reset-password mode '" << args[0] << "'\n";
    return 1;
}

int cmdMerge(const std::vector<std::string>& args) {
    if (args.empty() || args[0] == "--help" || args[0] == "-h") {
        std::cerr << "Usage:\n"
                  << "  opm merge <device> <numA> <numB> [--into <folder>]\n"
                  << "    Merge two adjacent partitions. The left partition survives and\n"
                  << "    grows right. If the right partition is empty, the merge is a\n"
                  << "    table-level grow (any filesystem). If both are FAT32, the right\n"
                  << "    partition's data is copied into a folder on the left first.\n";
        return 1;
    }
    if (args.size() < 3) {
        std::cerr << "Usage: opm merge <device> <numA> <numB> [--into <folder>]\n";
        return 1;
    }
    uint64_t na = 0, nb = 0;
    if (!parseU64(args[1], na) || !parseU64(args[2], nb)) {
        std::cerr << "Error: partition numbers must be integers\n";
        return 1;
    }
    MergeOptions opts;
    if (args.size() > 4 && args[3] == "--into") opts.folder_name = args[4];
    auto disk = DiskIO::openReadWrite(args[0]);
    if (!disk || !disk->isOpen()) {
        std::cerr << "Error: cannot open " << args[0] << "\n";
        return 1;
    }
    Result r = mergePartitions(disk, static_cast<int>(na), static_cast<int>(nb), opts);
    if (r.failed()) { std::cerr << "Error: " << r.message << "\n"; return 1; }
    std::cout << "Merged partitions " << na << " and " << nb << " on " << args[0] << "\n";
    return 0;
}

int cmdWipe(const std::vector<std::string>& args) {
    if (args.empty() || args[0] == "--help" || args[0] == "-h") {
        std::cerr << "Usage:\n"
                  << "  opm wipe <device> [--method <name>] [start_sector] [size_sectors]\n"
                  << "  opm wipe <device> --list-methods\n"
                  << "  Methods: zeros, random, dod522022, dod522022-ece, gutmann,\n"
                  << "           nist80088, nist80088-purge, rcmp-tssit, vsitr,\n"
                  << "           gost-p50739, us-army-ar380, ata-erase\n"
                  << "  Default: zeros. ATA-erase requires a TRIM-capable block device.\n";
        return 1;
    }
    if (args[0] == "--list-methods" || (args.size() > 1 && args[1] == "--list-methods")) {
        std::cout << "zeros\nrandom\ndod522022\ndod522022-ece\ngutmann\n"
                     "nist80088\nnist80088-purge\nrcmp-tssit\nvsitr\n"
                     "gost-p50739\nus-army-ar380\nata-erase\n";
        return 0;
    }
    std::string dev = args[0];
    EraseMethod method = EraseMethod::Zeros;
    size_t i = 1;
    if (i < args.size() && args[i] == "--method") {
        if (i + 1 >= args.size()) { std::cerr << "Error: --method requires a value\n"; return 1; }
        std::string m = args[i + 1];
        if (m == "zeros") method = EraseMethod::Zeros;
        else if (m == "random") method = EraseMethod::Random;
        else if (m == "dod522022") method = EraseMethod::DoD522022;
        else if (m == "dod522022-ece") method = EraseMethod::DoD522022ECE;
        else if (m == "gutmann") method = EraseMethod::Gutmann;
        else if (m == "nist80088") method = EraseMethod::NIST80088;
        else if (m == "nist80088-purge") method = EraseMethod::NIST80088Purge;
        else if (m == "rcmp-tssit") method = EraseMethod::RCMP_TSSIT;
        else if (m == "vsitr") method = EraseMethod::VSITR;
        else if (m == "gost-p50739") method = EraseMethod::GOST_P50739;
        else if (m == "us-army-ar380") method = EraseMethod::US_Army_AR380;
        else if (m == "ata-erase") method = EraseMethod::ATA_Erase;
        else { std::cerr << "Error: unknown method '" << m << "'\n"; return 1; }
        i += 2;
    }
    uint64_t start = 0, count = 0;
    if (i < args.size()) { if (!parseU64(args[i], start)) { std::cerr << "Error: bad start sector\n"; return 1; } i++; }
    if (i < args.size()) { if (!parseU64(args[i], count)) { std::cerr << "Error: bad size\n"; return 1; } i++; }

    auto disk = DiskIO::openReadWrite(dev);
    if (!disk || !disk->isOpen()) { std::cerr << "Error: cannot open " << dev << "\n"; return 1; }
    EraseOptions opts; opts.method = method;
    Result r;
    if (count > 0) {
        r = secureErase(disk, start, count, opts);
    } else {
        r = secureEraseDisk(disk, opts);
    }
    if (r.failed()) { std::cerr << "Error: " << r.message << "\n"; return 1; }
    std::cout << "Wiped " << dev << " (method " << (int)method << ")" << std::endl;
    return 0;
}

int cmdTrim(const std::vector<std::string>& args) {
    if (args.empty() || args[0] == "--help" || args[0] == "-h") {
        std::cerr << "Usage: opm trim <device> [start_sector] [count]\n"
                  << "  Sends BLKDISCARD to a TRIM-capable block device (SSD/NVMe).\n";
        return 1;
    }
    uint64_t start = 0, count = 0;
    size_t i = 1;
    if (i < args.size()) { if (!parseU64(args[i], start)) { std::cerr << "Error: bad start\n"; return 1; } i++; }
    if (i < args.size()) { if (!parseU64(args[i], count)) { std::cerr << "Error: bad count\n"; return 1; } i++; }
    auto disk = DiskIO::openReadWrite(args[0]);
    if (!disk || !disk->isOpen()) { std::cerr << "Error: cannot open " << args[0] << "\n"; return 1; }
    if (!disk->supportsTRIM()) {
        std::cerr << "Error: " << args[0] << " does not support TRIM/BLKDISCARD (not a block device or unsupported)\n";
        return 1;
    }
    if (count == 0) count = disk->sectorCount() - start;
    Result r = disk->trim(start, count);
    if (r.failed()) { std::cerr << "Error: " << r.message << "\n"; return 1; }
    std::cout << "TRIM sent to " << args[0] << " [" << start << ".." << (start + count) << ")\n";
    return 0;
}

int cmdBackup(const std::vector<std::string>& args) {
    if (args.empty() || args[0] == "--help" || args[0] == "-h") {
        std::cerr << "Usage:\n"
                  << "  opm backup create <device> <image> [--block-size N]\n"
                  << "  opm backup incremental <device> <base-image> <image> [--diff]\n"
                  << "  opm backup differential <device> <base-image> <image>\n"
                  << "  opm backup restore <image> <device>\n"
                  << "  opm backup info <image>\n"
                  << "  opm backup verify <image>\n"
                  << "  opm backup files <source-dir> <archive.tar>      (file-level backup)\n"
                  << "  opm backup listfiles <archive.tar>\n"
                  << "  opm backup extract <archive.tar> <dest-dir>\n"
                  << "  opm backup schedule add|list|remove|show|run ... (scheduled backups)\n";
        return 1;
    }
    const std::string& sub = args[0];
    BackupOptions opts;
    auto extract = [&](const std::vector<std::string>& a, size_t& i) {
        if (i + 1 < a.size() && a[i] == "--block-size") {
            uint64_t v = 0;
            if (parseSize(a[i + 1], v) && v > 0) opts.block_size = static_cast<uint32_t>(v);
            i += 2;
            return true;
        }
        return false;
    };

    if (sub == "create") {
        if (args.size() < 3) {
            std::cerr << "Usage: opm backup create <device> <image>\n";
            return 1;
        }
        std::string dev = args[1], img = args[2];
        size_t i = 3;
        while (i < args.size()) extract(args, i);
        auto disk = DiskIO::openReadWrite(dev);
        if (!disk || !disk->isOpen()) {
            std::cerr << "Error: cannot open " << dev << "\n";
            return 1;
        }
        Result r = backupCreateFull(disk, img, opts);
        if (r.failed()) { std::cerr << "Error: " << r.message << "\n"; return 1; }
        std::cout << "Full backup of " << dev << " written to " << img << "\n";
        return 0;
    }
    if (sub == "incremental" || sub == "differential") {
        bool diff = sub == "differential";
        if (args.size() < 4) {
            std::cerr << "Usage: opm backup " << sub << " <device> <base-image> <image>\n";
            return 1;
        }
        std::string dev = args[1], base = args[2], img = args[3];
        size_t i = 4;
        while (i < args.size()) extract(args, i);
        auto disk = DiskIO::openReadWrite(dev);
        if (!disk || !disk->isOpen()) {
            std::cerr << "Error: cannot open " << dev << "\n";
            return 1;
        }
        Result r = backupCreateIncremental(disk, base, img, diff, opts);
        if (r.failed()) { std::cerr << "Error: " << r.message << "\n"; return 1; }
        std::cout << (diff ? "Differential" : "Incremental") << " backup of " << dev
                  << " written to " << img << " (base: " << base << ")\n";
        return 0;
    }
    if (sub == "restore") {
        if (args.size() < 3) {
            std::cerr << "Usage: opm backup restore <image> <device>\n";
            return 1;
        }
        auto disk = DiskIO::openReadWrite(args[2]);
        if (!disk || !disk->isOpen()) {
            std::cerr << "Error: cannot open " << args[2] << "\n";
            return 1;
        }
        Result r = backupRestore(args[1], disk, opts);
        if (r.failed()) { std::cerr << "Error: " << r.message << "\n"; return 1; }
        std::cout << "Restored " << args[1] << " to " << args[2] << "\n";
        return 0;
    }
    if (sub == "info") {
        if (args.size() < 2) {
            std::cerr << "Usage: opm backup info <image>\n";
            return 1;
        }
        BackupInfo info;
        Result r = backupInfo(args[1], info);
        if (r.failed()) { std::cerr << "Error: " << r.message << "\n"; return 1; }
        std::cout << "Image:      " << args[1] << "\n"
                  << "Mode:       " << info.mode_string() << "\n"
                  << "Source:     " << info.source_name << "\n"
                  << "Size:       " << info.source_size << " bytes\n"
                  << "Blocks:     " << info.num_blocks << " (" << info.block_size << "B each)\n"
                  << "Stored:     " << info.present_blocks << "\n"
                  << "Created:    " << info.created_at << "\n";
        return 0;
    }
    if (sub == "verify") {
        if (args.size() < 2) {
            std::cerr << "Usage: opm backup verify <image>\n";
            return 1;
        }
        Result r = backupVerify(args[1]);
        if (r.failed()) { std::cerr << "Error: " << r.message << "\n"; return 1; }
        std::cout << "Image OK: all " << "stored blocks verified\n";
        return 0;
    }
    if (sub == "files") {
        // File/folder-level backup: opm backup files <src_dir> <archive>
        if (args.size() < 3) {
            std::cerr << "Usage: opm backup files <source-dir> <archive.tar>\n";
            return 1;
        }
        Result r = tarCreate(args[1], args[2]);
        if (r.failed()) { std::cerr << "Error: " << r.message << "\n"; return 1; }
        std::cout << "Archived " << args[1] << " -> " << args[2] << "\n";
        return 0;
    }
    if (sub == "extract") {
        if (args.size() < 3) {
            std::cerr << "Usage: opm backup extract <archive.tar> <dest-dir>\n";
            return 1;
        }
        Result r = tarExtract(args[1], args[2]);
        if (r.failed()) { std::cerr << "Error: " << r.message << "\n"; return 1; }
        std::cout << "Extracted " << args[1] << " -> " << args[2] << "\n";
        return 0;
    }
    if (sub == "listfiles") {
        if (args.size() < 2) {
            std::cerr << "Usage: opm backup listfiles <archive.tar>\n";
            return 1;
        }
        std::vector<TarEntryInfo> entries;
        Result r = tarList(args[1], entries);
        if (r.failed()) { std::cerr << "Error: " << r.message << "\n"; return 1; }
        for (const auto& e : entries) {
            std::cout << (e.is_dir ? "d " : e.is_symlink ? "l " : "- ")
                      << e.size << "  " << e.name;
            if (e.is_symlink) std::cout << " -> " << e.linkname;
            std::cout << "\n";
        }
        return 0;
    }
    if (sub == "schedule") {
        if (args.size() < 2 || args[1] == "--help" || args[1] == "-h") {
            std::cerr << "Usage:\n"
                      << "  opm backup schedule add <name> <cron> <command...>\n"
                      << "                         e.g. opm backup schedule add nightly '0 2 * * *' 'opm backup create /dev/sda /backup/img'\n"
                      << "  opm backup schedule list\n"
                      << "  opm backup schedule remove <name>\n"
                      << "  opm backup schedule show <name> [--cron|--systemd]\n"
                      << "  opm backup schedule run <name>\n"
                      << "  (cron fields: minute hour day-of-month month day-of-week; * and */N supported)\n";
            return 1;
        }
        const std::string& sub2 = args[1];
        if (sub2 == "add") {
            if (args.size() < 5) {
                std::cerr << "Usage: opm backup schedule add <name> '<minute hour dom month dow>' <command...>\n";
                return 1;
            }
            ScheduleEntry e;
            e.name = args[2];
            // Parse the quoted cron expression (single arg) into 5 fields.
            std::istringstream cron(args[3]);
            std::vector<std::string> fields;
            std::string f;
            while (cron >> f) fields.push_back(f);
            if (fields.size() != 5) {
                std::cerr << "Error: cron expression must have exactly 5 fields\n";
                return 1;
            }
            e.minute = fields[0]; e.hour = fields[1]; e.dom = fields[2];
            e.month = fields[3]; e.dow = fields[4];
            for (size_t i = 4; i < args.size(); ++i) {
                if (i > 4) e.command += " ";
                e.command += args[i];
            }
            std::string err;
            if (!e.valid(err)) { std::cerr << "Error: " << err << "\n"; return 1; }
            Result r = scheduleAdd(e);
            if (r.failed()) { std::cerr << "Error: " << r.message << "\n"; return 1; }
            std::cout << "Schedule '" << e.name << "' saved: " << e.describe() << "\n";
            std::cout << "  cron line: " << e.cronLine() << "\n";
            std::cout << "  registry:  " << scheduleRegistryPath() << "\n";
            std::cout << "Install the live backend with:\n"
                      << "  opm backup schedule install-systemd " << e.name << "\n"
                      << "  opm backup schedule install-crontab " << e.name << "\n";
            return 0;
        }
        if (sub2 == "list") {
            std::vector<ScheduleEntry> entries;
            Result r = scheduleList(entries);
            if (r.failed()) { std::cerr << "Error: " << r.message << "\n"; return 1; }
            if (entries.empty()) { std::cout << "No schedules configured.\n"; return 0; }
            std::cout << "Schedules (" << scheduleRegistryPath() << "):\n";
            for (const auto& e : entries) {
                std::cout << "  " << e.name << "  " << e.describe() << "  ->  " << e.command << "\n";
            }
            return 0;
        }
        if (sub2 == "remove") {
            if (args.size() < 3) { std::cerr << "Usage: opm backup schedule remove <name>\n"; return 1; }
            Result r = scheduleRemove(args[2]);
            if (r.failed()) { std::cerr << "Error: " << r.message << "\n"; return 1; }
            std::cout << "Removed schedule '" << args[2] << "' (registry). Disable any live timer with:\n"
                      << "  systemctl --user disable --now opm-backup-" << args[2] << ".timer\n";
            return 0;
        }
        if (sub2 == "show") {
            if (args.size() < 3) { std::cerr << "Usage: opm backup schedule show <name> [--cron|--systemd]\n"; return 1; }
            ScheduleEntry e;
            if (!scheduleFind(args[2], e)) { std::cerr << "Error: schedule '" << args[2] << "' not found\n"; return 1; }
            bool want_cron = args.size() > 3 && args[3] == "--cron";
            bool want_systemd = args.size() > 3 && args[3] == "--systemd";
            if (want_cron) { std::cout << e.cronLine() << "\n"; return 0; }
            if (want_systemd) {
                auto inst = scheduleInstallSystemd(e);
                std::cout << "units:\n  " << inst.written_units << "\nnote: " << inst.note << "\n";
                return 0;
            }
            std::cout << "Name:    " << e.name << "\n"
                      << "Cron:    " << e.cronLine() << "\n"
                      << "Runs:    " << e.describe() << "\n"
                      << "Command: " << e.command << "\n";
            return 0;
        }
        if (sub2 == "install-systemd") {
            if (args.size() < 3) { std::cerr << "Usage: opm backup schedule install-systemd <name>\n"; return 1; }
            ScheduleEntry e;
            if (!scheduleFind(args[2], e)) { std::cerr << "Error: schedule '" << args[2] << "' not found\n"; return 1; }
            auto inst = scheduleInstallSystemd(e);
            if (inst.status.failed()) { std::cerr << "Error: " << inst.status.message << "\n"; return 1; }
            std::cout << inst.note << "\n";
            return 0;
        }
        if (sub2 == "install-crontab") {
            if (args.size() < 3) { std::cerr << "Usage: opm backup schedule install-crontab <name>\n"; return 1; }
            ScheduleEntry e;
            if (!scheduleFind(args[2], e)) { std::cerr << "Error: schedule '" << args[2] << "' not found\n"; return 1; }
            auto inst = scheduleInstallCrontab(e);
            if (inst.status.failed()) { std::cerr << "Error: " << inst.status.message << "\n"; return 1; }
            std::cout << inst.note << "\n";
            return 0;
        }
        if (sub2 == "run") {
            if (args.size() < 3) { std::cerr << "Usage: opm backup schedule run <name>\n"; return 1; }
            ScheduleEntry e;
            if (!scheduleFind(args[2], e)) { std::cerr << "Error: schedule '" << args[2] << "' not found\n"; return 1; }
            std::cout << "Running schedule '" << e.name << "': " << e.command << "\n";
            int rc = std::system(e.command.c_str());
            return rc == 0 ? 0 : 1;
        }
        std::cerr << "Error: unknown schedule subcommand '" << sub2 << "'\n";
        return 1;
    }
    std::cerr << "Error: unknown backup subcommand '" << sub << "'\n";
    return 1;
}

} // namespace cli
} // namespace opm
