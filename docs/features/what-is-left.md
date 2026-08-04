---
sidebar_position: 2
---

# What's Left To Do

## Current Status

**Version**: 0.2.0
**Completed**: All roadmap phases implemented (August 2026)
**Core + CLI + GUI + Tests**: ~24,000 lines
**Tests**: 230 unit tests + CLI E2E, passing on Linux and Windows (MinGW)

---

## Completed ✅

### Phase 1-5: Core Functionality
- MBR/GPT partition tables (read + write + convert)
- Transaction system with rollback
- Device enumeration
- Safe operations with validation

### All Filesystems (COMPLETE)

| Filesystem | Format | Check | Resize | Label | Undelete | Status |
|-----------|--------|-------|--------|-------|----------|--------|
| **FAT32** | ✅ | ✅ | ✅ | ✅ | ✅ | Complete |
| **EXT4** | ✅ | ✅ | ✅ | ✅ | — | Complete |
| **NTFS** | ✅ | ✅ | ✅ | ✅ | — | Complete |
| **exFAT** | ✅ | ✅ | ✅ | ✅ | — | Complete |
| **swap** | ✅ | ✅ | — | ✅ | — | Complete |

### Advanced Operations ✅
- Disk cloning (sector + verify, resize-aware)
- Partition copying
- Secure erase (zero/random/DoD/Gutmann/NIST)
- Benchmarking
- LVM detection, software RAID detection
- BitLocker/LUKS detection + unlock (tool wrappers)

### Boot & Recovery ✅
- Live USB creation + verify
- Boot repair (MBR signature, GPT restore-from-backup)
- ISO mount/extract/create
- Partition recovery scan + MBR rebuild (`opm recover`)
- FAT32 undelete (`opm undelete`)
- UEFI boot entries, Windows SAM reset, Linux shadow reset

### GUI ✅
- Qt6 interface, real device enumeration
- All 7 operation dialogs (real core execution)
- Visual partition map, operation log, dark/light theme, progress bar
- 4 wizards (clone, migrate OS, bootable media, recovery)

### Release ✅
- CI matrix: ubuntu + macos + windows (MSYS2/MinGW)
- CPack DEB + RPM packaging (verified), man page, Dockerfile
- i18n catalogs: es, fr, de, zh, ja

---

## Remaining (external / long-tail) 📋

These items require external resources or real-world hardware and are
documented as future work rather than claimed:

- **Windows GUI packaging** (Qt app bundles for Windows/macOS installers)
- **Hardware-in-the-loop testing** on many real devices (HDD/SSD/NVMe/USB)
- **Compatibility testing** across BIOS/UEFI vendor firmware
- **Community building** (forum/Discord, issue workflows, contribution metrics)
- **Video tutorials**
- **AUR / macOS package formats** (deb+rpm done)

---

## Total Estimated Code

**Current**: ~24,000 lines (core 55+ files, CLI, GUI, tests)

---

*Last Updated: August 2026*
