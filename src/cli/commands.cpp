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
// create <device> <start> <size> <type> [name]
// ---------------------------------------------------------------------------
int cmdCreate(const std::vector<std::string>& args) {
    if (args.size() < 4) {
        std::cerr << "Usage: opm create <device> <start_sector> <size> <type> [name]\n";
        return 1;
    }
    uint64_t start = 0, size = 0;
    if (!parseU64(args[1], start)) {
        std::cerr << "Error: invalid start sector: " << args[1] << "\n";
        return 1;
    }
    if (!parseSize(args[2], size)) {
        std::cerr << "Error: invalid size: " << args[2] << "\n";
        return 1;
    }
    bool ok = false;
    PartitionType type = parsePartitionType(args[3], ok);
    if (!ok) {
        std::cerr << "Error: unknown partition type '" << args[3]
                  << "' (ntfs, fat32, linux, swap, efi, lvm, raid)\n";
        return 1;
    }
    std::string name = args.size() > 4 ? args[4] : "";

    std::string err;
    std::shared_ptr<DiskIO> disk;
    if (!openReadWrite(args[0], disk, err)) { std::cerr << err << "\n"; return 1; }
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
// delete <device> <number>
// ---------------------------------------------------------------------------
int cmdDelete(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "Usage: opm delete <device> <partition_number>\n";
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
// resize <device> <number> <new_size>
// ---------------------------------------------------------------------------
int cmdResize(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        std::cerr << "Usage: opm resize <device> <partition_number> <new_size>\n";
        return 1;
    }
    uint64_t number = 0, new_size = 0;
    if (!parseU64(args[1], number) || number == 0) {
        std::cerr << "Error: invalid partition number: " << args[1] << "\n";
        return 1;
    }
    if (!parseSize(args[2], new_size)) {
        std::cerr << "Error: invalid size: " << args[2] << "\n";
        return 1;
    }

    std::string err;
    std::shared_ptr<DiskIO> disk;
    if (!openReadWrite(args[0], disk, err)) { std::cerr << err << "\n"; return 1; }
    std::unique_ptr<PartitionTable> table;
    if (!loadTable(disk, table, err)) { std::cerr << err << "\n"; return 1; }

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
    } else {
        std::cerr << "Error: unknown filesystem '" << fs_name
                  << "' (fat32, ntfs, ext4, exfat)\n";
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

} // namespace cli
} // namespace opm
