#include <iostream>
#include <vector>
#include "opm/disk_io.hpp"
#include "opm/partition_table.hpp"
#include "opm/utils.hpp"

using namespace opm;

int main() {
    std::cout << "Open Partition Manager - Disk Detection Test\n";
    std::cout << "=============================================\n\n";
    
    std::cout << "Enumerating storage devices...\n\n";
    
    auto devices = DeviceEnumerator::enumerateDevices();
    
    if (devices.empty()) {
        std::cout << "No storage devices found.\n";
        std::cout << "Note: This test requires root privileges.\n";
        std::cout << "Try running with sudo.\n";
        return 1;
    }
    
    std::cout << "Found " << devices.size() << " device(s):\n\n";
    
    for (size_t i = 0; i < devices.size(); i++) {
        const auto& dev = devices[i];
        
        std::cout << "Device " << (i + 1) << ":\n"
                  << "  Path:        " << dev.path << "\n"
                  << "  Model:       " << (dev.model.empty() ? "Unknown" : dev.model) << "\n"
                  << "  Serial:      " << (dev.serial.empty() ? "Unknown" : dev.serial) << "\n"
                  << "  Size:        " << utils::formatBytes(dev.size) << "\n"
                  << "  Sector Size: " << dev.geometry.bytes_per_sector << " bytes\n"
                  << "  Total Sectors: " << dev.geometry.total_sectors << "\n"
                  << "  SSD:         " << (dev.ssd ? "Yes" : "No/Unknown") << "\n"
                  << "  Removable:   " << (dev.removable ? "Yes" : "No") << "\n"
                  << "  Read-Only:   " << (dev.readonly ? "Yes" : "No") << "\n";
        
        // Try to read partition table
        try {
            auto disk = DiskIO::openReadOnly(dev.path);
            if (disk) {
                auto table = PartitionTable::load(disk);
                if (table) {
                    std::cout << "  Table Type:  " << table->typeName() << "\n"
                              << "  Valid:       " << (table->isValid() ? "Yes" : "No") << "\n"
                              << "  Partitions:  " << table->getPartitionCount() << "\n";
                } else {
                    std::cout << "  Table Type:  None or unknown\n";
                }
            }
        } catch (const std::exception& e) {
            std::cout << "  Error:       " << e.what() << "\n";
        }
        
        std::cout << "\n";
    }
    
    std::cout << "Test completed successfully.\n";
    return 0;
}
