---
sidebar_position: 100
---

# Frequently Asked Questions

## General

### Is OPM really free?
**Yes.** Completely free under GPL-3.0. No paid editions, no feature restrictions.

### What platforms are supported?
Currently **Linux only**. Windows and macOS planned for Phase 7.

### Is there a GUI?
Not yet. CLI only. GUI planned for Phase 6.

### What filesystems are supported?
- FAT32: Complete ✅
- EXT4: Complete ✅
- NTFS: Complete ✅
- exFAT: In progress 🚧

## Usage

### Why does OPM need root access?
Partition management requires direct disk access and writing to partition tables.

### Is it safe to use?
OPM has a transaction system with automatic rollback, but always backup data first!

### How do I install?
```bash
git clone https://github.com/openpartitionmanager/opm.git
cd opm && mkdir build && cd build
cmake .. && make -j4 && sudo make install
```

## Contributing

### How can I contribute?
Check our GitHub repository for contribution guidelines.

### What skills are needed?
- C++
- CMake
- Git
- Linux

## Getting Help

- GitHub Issues
- Discord
- Documentation
