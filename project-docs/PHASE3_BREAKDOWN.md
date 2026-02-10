# Phase 3: File System Operations - Detailed Breakdown

Phase 3 from the main roadmap is too large to complete in one step. Here's a breakdown into sub-phases:

---

## Phase 3.1: File System Detection & Structure (1-2 weeks)

**Goal**: Detect file systems reliably and read basic information

### Tasks:
- [ ] Improve file system detection in DiskIO::detectFilesystem()
- [ ] Create filesystem base classes
- [ ] Implement FSInfo structure for reading filesystem metadata
- [ ] Add "format" command to CLI
- [ ] Add "info" command for filesystem details
- [ ] Add "check" command (dry-run)

### Deliverables:
- Can detect all supported file systems (FAT32, NTFS, ext4, exFAT)
- Can read filesystem info (label, size, free space)
- CLI: `opm format <device> <partition> --type <fs> --label <name>`
- CLI: `opm fsinfo <device> <partition>`

---

## Phase 3.2: FAT32 Implementation (2-3 weeks)

**Goal**: Full FAT32 support (create, check, resize)

### Tasks:
- [ ] Create FAT32 boot sector (BPB)
- [ ] Initialize FAT tables
- [ ] Create root directory
- [ ] Implement FAT32 check (verify FAT consistency)
- [ ] Implement FAT32 resize (expand)
- [ ] Handle cluster sizes
- [ ] Support FAT12/16 as well

### Deliverables:
- Can format partitions to FAT32
- Can check FAT32 integrity
- Can resize FAT32 (expand)
- CLI: `opm format /dev/sda1 --type fat32 --label "MyDrive"`
- CLI: `opm check /dev/sda1`
- CLI: `opm resize /dev/sda1 --size 10G`

---

## Phase 3.3: ext4 Implementation (2-3 weeks)

**Goal**: Full ext2/3/4 support (create, check, resize)

### Tasks:
- [ ] Create ext4 superblock
- [ ] Initialize block groups
- [ ] Create inode tables
- [ ] Create journal (ext3/4)
- [ ] Implement e2fsck-like checking
- [ ] Implement resize2fs functionality
- [ ] Handle ext2/3 compatibility

### Deliverables:
- Can format partitions to ext4
- Can check ext4 filesystems
- Can resize ext4 (shrink and expand)
- CLI: `opm format /dev/sda1 --type ext4 --label "rootfs"`
- CLI: `opm check /dev/sda1`
- CLI: `opm resize /dev/sda1 --size 50G`

---

## Phase 3.4: NTFS Implementation (3-4 weeks)

**Goal**: NTFS support (create, check, resize) - NOT SKIPPED!

### Tasks:
- [ ] Create NTFS boot sector
- [ ] Initialize Master File Table (MFT)
- [ ] Create $Bitmap file
- [ ] Create $LogFile (journal)
- [ ] Create system files ($Volume, $AttrDef, etc.)
- [ ] Implement NTFS consistency check
- [ ] Implement NTFS expand (grow NTFS)
- [ ] Handle NTFS attributes
- [ ] Support NTFS compression attributes

### Deliverables:
- Can format partitions to NTFS
- Can check NTFS integrity (chkdsk-like)
- Can resize NTFS (expand)
- CLI: `opm format /dev/sda1 --type ntfs --label "Windows"`
- CLI: `opm check /dev/sda1`
- CLI: `opm resize /dev/sda1 --size 100G`

---

## Phase 3.5: exFAT & Advanced Features (1-2 weeks)

**Goal**: exFAT support and advanced filesystem features

### Tasks:
- [ ] Implement exFAT boot sector
- [ ] Create allocation bitmap
- [ ] Create root directory
- [ ] exFAT integrity check
- [ ] File system conversion utilities (FAT32<->exFAT)
- [ ] Label editing for all FS types

### Deliverables:
- exFAT format support
- exFAT check support
- Label editing: `opm label <device> <partition> --set "NewLabel"`

---

## Implementation Order

### Week 1-2: Phase 3.1
- Detection improvements
- Basic structure
- CLI commands

### Week 3-4: Phase 3.2
- FAT32 implementation
- Tests for FAT32

### Week 5-6: Phase 3.3
- ext4 implementation
- Tests for ext4

### Week 7-9: Phase 3.4
- NTFS implementation (FULLY IMPLEMENTED, NOT SKIPPED)
- Tests for NTFS
- This is the most complex part

### Week 10: Phase 3.5
- exFAT
- Advanced features
- Integration testing

---

## Technical Notes

### FAT32 Complexity: MEDIUM
- Well documented
- Simple structures
- Extensive existing implementations (dosfstools)

### ext4 Complexity: MEDIUM-HIGH
- Complex structures (superblock, block groups, inodes)
- Journal handling
- ext2/3 compatibility
- Reference: e2fsprogs

### NTFS Complexity: HIGH
- Most complex of the three
- MFT (Master File Table) structure
- Attribute handling
- $Bitmap, $LogFile system files
- Reference: ntfs-3g, NTFS specification
- **BUT WE WILL IMPLEMENT IT FULLY**

### exFAT Complexity: LOW-MEDIUM
- Similar to FAT32
- Simpler than FAT32 in some ways
- Microsoft specification available

---

## Dependencies

### External Libraries to Study (Not Use Directly)
- dosfstools - FAT implementation reference
- e2fsprogs - ext implementation reference  
- ntfs-3g - NTFS implementation reference
- libexfat - exFAT reference

### We Will Implement
- Everything ourselves (learning exercise)
- Study existing implementations for reference
- Follow official specifications

---

## Success Criteria

Each sub-phase must:
1. Build successfully
2. Pass all unit tests
3. Have working CLI commands
4. Handle error cases gracefully
5. Include documentation

---

## Current Status

**Starting**: Phase 3.1
**Next**: Implement file system detection and basic structure

---

## Notes for Implementation

1. **Start Simple**: Get basic structures working first
2. **Test Often**: Create test images and verify structures
3. **Reference Existing**: Study dosfstools, e2fsprogs, ntfs-3g
4. **Safety First**: Never write to real disks during testing
5. **Documentation**: Document all structures and algorithms

---

## NTFS Commitment

**We will NOT skip NTFS.** Despite its complexity:
- It's the standard for Windows
- Essential for OS migration
- Required for professional partition manager
- We have 3-4 weeks allocated specifically for it
- Will study ntfs-3g and NTFS specification thoroughly

---

*Document Version: 1.0*
*Created: February 2026*
