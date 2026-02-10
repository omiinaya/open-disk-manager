# Open Partition Manager

A comprehensive, open-source partition management tool for Windows, Linux, and macOS.

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Build Status](https://github.com/opencode/partition-manager/workflows/CI/badge.svg)](https://github.com/opencode/partition-manager/actions)

## Philosophy

**All features are free and open-source.** No paid editions, no restrictions, no artificial limitations.

Unlike commercial tools that lock features behind paywalls ($50-$800), this tool provides complete functionality to all users:
- ✅ **100% Free** - No cost, ever
- ✅ **Open Source** - GPL-3.0 licensed
- ✅ **Complete Feature Set** - Everything included
- ✅ **No Restrictions** - Use on unlimited machines
- ✅ **Cross-Platform** - Windows, Linux, macOS

## Features

### Core Partition Management
- Create, delete, resize, move, merge, split partitions
- Support for MBR and GPT partition tables
- File system support: NTFS, FAT32, exFAT, ext2/3/4, ReFS
- Format and check file systems

### Advanced Operations
- Clone disks and partitions
- Migrate OS to SSD without reinstallation
- Convert MBR ↔ GPT without data loss
- Dynamic disk and LVM support
- RAID support (0, 1, 5, 10)

### Data Protection
- Partition recovery
- Boot repair (MBR, GPT, GRUB, BCD)
- Password reset
- 4K alignment optimization
- Secure disk wiping

### Bootable Environment
- Create bootable USB/CD for recovery
- Standalone bootable ISO
- Portable version (run from USB)
- Hardware diagnostics

### Encryption Support
- BitLocker (read, write, resize)
- LUKS (Linux Unified Key Setup)
- VeraCrypt containers

## Quick Start

### Requirements
- C++17 compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.16+
- 100MB disk space

### Build

```bash
# Clone the repository
git clone https://github.com/opencode/partition-manager.git
cd partition-manager

# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
make -j$(nproc)

# Run tests
make test
```

### Usage

```bash
# List partitions
sudo ./opm list

# Create partition
sudo ./opm create /dev/sda --size 100G --type ntfs

# Resize partition
sudo ./opm resize /dev/sda1 --size +50G

# Clone disk
sudo ./opm clone /dev/sda /dev/sdb
```

## Documentation

- [User Guide](docs/user-guide.md)
- [API Documentation](docs/api.md)
- [Contributing](CONTRIBUTING.md)
- [Roadmap](docs/ROADMAP.md)

## Safety First

⚠️ **Warning**: Partition operations can result in data loss. Always:
- Backup important data before operations
- Verify operations in the preview before applying
- Use the bootable environment for system disk operations

## License

GPL-3.0 - See [LICENSE](LICENSE) for details.

## Contributing

Contributions are welcome! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## Acknowledgments

This project is inspired by the need for a truly free, comprehensive partition management solution.
