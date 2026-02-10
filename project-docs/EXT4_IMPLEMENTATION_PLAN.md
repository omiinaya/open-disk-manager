# ext4 Implementation Plan

## Overview
ext4 is the standard Linux filesystem with journaling, extents, and modern features. Essential for Linux root partitions.

---

## Phase 3.3.1: ext4 Structure & Constants (Day 1-2)

### Goals
- Define all ext4 data structures
- Understand superblock, block groups, inodes

### Tasks
- [ ] Define ext4_super_block structure
- [ ] Define ext4_group_desc structure
- [ ] Define ext4_inode structure
- [ ] Define ext4_extent structures
- [ ] Define journal superblock
- [ ] Create block size calculation functions

### Deliverables
```cpp
struct ext4_super_block { /* 1024 bytes */ };
struct ext4_group_desc { /* 64 bytes */ };
struct ext4_inode { /* 256 bytes */ };
struct ext4_extent_header { /* 12 bytes */ };
```

### Test
```cpp
TEST(EXT4StructureTest, SuperblockSize) {
    EXPECT_EQ(sizeof(ext4_super_block), 1024);
}
```

---

## Phase 3.3.2: Superblock Creation (Day 3-5)

### Goals
- Create valid ext4 superblock
- Calculate filesystem layout

### Tasks
- [ ] Calculate block size (1K, 2K, 4K)
- [ ] Calculate blocks per group
- [ ] Calculate number of block groups
- [ ] Fill superblock fields
- [ ] Calculate inodes per group
- [ ] Set UUID
- [ ] Set volume label
- [ ] Set feature flags
- [ ] Write superblock

### Deliverables
```cpp
Result createSuperblock(uint64_t size, uint32_t block_size);
Result writeSuperblock(uint64_t sector, uint32_t block_size);
```

### Test
```cpp
TEST(EXT4Test, SuperblockMagic) {
    // Verify magic number 0xEF53
    // Verify block size
    // Verify block count
}
```

---

## Phase 3.3.3: Block Group Descriptors (Day 6-7)

### Goals
- Create block group descriptor table
- Initialize each group

### Tasks
- [ ] Calculate descriptor table size
- [ ] Create descriptor for each group
- [ ] Set block bitmap location
- [ ] Set inode bitmap location
- [ ] Set inode table location
- [ ] Calculate free blocks/inodes per group
- [ ] Write descriptor table
- [ ] Backup descriptor table

### Deliverables
```cpp
Result createBlockGroupDescriptors(uint32_t num_groups);
Result writeGroupDescriptors();
```

### Test
```cpp
TEST(EXT4Test, GroupDescriptors) {
    // Verify number of groups
    // Verify bitmap locations
    // Verify inode table locations
}
```

---

## Phase 3.3.4: Block & Inode Bitmaps (Day 8-9)

### Goals
- Initialize block bitmaps
- Initialize inode bitmaps
- Mark system blocks/inodes

### Tasks
- [ ] For each block group:
  - [ ] Create block bitmap
  - [ ] Mark superblock area
  - [ ] Mark descriptor table
  - [ ] Mark bitmap blocks
  - [ ] Mark inode table
  - [ ] Mark root inode
  - [ ] Create inode bitmap
  - [ ] Mark used inodes
- [ ] Write all bitmaps

### Deliverables
```cpp
Result createBlockBitmaps();
Result createInodeBitmaps();
```

### Test
```cpp
TEST(EXT4Test, BlockBitmaps) {
    // Verify system blocks marked
    // Verify free blocks correct count
}
```

---

## Phase 3.3.5: Inode Table (Day 10-11)

### Goals
- Initialize inode table
- Create root directory inode

### Tasks
- [ ] Calculate inode table size
- [ ] Initialize all inodes to zero
- [ ] Create inode 2 (root directory)
- [ ] Set directory mode
- [ ] Set timestamps
- [ ] Set extent tree (for ext4)
- [ ] Write inode table

### Deliverables
```cpp
Result createInodeTable();
Result createRootInode();
```

### Test
```cpp
TEST(EXT4Test, RootInode) {
    // Verify inode 2 is directory
    // Verify mode bits
    // Verify timestamps set
}
```

---

## Phase 3.3.6: Root Directory (Day 12)

### Goals
- Create root directory structure

### Tasks
- [ ] Calculate first data block
- [ ] Create directory block
- [ ] Add "." entry
- [ ] Add ".." entry
- [ ] Add volume label entry (if specified)
- [ ] Mark block as used in bitmap
- [ ] Write directory block

### Deliverables
```cpp
Result createRootDirectory();
```

### Test
```cpp
TEST(EXT4Test, RootDirectoryContents) {
    // Check "." entry
    // Check ".." entry
    // Check volume label
}
```

---

## Phase 3.3.7: Journal (ext3/4) (Day 13-15)

### Goals
- Create journal for ext3/ext4

### Tasks
- [ ] Calculate journal size
- [ ] Create journal superblock
- [ ] Initialize journal blocks
- [ ] Update superblock with journal info
- [ ] Create journal inode
- [ ] Mark journal blocks in bitmap

### Deliverables
```cpp
Result createJournal(uint32_t journal_size);
```

### Test
```cpp
TEST(EXT4Test, JournalStructure) {
    // Verify journal superblock
    // Verify journal inode exists
}
```

---

## Phase 3.3.8: Integration (Day 16-18)

### Goals
- Put it all together
- Complete format operation

### Tasks
- [ ] Implement `EXT4FileSystem::create()`
- [ ] Parameter validation
- [ ] Step-by-step format
- [ ] Progress callbacks
- [ ] Handle ext2/ext3 compatibility
- [ ] CLI format command

### Deliverables
```cpp
Result EXT4FileSystem::create(disk, start, size, label, block_size) {
    // 1. Validate parameters
    // 2. Calculate layout
    // 3. Create superblock
    // 4. Create group descriptors
    // 5. Create bitmaps
    // 6. Create inode table
    // 7. Create root directory
    // 8. Create journal (ext3/4)
    // 9. Write superblock backups
}
```

### Test
```cpp
TEST(EXT4IntegrationTest, FormatPartition) {
    // Create 4GB test image
    // Format as ext4
    // Mount and verify
}
```

---

## Phase 3.3.9: ext4 Check (Day 19-21)

### Goals
- Verify ext4 integrity
- Detect and repair corruption

### Tasks
- [ ] Check superblock
- [ ] Check group descriptors
- [ ] Verify bitmaps
- [ ] Check inode table
- [ ] Verify directory structure
- [ ] Check journal
- [ ] Count free blocks/inodes
- [ ] Report and optionally repair

### Deliverables
```cpp
Result EXT4FileSystem::check(disk, start, repair, errors);
```

---

## Phase 3.3.10: ext4 Resize (Day 22-24)

### Goals
- Expand and shrink ext4
- Online resize support foundation

### Tasks
- [ ] Calculate new layout
- [ ] Add new block groups
- [ ] Update superblock
- [ ] Extend descriptor table
- [ ] Initialize new bitmaps
- [ ] For shrink: validate enough free space

### Deliverables
```cpp
Result EXT4FileSystem::resize(disk, start, new_size);
```

---

## Implementation Order

### Week 1-2: Foundation
- Day 1-2: Phase 3.3.1 (Structures)
- Day 3-5: Phase 3.3.2 (Superblock)
- Day 6-7: Phase 3.3.3 (Group Descriptors)

### Week 3-4: Tables & Directories
- Day 8-9: Phase 3.3.4 (Bitmaps)
- Day 10-11: Phase 3.3.5 (Inode Table)
- Day 12: Phase 3.3.6 (Root Directory)
- Day 13-15: Phase 3.3.7 (Journal)

### Week 4: Integration & Testing
- Day 16-18: Phase 3.3.8 (Integration)
- Day 19-21: Phase 3.3.9 (Check)
- Day 22-24: Phase 3.3.10 (Resize)

---

## Key Constants

```cpp
// Magic Numbers
constexpr uint16_t EXT4_SUPER_MAGIC = 0xEF53;
constexpr uint32_t EXT4_JNL_BACKUP_BLOCKS = 337;

// Block Sizes
constexpr uint32_t EXT4_MIN_BLOCK_SIZE = 1024;
constexpr uint32_t EXT4_MAX_BLOCK_SIZE = 4096;

// Feature Incompat (ext4 requires these)
constexpr uint32_t EXT4_FEATURE_INCOMPAT_EXTENTS = 0x40;
constexpr uint32_t EXT4_FEATURE_INCOMPAT_64BIT = 0x80;
constexpr uint32_t EXT4_FEATURE_INCOMPAT_FLEX_BG = 0x200;

// Inodes
constexpr uint32_t EXT4_ROOT_INO = 2;
constexpr uint32_t EXT4_JOURNAL_INO = 8;
```

---

## Block Group Layout

```
Block 0: Boot sector (if any)
Block 1-2: Superblock (1K each, 2 copies)
Block 3+: Group descriptors
Block ?: Block bitmap
Block ?: Inode bitmap  
Block ?: Inode table
Block ?: Data blocks
```

---

## Testing Strategy

1. **Unit Tests**: Test each structure
2. **Integration**: Full format
3. **Mount Test**: Mount with Linux
4. **fsck**: Run e2fsck on created FS
5. **Stress**: Large volumes, many files

---

## References

- Linux kernel: `fs/ext4/`
- e2fsprogs source
- ext4 wiki: ext4.wiki.kernel.org
- "Design and Implementation of the ext4 Filesystem"

---

## Current Status

**Phase**: 3.3.1 - Starting
**Next**: Define ext4 structures
**Estimated Completion**: 3-4 weeks

---

*Document Version: 1.0*
*Created: February 2026*
