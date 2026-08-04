#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <memory>
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
#include "opm/i18n.hpp"

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

} // namespace cli
} // namespace opm
