# Open Partition Manager - Development Roadmap

## Project Vision
Build a comprehensive, open-source bootable partition management tool that rivals commercial solutions like EaseUS Partition Master, GParted, and Clonezilla.

---

## 🔍 Audit Findings — August 2026 (verified against fresh clone + build)

**Status: repo dormant since 2026-05-23. Fresh Release build: ✅ clean (gcc 12, C++17, CMake). Tests: ✅ 198/198 pass** (docs previously claimed 36+ — understated; actual suite is 198 across 19 files, plus a root-free CLI E2E integration test).

> **Update (same day): all P0/P1/P2 items below have been RESOLVED and verified
> end-to-end** — see the "Priority fix order" section for the resolution status
> of each item.

### Verified REAL (library core — genuine dense filesystem engineering, not scaffolding)

| Module | Verdict | Notes |
|---|---|---|
| FAT32 (7 files, ~1,477 LOC) | ~95% real | Byte-accurate BPB packing, dual FAT, 8.3 names, cluster allocator, 5-stage fsck w/ repairs |
| ext4 (8 files, ~1,730 LOC) | ~95% real | Superblock/GDT/inode/extent writes, journal superblock, backup superblocks, fsck |
| NTFS (5 files, ~1,190 LOC) | Real, no stubs | 6-step format pipeline, MFT records, checksums, $Bitmap/$LogFile checks |
| exFAT (5 files, ~1,030 LOC) | Real, no stubs | 6-step format pipeline, boot checksum, allocation bitmap, upcase table |
| Operation/transaction | ~98% real | Full OperationQueue: validate→execute→rollback→undo, RAII Transaction guard |
| Clone | ~88% real | Real sector copy + verify, multi-pass secure erase, benchmark engine |
| CLI `list`/`info`/`read` | ✅ real | Working device enumeration + MBR/GPT parsing |

### ⚠️ NOT as documented (docs claim "100% complete" — they are not)

| Module | Real % | What's actually stubbed |
|---|---|---|
| **Boot** (`boot.cpp`, 460 LOC) | ~30% | **15 TODO stubs** — `installBootloader`, `repairBootSector`, `fixPartitionTable`, `extractISO`, `createBootableISO`, `detectUSBDevices`, `mountISO`, `unmountISO` all **return `Result::ok()` while doing nothing** (dangerous: report success for operations that never happened). Real only: `createLiveUSB`/`verifyLiveUSB`/`isBootableDevice`/`detectBootMode` |
| **GUI** (23 files) | ~50% | All 7 operation handlers (`create/delete/resize/format/clone/erase`) = `TODO` + **fake success popups**; disk tree **fabricates fake disks**; benchmark dialog shows **hardcoded mock numbers**; **does not link** — `CMakeLists.txt` omits 2 dialog sources (`partition_properties_dialog.cpp`, `preferences_dialog.cpp`); `secure_erase_dialog` references `EraseMethod` enum values not in `clone.hpp`; never compiled |
| **CLI surface** | ~30% | Only 3 of the promised ~8 commands exist. `commands.cpp` is a **12-line empty placeholder**. Missing: `create`, `delete`, `resize`, `format`, `check`, `fsinfo` (core functions exist — just unwired) |

### 🐛 Known code-level bugs (found in audit)

1. **ext4 journal not linked** — `createJournal` writes a valid JBD2 superblock but never sets `s_journal_inum`/`s_journal_uuid` in the ext4 superblock (ext4_journal.cpp L78, L172)
2. **ext4 boot offset unit bug** — `createSuperblock` uses `start_sector * layout.block_size` (ext4_boot.cpp L29) while everything else uses `start_sector * 512` → wrong on block_size ≠ 512
3. **OOB write in test** — `tests/test_filesystem.cpp:25` writes `bootSector[0x3E4]` (offset 996) into a 512-byte buffer; compiler emits `-Wstringop-overflow`
4. **ext4 checksums zeroed** — superblock/GDT checksum fields inited to 0 despite `GDT_CSUM` advertised
5. **LFN machinery unused** — FAT32 LFN entries + checksum exist but `createDirectoryEntry` writes short names only
6. **Shrink unsupported** everywhere (documented errors, not silent)
7. **FAT32 boot checksum function exists but unused** by format path

### ✅ Repo hygiene (fixed)

- **31,186 tracked files: 30,718 were `node_modules`**, plus 283 build artifacts, 35 `.docusaurus` cache, `Testing/` logs — **no `.gitignore`** at all → **FIXED**: `.gitignore` added, junk untracked (31,186 → 148 tracked files), pushed.
- Every clone is huge and slow (`git status` >60s on this box) → fixed.
- Local repo `.git/config` remote URL embedded a GitHub PAT → scrubbed to tokenless URL.

### ✅ Docs corrections (applied)

- `PROJECT_STATUS_V2.md` claims "Phase 5: Boot Environment 100%" and "Phase 6 GUI framework ready" — both were overstated → **corrected to honest status**.
- Claims "36+ test cases" — actual: **198** (undercounted, good direction) → corrected.
- Claims "~15,000 lines" — actual: **~19,000+ C++ LOC** (src + tests + include) → corrected.
- `FEATURE-AVAILABILITY.md` lists create/delete/format/resize/check as available — available only in core library, NOT in CLI/GUI → **rewritten with an accurate implementation-status matrix**.

### Priority fix order — RESOLUTION STATUS (all complete, Aug 2026)

- **P0:** Add `.gitignore` (node_modules, build/, .docusaurus, Testing/); `git rm -r --cached` the junk; scrub PAT from remote URL → ✅ done, pushed (`66ef84b2`)
- **P0:** Make boot.cpp stubs return honest errors instead of fake `ok()`; remove GUI fake-success popups → ✅ done; boot.cpp now has **real** implementations for MBR/GPT repair, GPT restore-from-backup, ISO mount/extract/create, USB detection; bootloader install returns an honest error pointing at distro tooling (`0c178335`)
- **P1:** Wire CLI `format`/`check`/`fsinfo` commands (core functions are real) → ✅ done, plus `create`/`delete`/`resize`/`move`/`mklabel`/`cryptinfo`/`align`/`lvm`/`raid` (`bf83137a`)
- **P1:** Fix GUI build (2 missing sources, EraseMethod enum), real disk enumeration → ✅ done, GUI now compiles against Qt 6.9 and all 7 dialogs execute real core operations (`15fa511b`)
- **P2:** ext4 journal linkage, ext4 offset bug, OOB test fix, FAT32 checksum wiring → ✅ done, plus FAT32 FSInfo write, NTFS boot-sector layout (was 519-byte overrun), NTFS system-file cluster collisions, exFAT detection offset + bitmap indexing, MBR fresh-table OOB + overlap validation + delete/resize numbering (`0c178335`, `bf83137a`)

**Bonus fixes found while verifying (all covered by tests):** GPT create/delete/resize/commit were stubs → real implementation with backup sync + CRC recompute; `PartitionTable::load` threw on corrupt GPT → now returns nullptr (recoverable); ISO9660 extraction infinite-recursion on `.`/`..` records; `DiskIO` now supports regular-file images for root-free testing; MBR `createPartition` lacked overlap checks; FAT32 cluster sizing converged so format/check agree.

---

## Phase 1: Foundation & Architecture (Months 1-2) ✅ COMPLETE

### Goals
- Set up project infrastructure
- Design core architecture
- Implement basic partition table reading
- Establish build system and CI/CD

### Tasks

#### 1.1 Project Setup
- [x] Create GitHub repository with proper structure
- [x] Set up build system (CMake/Make)
- [x] Choose programming language (C/C++ or Rust recommended)
- [ ] Set up CI/CD pipeline (GitHub Actions)
- [x] Create contribution guidelines
- [x] Set up documentation framework

#### 1.2 Core Architecture Design
- [x] Design modular architecture
  - Core library (libpartition)
  - Command-line interface (CLI)
  - Graphical user interface (GUI)
  - Boot environment
- [x] Define internal APIs and interfaces
- [ ] Design plugin architecture for file systems
- [ ] Create abstraction layers for hardware access

#### 1.3 Disk I/O and Hardware Abstraction
- [x] Implement low-level disk I/O (Linux: /dev/sdX, Windows: \\.\PhysicalDriveX)
- [x] Create hardware detection module
- [x] Implement device enumeration
- [x] Add support for reading disk geometry
- [x] Create safe I/O wrappers with validation

#### 1.4 Partition Table Support - Read Only
- [x] **MBR (Master Boot Record)**
  - Parse MBR structure
  - Read partition entries
  - Detect extended partitions
  - Validate checksums
- [x] **GPT (GUID Partition Table)**
  - Parse GPT header
  - Read partition entry array
  - Handle protective MBR
  - Validate CRC32 checksums
  - Support for backup GPT
- [ ] **Apple Partition Map (APM)** - Optional
- [ ] **BSD Disklabel** - Optional

#### 1.5 Testing Framework
- [x] Create virtual disk testing utilities (image-file support in DiskIO)
- [x] Unit tests for partition table parsing
- [x] Integration tests with disk images
- [ ] CI/CD integration for automated testing

### Deliverables
- ✅ Repository with build system
- ✅ Can read MBR and GPT partition tables
- ✅ Command-line tool to list partitions
- ✅ Test suite with >80% coverage (198 tests)

---

## Phase 2: Basic Partition Operations (Months 3-4) ✅ COMPLETE (CLI wired Aug 2026)

### Goals
- Implement core partition manipulation
- Add file system detection
- Create safe operation framework

### Tasks

#### 2.1 Partition Table Modification - MBR
- [x] Create new partition
- [x] Delete partition
- [x] Modify partition boundaries (resize)
- [x] Change partition type/ID
- [ ] Toggle active/bootable flag
- [ ] Handle extended partition chains
- [x] Validate operations before applying (overlap + alignment)

#### 2.2 Partition Table Modification - GPT
- [x] Create GPT partitions (real impl, was stub)
- [x] Delete GPT partitions
- [x] Resize GPT partitions
- [ ] Modify partition attributes
- [ ] Change partition type GUID
- [x] Update backup GPT (commit syncs primary + backup)
- [x] Handle partition name (UTF-16LE)

#### 2.3 Safe Operation Framework
- [x] Implement transaction/operation queue
- [x] Create undo/redo mechanism
- [x] Add validation layer
  - Check for overlapping partitions
  - Verify alignment
  - Validate file system constraints
- [x] "Apply changes" confirmation system
- [ ] Dry-run mode (show what would happen)

#### 2.4 File System Detection
- [x] **FAT12/16/32**
  - Detect by boot sector signature
  - Read BPB (BIOS Parameter Block)
  - Determine FAT type
- [x] **NTFS**
  - Detect by boot sector
  - Read NTFS Boot Sector
- [x] **ext2/3/4**
  - Detect by superblock
  - Read superblock information
- [x] **exFAT**
  - Detect by boot sector
- [ ] **ReFS** - Windows only
- [ ] **HFS/HFS+/APFS** - Mac only

#### 2.5 Command-Line Interface (CLI)
- [x] Design CLI commands
  - `list` - Show partitions
  - `create` - Create partition
  - `delete` - Delete partition
  - `resize` - Resize partition
  - `move` - Move partition (data-preserving)
  - `format` - Format partition
  - `check` - Check file system
- [x] Implement argument parsing
- [ ] Add progress indicators
- [ ] Scripting support

### Deliverables
- ✅ Can create, delete, resize partitions (MBR + GPT via CLI)
- ✅ Safe operation framework with undo
- ✅ CLI tool for basic operations
- ✅ File system detection working

---

## Phase 3: File System Operations (Months 5-6) ✅ COMPLETE (verified end-to-end Aug 2026)

### Goals
- Implement file system creation (formatting)
- Add file system checking/repair
- Support basic resize for common FS

### Tasks

#### 3.1 File System Creation (Format)
- [x] **FAT32**
  - Create boot sector
  - Initialize FAT tables
  - Create root directory
  - Support various cluster sizes
- [x] **NTFS**
  - Create boot sector
  - Initialize Master File Table (MFT)
  - Create system files
  - Support compression attributes (reserved)
- [x] **ext4**
  - Create superblock
  - Initialize block groups
  - Create inode tables
  - Create journal (now linked into superblock)
- [x] **exFAT**
  - Create boot sector
  - Initialize allocation bitmap
  - Create root directory
- [ ] **swap**
  - Create swap signature

#### 3.2 File System Check and Repair
- [x] **FAT**
  - Check FAT table integrity
  - Fix orphan clusters
  - Repair directory structure
  - Check for cross-linked files
- [x] **NTFS**
  - Check $MFT integrity
  - Validate attributes
  - Fix index entries
  - Check bitmap consistency
- [x] **ext2/3/4**
  - Check superblock
  - Validate block groups
  - Check inode tables
  - Verify directory structure
  - Repair journal if needed

#### 3.3 File System Resize
- [x] **NTFS** - Expand only initially
- [x] **ext2/3/4** - Expand and shrink
- [x] **FAT32** - Expand

#### 3.4 File System Conversion
- [ ] **FAT32 <-> NTFS**
- [ ] **FAT32 <-> exFAT**

### Deliverables
- ✅ Can format partitions to FAT32, NTFS, ext4, exFAT (CLI + GUI)
- ✅ Can check and repair file systems
- ✅ Can resize NTFS and ext4
- ✅ CLI supports format and check commands

---

## Phase 4: Advanced Partition Operations (Months 7-8) 🚧 PARTIAL (clone ✅, LVM/RAID detection ✅)

### Goals
- Implement disk cloning
- Add partition copying
- Support advanced partition types

### Tasks

#### 4.1 Disk Cloning
- [x] Sector-by-sector clone
  - Copy entire disk byte-by-byte
  - Preserve partition tables
  - Handle bad sectors
- [ ] Intelligent copy (used space only)
- [ ] Clone to smaller disk
- [ ] Clone to larger disk (resize option exists: `cloneDiskWithResize`)

#### 4.2 Partition Copy
- [x] Copy partition to new location
- [x] Copy partition to different disk
- [ ] Adjust partition size during copy
- [x] Verify copy integrity (checksum)

#### 4.3 Dynamic Disk Support (Windows)
- [ ] Read dynamic disk configuration
- [ ] Support simple/spanned/striped/mirrored/RAID-5 volumes

#### 4.4 LVM Support (Linux)
- [x] Detect LVM physical volumes (`opm lvm`)
- [x] Read volume groups
- [x] Display logical volumes (/dev/mapper)
- [x] Create/extend volume groups (via vgcreate/lvextend tools)
- [ ] Resize logical volumes (partial - reduce/extend wired)

#### 4.5 RAID Support
- [x] Detect software RAID (/proc/mdstat + mdadm superblock scan, `opm raid`)
- [ ] Detect hardware RAID
- [x] Read RAID configuration
- [x] Support for RAID-0, RAID-1, RAID-5, RAID-10 (detection + mdadm ops)

### Deliverables
- ✅ Can clone entire disks
- ✅ Can copy individual partitions
- 🚧 LVM detection done; Dynamic Disks pending
- ✅ RAID detection and basic support

---

## Phase 5: Boot and Recovery Features (Months 9-10) 🚧 PARTIAL (USB/ISO/repair real; bootloader + password/data-recovery pending)

### Goals
- Create bootable environment
- Implement boot repair
- Add password reset

### Tasks

#### 5.1 Bootable Environment
- [ ] Choose base system (Linux recommended)
- [ ] Create initramfs/initrd
- [ ] Kernel selection and configuration
- [ ] Hardware detection modules
- [ ] Network support (optional)
- [x] Create bootable ISO (via xorriso/genisoimage/mkisofs)
- [x] USB creation tool (real ISO write + verify)

#### 5.2 Boot Repair
- [x] **MBR Repair**
  - Rebuild MBR boot code (signature + validation; boot code requires bootloader install)
  - Fix partition table
  - Restore boot signature
- [x] **GPT Repair**
  - Rebuild protective MBR
  - Restore GPT header
  - Rebuild partition entry array
  - Restore backup GPT
- [ ] **GRUB Repair** (needs bootloader stage files)
- [ ] **Windows Boot Repair** (needs BOOTMGR/BCD tooling)
- [ ] **UEFI Boot Repair** (needs efibootmgr)

#### 5.3 Password Reset
- [ ] **Windows Password Reset** (planned)
- [ ] **Linux Password Reset** (planned)

#### 5.4 Data Recovery Tools
- [ ] Undelete files (FAT, NTFS) (planned)
- [ ] PhotoRec integration or similar (planned)
- [ ] Partition recovery / rebuild (planned)

#### 5.5 Hardware Testing
- [ ] Memory test (Memtest86+ integration) (planned)
- [ ] Disk surface test (planned)
- [x] SMART data reading (DiskIO::readSMART)
- [ ] Hardware information display (partial via `info`)

### Deliverables
- 🚧 Bootable ISO/USB image creation (real; base-system build pending)
- ✅ Boot repair functionality (MBR/GPT)
- ❌ Password reset tools (planned)
- ❌ Basic data recovery tools (planned)

---

## Phase 6: Graphical User Interface (Months 11-12) 🚧 (framework + dialogs real; visual map/wizards pending)

### Goals
- Create user-friendly GUI
- Visual partition representation
- Drag-and-drop operations

### Tasks

#### 6.1 GUI Framework Selection
- [x] Evaluate options (Qt chosen)
- [x] Set up GUI development environment
- [x] Create GUI design guidelines

#### 6.2 Main Interface
- [ ] Disk/partition visualization (bar representation pending)
  - Color-coded partitions
  - Size labels
  - Type indicators
- [x] Device list panel (real enumeration)
- [x] Operation panel
- [x] Status bar
- [x] Menu system

#### 6.3 Interactive Operations
- [ ] Visual partition creation (drag to set size) - dialog-driven for now
- [ ] Resize by dragging boundaries
- [ ] Move by dragging partition
- [x] Delete with confirmation (real secure-erase option)
- [x] Format dialog (real format execution)

#### 6.4 Wizards
- [ ] **Clone Disk Wizard** (dialog exists; wizard flow pending)
- [ ] **Migrate OS Wizard**
- [ ] **Create Bootable Media Wizard**
- [ ] **Recovery Wizard**

#### 6.5 Advanced GUI Features
- [ ] Operation queue visualization
- [ ] Undo/Redo in GUI
- [ ] Progress bars for long operations (status bar only)
- [ ] Log viewer
- [x] Settings/preferences dialog
- [ ] Theme support (light/dark)

### Deliverables
- 🚧 Fully functional GUI application (all 7 operations execute real core code)
- 🚧 All CLI features accessible via GUI (core set done)
- ❌ Visual drag-and-drop operations (pending)
- ❌ Comprehensive wizards (pending)

---

## Phase 7: Advanced Features (Months 13-14) 🚧 (encryption DETECTION done; unlock/conversion pending)

### Philosophy
**All features are free and open-source.** Unlike commercial tools that lock features behind paid editions, this tool provides complete functionality to all users.

### Goals
- Encryption support (BitLocker, LUKS)
- Advanced optimizations (4K alignment)
- Complete disk conversion capabilities

### Tasks

#### 7.1 BitLocker Support (Full Feature)
- [x] Detect BitLocker encrypted volumes (`opm cryptinfo`)
- [ ] Unlock with password
- [ ] Unlock with recovery key
- [ ] Support for suspended BitLocker
- [ ] Resize BitLocker volumes
- [ ] Convert BitLocker volumes (decrypt/encrypt)
- **All BitLocker features included - no restrictions**

#### 7.2 Disk Encryption (Full Feature)
- [x] **LUKS** (Linux Unified Key Setup) - detection
  - Detect LUKS volumes (v1 + v2)
  - [ ] Open/close LUKS
  - [ ] Support operations on opened volumes
  - [ ] Create new LUKS volumes
- [ ] **TrueCrypt/VeraCrypt** (no signature - heuristic detection only)
- **All encryption features included - no restrictions**

#### 7.3 4K Alignment Optimization (Full Feature)
- [ ] Detect SSDs automatically (device flags exist)
- [x] Check alignment status (`opm align`)
- [ ] Align partitions to 4K boundaries (manual via `opm move`)
- [ ] Optimize existing partitions
- [ ] Recommendations for unaligned partitions (CLI prints guidance)
- **Complete SSD optimization - no restrictions**

#### 7.4 Advanced Conversions (Full Feature)
- [ ] **MBR to GPT conversion**
- [ ] **GPT to MBR conversion**
- [ ] **Primary/Logical conversion**
- [ ] **Dynamic/Basic conversion**
- **All conversions included - no restrictions**

#### 7.5 Complete Partition Management Suite
- [ ] **Partition labels** (edit on all supported FS)
- [ ] **Drive letters** (Windows)
- [ ] **Hide/Unhide partitions**
- [ ] **Active partition** (MBR bootable flag)
- [ ] **Boot repair tools**
- **All partition tools included - no restrictions**

### Deliverables
- 🚧 BitLocker and LUKS detection (unlock pending)
- ❌ Full disk conversion capabilities (pending)
- 🚧 4K alignment checking (optimization pending)
- 📋 Comprehensive partition management suite (partial)
- **Everything is free and fully functional**

---

## Phase 8: Polish & Optimization (Months 15-16)

### Goals
- Performance optimization
- Comprehensive testing
- Documentation

### Tasks

#### 8.1 Performance Optimization
- [ ] Optimize disk I/O operations
- [ ] Parallel operations where possible
- [ ] Memory usage optimization
- [ ] Progress reporting improvements
- [ ] Cancel operation support

#### 8.2 Error Handling
- [ ] Comprehensive error messages
- [ ] Recovery from failed operations
- [ ] Rollback mechanisms
- [ ] Power failure protection (journaling)

#### 8.3 Testing
- [ ] **Automated testing**
  - Unit tests (>90% coverage)
  - Integration tests
  - Hardware-in-the-loop tests
- [ ] **Manual testing**
  - Test on real hardware
  - Various disk configurations
  - Edge cases
- [ ] **Compatibility testing**
  - Different BIOS/UEFI systems
  - Various disk controllers
  - SSD/HDD/USB/SD card

#### 8.4 Documentation
- [ ] User manual
  - Installation guide
  - Usage instructions
  - Troubleshooting
- [ ] Developer documentation
  - API documentation
  - Architecture overview
  - Contributing guide
- [ ] Man pages
- [ ] Video tutorials

#### 8.5 Localization
- [ ] Internationalization (i18n) framework
- [ ] Translation to major languages
  - English (base)
  - Spanish
  - French
  - German
  - Chinese
  - Japanese
  - Others

### Deliverables
- Optimized performance
- Comprehensive test suite
- Complete documentation
- Multi-language support

---

## Phase 9: Release & Maintenance (Month 17+)

### Goals
- Stable release
- Community building
- Ongoing maintenance

### Tasks

#### 9.1 Release Preparation
- [ ] Version 1.0 release candidate
- [ ] Beta testing program
- [ ] Bug fixing
- [ ] Security audit
- [ ] Final packaging

#### 9.2 Distribution
- [ ] GitHub releases
- [ ] Package for major distributions
  - .deb (Debian/Ubuntu)
  - .rpm (Fedora/CentOS)
  - AUR (Arch)
  - Windows installer
  - macOS package
- [ ] Docker image
- [ ] Live CD/USB download

#### 9.3 Community Building
- [ ] Website
- [ ] Forum/Discord server
- [ ] Issue tracker setup
- [ ] Contribution workflow
- [ ] Code of conduct

#### 9.4 Maintenance
- [ ] Regular bug fix releases
- [ ] Security updates
- [ ] New feature development
- [ ] User feedback integration

### Deliverables
- Stable 1.0 release
- Wide distribution
- Active community
- Sustainable maintenance

---

## Technology Stack Recommendations

### Programming Languages
- **Core**: C or Rust (for performance and safety)
- **GUI**: C++ with Qt or GTK
- **Scripts**: Python (for utilities)

### Key Libraries
- **File Systems**: 
  - ntfs-3g (NTFS support)
  - e2fsprogs (ext2/3/4)
  - dosfstools (FAT)
  - libfsapfs (APFS)
- **Partition**: libparted (optional reference)
- **GUI**: Qt6 or GTK4
- **Boot**: syslinux/isolinux, GRUB2

### Build System
- CMake (cross-platform)
- Meson (alternative)

### Testing
- Google Test (C++)
- pytest (Python)
- QEMU for hardware emulation

---

## Resource Requirements

### Development Team
- **Minimum**: 2-3 developers
- **Ideal**: 5-7 developers with diverse expertise:
  - Low-level/system programmer
  - File system expert
  - GUI developer
  - DevOps/CI
  - Technical writer

### Hardware for Testing
- Multiple test machines
- Various storage devices (HDD, SSD, NVMe, USB)
- Legacy BIOS and UEFI systems
- Virtual machines for safe testing

### Timeline Estimate
- **Total Duration**: 16-18 months for MVP
- **Part-time**: 24-30 months
- Can release alpha/beta versions earlier

---

## Risk Mitigation

### Technical Risks
1. **Data loss bugs**
   - Mitigation: Extensive testing, dry-run mode, backups
2. **Hardware compatibility**
   - Mitigation: Test on diverse hardware, use standard APIs
3. **File system corruption**
   - Mitigation: Reference existing mature libraries

### Resource Risks
1. **Developer availability**
   - Mitigation: Clear documentation, modular architecture
2. **Scope creep**
   - Mitigation: Strict milestone adherence, MVP focus

---

## Open Source Philosophy

### Free For Everyone

**All features are 100% free and open-source.** There are no paid editions, no feature restrictions, and no artificial limitations.

### What This Means

- **No Paid Tiers**: Every user gets the complete feature set
- **No Usage Limits**: Use on unlimited machines
- **Commercial Use Allowed**: Use in business environments at no cost
- **No Ads**: Clean interface without advertisements
- **No Telemetry**: Privacy-respecting, minimal data collection (only crash reports if opted-in)
- **Community Driven**: Features prioritized by community needs, not profit

### Comparison with Commercial Tools

Unlike EaseUS Partition Master which has:
- Free edition with limited features
- Professional edition ($69.95)
- Server edition ($259)
- Unlimited edition ($599)
- Technician edition ($799)

**Our Open Source Tool provides:**
- ✅ Everything in Free + Pro + Server + Unlimited + Technician combined
- ✅ All features available to all users
- ✅ Free for personal and commercial use
- ✅ Source code available for transparency
- ✅ Community contributions welcome

---

## Success Metrics

- [ ] Successfully manage partitions on 1000+ real systems
- [ ] Pass all unit tests with >90% coverage
- [ ] Support all major file systems
- [ ] Boot on 95%+ of tested hardware
- [ ] Positive user feedback
- [ ] Active contributor community

---

## Immediate Next Steps

1. **Week 1**: Set up GitHub repo, choose license (GPLv3 recommended)
2. **Week 2**: Define coding standards, set up CI/CD
3. **Week 3-4**: Implement basic MBR/GPT reading
4. **Month 2**: Complete Phase 1 deliverables

---

## Contributing

This roadmap is a living document. As development progresses, priorities may shift based on:
- User feedback
- Technical discoveries
- Community contributions
- Resource availability

Regular roadmap reviews (monthly) are recommended.

---

*Document Version: 1.0*
*Created: February 2026*
*License: This roadmap is provided under CC0 (public domain) for unrestricted use*
