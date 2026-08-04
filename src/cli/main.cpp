#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include "opm/partition_table.hpp"
#include "opm/disk_io.hpp"
#include "opm/utils.hpp"
#include "opm/types.hpp"
#include "opm/i18n.hpp"

using namespace opm;

namespace opm {
namespace cli {
int cmdCreate(const std::vector<std::string>& args);
int cmdDelete(const std::vector<std::string>& args);
int cmdResize(const std::vector<std::string>& args);
int cmdMove(const std::vector<std::string>& args);
int cmdFormat(const std::vector<std::string>& args);
int cmdCheck(const std::vector<std::string>& args);
int cmdFSInfo(const std::vector<std::string>& args);
int cmdMklabel(const std::vector<std::string>& args);
int cmdLVM(const std::vector<std::string>& args);
int cmdRAID(const std::vector<std::string>& args);
int cmdCryptInfo(const std::vector<std::string>& args);
int cmdAlign(const std::vector<std::string>& args);
int cmdI18n(const std::vector<std::string>& args);
int cmdConvert(const std::vector<std::string>& args);
int cmdSetActive(const std::vector<std::string>& args);
int cmdHideUnhide(const std::vector<std::string>& args, bool hide);
}
}

void printUsage(const char* program) {
    std::cout << "Open Partition Manager - CLI Tool\n"
              << "Usage: " << program << " <command> [options]\n\n"
              << "Device commands:\n"
              << "  list                  List all disks and partitions\n"
              << "  info <device>         Show device information\n"
              << "  read <device>         Read and display partition table\n"
              << "Partition commands:\n"
              << "  mklabel <device> <mbr|gpt>        Write a new partition table\n"
              << "  create <device> <start_sector> <size> <type> [name]\n"
              << "                         Create a partition (type: ntfs, fat32,\n"
              << "                         linux, swap, efi, lvm, raid)\n"
              << "  delete <device> <number>          Delete a partition\n"
              << "  resize <device> <number> <size>   Resize a partition\n"
              << "  move <device> <number> <start>    Move a partition (copies data)\n"
              << "  convert <device> <mbr|gpt>        Convert the partition table in place\n"
              << "  set-active <device> <n> [on|off]  Toggle the MBR bootable flag\n"
              << "  hide/unhide <device> <n>          Hide/unhide a FAT-family partition\n"
              << "\nFilesystem commands:\n"
              << "  format <device> <fs> <start_sector> [size] [label]\n"
              << "                         Format a partition (fs: fat32, ntfs,\n"
              << "                         ext4, exfat)\n"
              << "  check <device> <start_sector>     Check a filesystem\n"
              << "  fsinfo <device> <start_sector>    Show filesystem information\n"
              << "  cryptinfo <device> <start>        Detect encryption (BitLocker/LUKS)\n"
              << "  align <device>                    Report 4K partition alignment\n"
              << "System commands:\n"
              << "  lvm                              List LVM PVs/VGs/LVs\n"
              << "  raid                             List software RAID arrays\n"
              << "  i18n [locale] [catalog]          Manage message catalogs\n"
              << "\n<size> accepts byte suffixes: 512M, 10G, 2T, or plain bytes.\n"
              << "\nExamples:\n"
              << "  " << program << " list\n"
              << "  " << program << " read /dev/sda\n"
              << "  " << program << " create /dev/sdb 2048 10G linux mydata\n"
              << "  " << program << " format /dev/sdb ext4 2048 10G data\n"
              << "  " << program << " check /dev/sdb 2048\n";
}

void printPartition(const Partition& part, int number) {
    std::cout << "  Partition " << number << ":\n"
              << "    Device:      " << part.device() << "\n"
              << "    Start:       " << part.startSector() << "\n"
              << "    End:         " << part.endSector() << "\n"
              << "    Sectors:     " << part.sectorCount() << "\n"
              << "    Size:        " << part.formattedSize() << "\n"
              << "    Type:        " << static_cast<int>(part.type()) << "\n"
              << "    Filesystem:  " << static_cast<int>(part.filesystem()) << "\n"
              << "    Bootable:    " << (part.isBootable() ? "Yes" : "No") << "\n"
              << "    Aligned:     " << (part.isAligned() ? "Yes" : "No") << "\n"
              << "\n";
}

void cmdList() {
    std::cout << "Scanning for storage devices...\n\n";
    
    auto devices = DeviceEnumerator::enumerateDevices();
    
    if (devices.empty()) {
        std::cout << "No storage devices found.\n"
                  << "Note: This program requires root privileges to access disks.\n";
        return;
    }
    
    std::cout << "Found " << devices.size() << " storage device(s):\n\n";
    
    for (size_t i = 0; i < devices.size(); i++) {
        const auto& dev = devices[i];
        std::cout << "Device " << (i + 1) << ": " << dev.path << "\n"
                  << "  Model:    " << (dev.model.empty() ? "Unknown" : dev.model) << "\n"
                  << "  Size:     " << utils::formatBytes(dev.size) << "\n"
                  << "  Type:     " << (dev.ssd ? "SSD" : "HDD") << "\n"
                  << "  Removable: " << (dev.removable ? "Yes" : "No") << "\n";
        
        // Try to read partition table
        try {
            auto disk = DiskIO::openReadOnly(dev.path);
            if (disk) {
                auto table = PartitionTable::load(disk);
                if (table && table->isValid()) {
                    std::cout << "  Table:    " << table->typeName() << "\n"
                              << "  Partitions: " << table->getPartitionCount() << "\n";
                } else {
                    std::cout << "  Table:    None or invalid\n";
                }
            }
        } catch (const std::exception& e) {
            std::cout << "  Error:    " << e.what() << "\n";
        }
        
        std::cout << "\n";
    }
}

void cmdInfo(const std::string& device) {
    std::cout << "Device Information: " << device << "\n\n";
    
    try {
        auto disk = DiskIO::openReadOnly(device);
        if (!disk) {
            std::cerr << "Error: Failed to open device: " << device << "\n";
            std::cerr << "Make sure you have root privileges.\n";
            return;
        }
        
        auto info = disk->getDeviceInfo();
        
        std::cout << "Path:        " << info.path << "\n"
                  << "Model:       " << (info.model.empty() ? "Unknown" : info.model) << "\n"
                  << "Serial:      " << (info.serial.empty() ? "Unknown" : info.serial) << "\n"
                  << "Size:        " << utils::formatBytes(info.size) << " ("
                  << info.size << " bytes)\n"
                  << "Sector Size: " << info.geometry.bytes_per_sector << " bytes\n"
                  << "Sectors:     " << info.geometry.total_sectors << "\n"
                  << "Type:        " << (info.ssd ? "SSD" : "HDD") << "\n"
                  << "Removable:   " << (info.removable ? "Yes" : "No") << "\n"
                  << "Read-Only:   " << (info.readonly ? "Yes" : "No") << "\n";
        
        if (disk->hasGeometry()) {
            const auto& geo = disk->geometry();
            std::cout << "\nGeometry:\n"
                      << "  Cylinders: " << geo.cylinders << "\n"
                      << "  Heads:     " << geo.heads << "\n"
                      << "  Sectors:   " << geo.sectors_per_track << "\n"
                      << "  LBA:       " << (geo.lba_supported ? "Yes" : "No") << "\n";
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
}

void cmdRead(const std::string& device) {
    std::cout << "Reading partition table from: " << device << "\n\n";
    
    try {
        auto disk = DiskIO::openReadOnly(device);
        if (!disk) {
            std::cerr << "Error: Failed to open device: " << device << "\n";
            std::cerr << "Make sure you have root privileges.\n";
            return;
        }
        
        auto table = PartitionTable::load(disk);
        
        if (!table) {
            std::cout << "No recognizable partition table found.\n"
                      << "Device may be unformatted or use an unsupported format.\n";
            return;
        }
        
        std::cout << "Partition Table Type: " << table->typeName() << "\n"
                  << "Valid: " << (table->isValid() ? "Yes" : "No") << "\n";
        
        // Additional info for GPT
        if (table->type() == TableType::GPT) {
            auto gpt = dynamic_cast<GPTTable*>(table.get());
            if (gpt) {
                std::cout << "Disk GUID: " << gpt->getDiskGuid() << "\n"
                          << "UEFI System: " << (gpt->isUEFISystem() ? "Yes" : "No") << "\n"
                          << "Protective MBR: " << (gpt->hasProtectiveMBR() ? "Yes" : "No") << "\n";
            }
        }
        
        // Additional info for MBR
        if (table->type() == TableType::MBR) {
            auto mbr = dynamic_cast<MBRTable*>(table.get());
            if (mbr) {
                std::cout << "Disk Signature: 0x" << std::hex << mbr->getDiskSignature() << std::dec << "\n"
                          << "Extended Partition: " << (mbr->hasExtendedPartition() ? "Yes" : "No") << "\n";
            }
        }
        
        auto partitions = table->getPartitions();
        
        std::cout << "\n" << partitions.size() << " partition(s) found:\n\n";
        
        for (size_t i = 0; i < partitions.size(); i++) {
            printPartition(partitions[i], static_cast<int>(i + 1));
        }
        
        // Validation
        auto validation = table->validate();
        if (validation.failed()) {
            std::cout << "Warning: Validation issues found:\n"
                      << validation.message << "\n";
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
}

int main(int argc, char* argv[]) {
    // Load the message catalog from the environment, if configured:
    //   OPM_LOCALE=es  OPM_CATALOG=/path/to/es.po
    const char* locale_env = std::getenv("OPM_LOCALE");
    const char* catalog_env = std::getenv("OPM_CATALOG");
    if (locale_env && *locale_env && catalog_env && *catalog_env) {
        int loaded = i18n::loadCatalog(locale_env, catalog_env);
        if (loaded > 0) {
            i18n::setLocale(locale_env);
        }
    }

    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    std::string command = argv[1];
    
    // Collect remaining args
    std::vector<std::string> args;
    for (int i = 2; i < argc; i++) {
        args.push_back(argv[i]);
    }
    
    if (command == "list" || command == "ls") {
        cmdList();
    } else if (command == "info") {
        if (argc < 3) {
            std::cerr << "Error: info command requires a device path\n"
                      << "Usage: " << argv[0] << " info <device>\n"
                      << "Example: " << argv[0] << " info /dev/sda\n";
            return 1;
        }
        cmdInfo(argv[2]);
    } else if (command == "read" || command == "show") {
        if (argc < 3) {
            std::cerr << "Error: read command requires a device path\n"
                      << "Usage: " << argv[0] << " read <device>\n"
                      << "Example: " << argv[0] << " read /dev/sda\n";
            return 1;
        }
        cmdRead(argv[2]);
    } else if (command == "mklabel") {
        return cli::cmdMklabel(args);
    } else if (command == "create") {
        return cli::cmdCreate(args);
    } else if (command == "delete" || command == "rm") {
        return cli::cmdDelete(args);
    } else if (command == "resize") {
        return cli::cmdResize(args);
    } else if (command == "move" || command == "mv") {
        return cli::cmdMove(args);
    } else if (command == "format" || command == "mkfs") {
        return cli::cmdFormat(args);
    } else if (command == "check" || command == "fsck") {
        return cli::cmdCheck(args);
    } else if (command == "fsinfo") {
        return cli::cmdFSInfo(args);
    } else if (command == "lvm") {
        return cli::cmdLVM(args);
    } else if (command == "raid") {
        return cli::cmdRAID(args);
    } else if (command == "cryptinfo") {
        return cli::cmdCryptInfo(args);
    } else if (command == "align") {
        return cli::cmdAlign(args);
    } else if (command == "i18n") {
        return cli::cmdI18n(args);
    } else if (command == "convert") {
        return cli::cmdConvert(args);
    } else if (command == "set-active" || command == "active" || command == "bootable") {
        return cli::cmdSetActive(args);
    } else if (command == "hide") {
        return cli::cmdHideUnhide(args, true);
    } else if (command == "unhide") {
        return cli::cmdHideUnhide(args, false);
    } else if (command == "help" || command == "--help" || command == "-h") {
        printUsage(argv[0]);
    } else {
        std::cerr << "Unknown command: " << command << "\n\n";
        printUsage(argv[0]);
        return 1;
    }
    
    return 0;
}
