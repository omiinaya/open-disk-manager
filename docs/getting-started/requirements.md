---
sidebar_position: 1
---

# System Requirements

## Supported Operating Systems

- **Linux**: Kernel 3.10+ (Ubuntu 18.04+, CentOS 7+, Debian 9+)

## Hardware Requirements

### Minimum
- **Processor**: x86_64 (64-bit)
- **RAM**: 512 MB
- **Disk Space**: 50 MB
- **Privileges**: Root/sudo access

### Recommended
- **Processor**: Multi-core x86_64
- **RAM**: 2 GB+
- **Disk Space**: 100 MB

## Dependencies

```bash
# Ubuntu/Debian
sudo apt-get install build-essential cmake git libblkid-dev

# CentOS/RHEL/Fedora
sudo dnf install gcc-c++ cmake git libblkid-devel
```

## Supported Filesystems

- FAT32 - Full support
- EXT4 - Full support
- NTFS - Full support
- exFAT - In development
