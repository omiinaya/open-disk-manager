---
sidebar_position: 1
slug: /
---

# Open Partition Manager

**The Free, Open-Source Alternative to EaseUS Partition Master**

Welcome to Open Partition Manager (OPM), a comprehensive, open-source partition management tool designed to provide ALL features for free, forever. Licensed under GPL-3.0.

## What is OPM?

Open Partition Manager is a powerful partition management solution that gives you complete control over your disk partitions without the limitations of proprietary software. Unlike commercial alternatives that lock features behind paywalls, OPM provides every single feature completely free and open-source.

### Key Principles

- **100% Free**: No paid editions, no feature restrictions, no time limits
- **Open Source**: Full source code available under GPL-3.0 license
- **Cross-Platform**: Linux (current), Windows and macOS support planned
- **Safe**: Transaction support with rollback capability
- **Complete**: All filesystem implementations from scratch for maximum control

## Quick Start

### Installation

```bash
# Clone the repository
git clone https://github.com/openpartitionmanager/opm.git
cd opm

# Build the project
mkdir build && cd build
cmake ..
make -j4

# Install (optional)
sudo make install
```

### Basic Usage

```bash
# List all disks and partitions
opm list

# Get detailed info about a partition
opm info /dev/sda1

# Create a new partition
opm create /dev/sda --start-sector 2048 --size 50G --type fat32

# Format a partition
opm format /dev/sda1 --filesystem ntfs --label "MyData"

# Resize a partition
opm resize /dev/sda1 --size 100G
```

## Current Status

**Version**: 0.1.0 (Alpha)
**Status**: Active Development

### Completed Features ✅

- **Phase 1**: Foundation
  - MBR/GPT partition table reading
  - Device enumeration and detection
  - Disk I/O abstraction layer
  - CLI interface with basic commands

- **Phase 2**: Basic Operations
  - Transaction framework with rollback
  - Create/delete/resize partitions
  - Safe operation queue with validation

- **Phase 3**: Filesystem Support
  - **FAT32**: Complete
  - **ext4**: Complete
  - **NTFS**: Complete
  - **exFAT**: In Progress

### In Development 🚧

- Phase 3.5: exFAT implementation
- Phase 4: Advanced operations (clone, copy)
- Phase 5: Bootable environment
- Phase 6: GUI application

## Documentation Structure

- **Getting Started**: Installation and first steps
- **Features**: Detailed feature documentation
- **Development**: Contributing and architecture
- **Roadmap**: Future plans and timeline

## Getting Help

- **GitHub Issues**: https://github.com/openpartitionmanager/opm/issues
- **Discord**: https://discord.gg/opm

## Contributing

We welcome contributions! Check our GitHub repository for guidelines.

## License

Open Partition Manager is licensed under the GNU General Public License v3.0 (GPL-3.0).
