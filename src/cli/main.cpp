#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include "opm/partition_table.hpp"
#include "opm/disk_io.hpp"
#include "opm/utils.hpp"
#include "opm/types.hpp"

using namespace opm;

void printUsage(const char* program) {
    std::cout << "Open Partition Manager - CLI Tool\n"
              << "Usage: " << program << " <command> [options]\n\n"
              << "Commands:\n"
              << "  list                  List all disks and partitions\n"
              << "  info <device>         Show device information\n"
              << "  read <device>         Read and display partition table\n"
              << "\nExamples:\n"
              << "  " << program << " list\n"
              << "  " << program << " info /dev/sda\n"
              << "  " << program << " read /dev/sda\n";
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
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    std::string command = argv[1];
    
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
    } else if (command == "help" || command == "--help" || command == "-h") {
        printUsage(argv[0]);
    } else {
        std::cerr << "Unknown command: " << command << "\n\n";
        printUsage(argv[0]);
        return 1;
    }
    
    return 0;
}
