---
sidebar_position: 99
---

# Development Roadmap

## Phase Overview

### Phase 1: Foundation ✅
- MBR/GPT partition tables
- Device enumeration
- Basic CLI

### Phase 2: Basic Operations ✅
- Transaction system
- Create/delete/resize

### Phase 3: Filesystems ✅
- FAT32 support (~2,060 LOC)
- EXT4 support (~2,100 LOC)
- NTFS support (~1,020 LOC)
- exFAT support (~1,358 LOC) with full test coverage

### Phase 4: Advanced Operations ✅
- ✅ Disk cloning with verification
- ✅ Partition copying
- ✅ Secure erase (Zeros, Random, DoD, Gutmann, NIST)
- ✅ Benchmarking (sequential/random I/O, latency)

### Phase 5: Boot Environment ✅
- ✅ Live USB creation from ISO
- ✅ Boot repair tools
- ✅ Bootloader installation
- ✅ USB device detection

### Phase 6: GUI 🚧
- ✅ Qt interface framework (requires Qt5/6 installation)
- 📋 Dialogs: Create/Delete/Resize/Format/Clone/Secure Erase/Benchmark
- **Build**: `cmake .. -DBUILD_GUI=ON` (requires Qt5 or Qt6)

### Phase 7: Cross-Platform 📋
- Windows support
- macOS support

### Phase 8: Enterprise 📋
- RAID support
- LVM support

### Phase 9: Version 1.0 📋

## Current Status

**Version**: 0.1.0 Alpha  
**Completed**: ~95% of core functionality  
**Tests**: 36/36 passing

### Filesystem Support
| Filesystem | Format | Check | Resize | Status |
|-----------|--------|-------|--------|--------|
| FAT32 | ✅ | ✅ | ✅ | Complete |
| EXT4 | ✅ | ✅ | ✅ | Complete |
| NTFS | ✅ | ✅ | ⚠️ | Mostly Complete |
| exFAT | ✅ | ✅ | ✅ | Complete |

### Code Statistics
- Core library: ~50,000 LOC
- Tests: 36 test cases
- Total estimated: ~20,000 LOC at v1.0

## Timeline

**Target**: Version 1.0 by Q1 2025

## Contributing

See our GitHub repository for contribution guidelines.
