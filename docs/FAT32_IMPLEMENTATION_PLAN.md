# FAT32 Implementation Plan

## Overview
FAT32 is the most compatible filesystem across Windows, macOS, and Linux. It's essential for USB drives and bootable media.

---

## Phase 3.2.1: FAT32 Structure & Constants (Day 1-2)

### Goals
- Define all FAT32 data structures
- Create constants and calculations
- No actual code execution yet

### Tasks
- [ ] Define FAT32BootSector structure
- [ ] Define FSInfo sector structure
- [ ] Define FAT entry constants
- [ ] Create cluster calculation functions
- [ ] Document FAT table layout
- [ ] Create test: verify structure sizes

### Deliverables
```cpp
struct FAT32BootSector { /* ... */ };
struct FAT32FSInfo { /* ... */ };
constexpr uint32_t FAT32_EOC = 0x0FFFFFFF;
```

### Test
```cpp
TEST(FAT32StructureTest, BootSectorSize) {
    EXPECT_EQ(sizeof(FAT32BootSector), 512);
}
```

---

## Phase 3.2.2: Boot Sector Creation (Day 3-4)

### Goals
- Create valid FAT32 boot sector
- Write to disk
- Verify written data

### Tasks
- [ ] Calculate cluster size based on volume size
- [ ] Calculate FAT size
- [ ] Fill BPB (BIOS Parameter Block)
- [ ] Add boot signature (0xAA55)
- [ ] Write boot sector to disk
- [ ] Write backup boot sector
- [ ] Verify with readback

### Deliverables
```cpp
Result createBootSector(uint64_t size, uint32_t cluster_size);
Result writeBootSector(uint64_t sector);
```

### Test
```cpp
TEST(FAT32Test, CreateBootSector) {
    // Create 1GB test image
    // Format with FAT32
    // Verify boot sector signature
    // Verify FAT32 string at offset 82
}
```

---

## Phase 3.2.3: FAT Table Initialization (Day 5-6)

### Goals
- Create File Allocation Tables
- Initialize root cluster
- Write to disk

### Tasks
- [ ] Calculate number of FAT entries needed
- [ ] Create FAT buffer
- [ ] Entry 0: Media descriptor + reserved
- [ ] Entry 1: EOF marker (0xFFFFFFFF)
- [ ] Entry 2: Root directory (EOF marker)
- [ ] Write FAT1
- [ ] Write FAT2 (mirror)
- [ ] Verify FAT tables match

### Deliverables
```cpp
Result createFATTables(uint32_t fat_sectors);
Result writeFAT(uint32_t fat_num, uint32_t fat_sectors);
```

### Test
```cpp
TEST(FAT32Test, FATTableContents) {
    // Check entry 0
    // Check entry 1 is EOC
    // Check entry 2 is EOC (root)
    // Verify FAT1 == FAT2
}
```

---

## Phase 3.2.4: FSInfo Sector (Day 7)

### Goals
- Create FSInfo sector for free space tracking

### Tasks
- [ ] Create FSInfo structure
- [ ] Signature "RRaA"
- [ ] Free cluster count (initially all free)
- [ ] Next free cluster hint
- [ ] Write FSInfo sector
- [ ] Write backup FSInfo
- [ ] Verify with readback

### Deliverables
```cpp
Result createFSInfo(uint32_t total_clusters);
Result writeFSInfo(uint32_t fs_info_sector);
```

### Test
```cpp
TEST(FAT32Test, FSInfoStructure) {
    // Verify signature at offset 0
    // Verify free cluster count
    // Verify next free cluster hint
}
```

---

## Phase 3.2.5: Root Directory (Day 8)

### Goals
- Initialize root directory cluster
- Set volume label

### Tasks
- [ ] Clear root cluster
- [ ] Add volume label entry (if provided)
- [ ] Calculate first data cluster
- [ ] Write root cluster
- [ ] Verify empty directory

### Deliverables
```cpp
Result createRootDirectory(const std::string& label);
Result writeRootCluster(uint32_t cluster);
```

### Test
```cpp
TEST(FAT32Test, RootDirectory) {
    // Check cluster is allocated
    // Check for volume label entry
    // Verify no other entries
}
```

---

## Phase 3.2.6: Integration - Complete Format (Day 9-10)

### Goals
- Put it all together
- End-to-end format operation

### Tasks
- [ ] Implement `FAT32FileSystem::create()`
- [ ] Validation of parameters
- [ ] Step-by-step format
- [ ] Rollback on error
- [ ] Progress callbacks
- [ ] CLI format command

### Deliverables
```cpp
Result FAT32FileSystem::create(disk, start, size, label, cluster_size) {
    // 1. Validate parameters
    // 2. Calculate layout
    // 3. Write boot sector
    // 4. Write FAT tables
    // 5. Write FSInfo
    // 6. Initialize root directory
    // 7. Flush changes
}
```

### Test
```cpp
TEST(FAT32IntegrationTest, FormatPartition) {
    // Create 2GB test image
    // Format as FAT32
    // Mount (if possible) or verify structure
    // Check all components present
}
```

---

## Phase 3.2.7: FAT32 Check (Day 11-13)

### Goals
- Verify FAT32 integrity
- Detect corruption

### Tasks
- [ ] Check boot sector signature
- [ ] Check FAT signatures
- [ ] Compare FAT1 and FAT2
- [ ] Check for orphaned clusters
- [ ] Check free cluster count matches FSInfo
- [ ] Report errors

### Deliverables
```cpp
Result FAT32FileSystem::check(disk, start, repair, errors) {
    // Verify all structures
    // Report errors
    // Optionally repair
}
```

### Test
```cpp
TEST(FAT32Test, CheckValid) {
    // Format FAT32
    // Check should pass
}

TEST(FAT32Test, CheckCorrupt) {
    // Corrupt FAT
    // Check should detect error
}
```

---

## Phase 3.2.8: FAT32 Resize (Day 14)

### Goals
- Expand FAT32 (grow only)
- Update all structures

### Tasks
- [ ] Calculate new layout
- [ ] Extend FAT tables
- [ ] Update boot sector
- [ ] Update FSInfo
- [ ] Mark new clusters free

### Deliverables
```cpp
Result FAT32FileSystem::resize(disk, start, new_size);
```

### Test
```cpp
TEST(FAT32Test, ResizeExpand) {
    // Format 1GB
    // Resize to 2GB
    // Verify structures updated
}
```

---

## Implementation Order

### Week 1: Foundation
- Day 1-2: Phase 3.2.1 (Structures)
- Day 3-4: Phase 3.2.2 (Boot Sector)
- Day 5-7: Phase 3.2.3 (FAT Tables)

### Week 2: Completion
- Day 8: Phase 3.2.4 (FSInfo)
- Day 9-10: Phase 3.2.5 (Root Directory)
- Day 11-13: Phase 3.2.6 (Integration)
- Day 14: Phase 3.2.7-3.2.8 (Check & Resize)

---

## Key Constants

```cpp
// FAT32 Special Values
constexpr uint32_t FAT32_EOC = 0x0FFFFFFF;  // End of chain
constexpr uint32_t FAT32_FREE = 0x00000000; // Free cluster
constexpr uint32_t FAT32_BAD = 0x0FFFFFF7;  // Bad cluster
constexpr uint32_t FAT32_RESERVED_START = 0x0FFFFFF0;

// Boot Sector
constexpr uint16_t BOOT_SIGNATURE = 0xAA55;
constexpr uint32_t FSINFO_SIGNATURE_LEAD = 0x41615252;  // "RRaA"
constexpr uint32_t FSINFO_SIGNATURE_STRUC = 0x61417272;  // "rrAa"
constexpr uint32_t FSINFO_SIGNATURE_TRAIL = 0xAA550000;

// Media Descriptors
constexpr uint8_t MEDIA_FIXED = 0xF8;
constexpr uint8_t MEDIA_REMOVABLE = 0xF0;
```

---

## Cluster Size Selection

| Volume Size | Sectors/Cluster | Cluster Size |
|-------------|----------------|--------------|
| 64MB - 128MB | 2 | 1KB |
| 128MB - 256MB | 4 | 2KB |
| 256MB - 1GB | 8 | 4KB |
| 1GB - 16GB | 16 | 8KB |
| 16GB - 32GB | 32 | 16KB |
| 32GB - 2TB | 64 | 32KB |

---

## Testing Strategy

1. **Unit Tests**: Test each component in isolation
2. **Integration Tests**: Test full format process
3. **Verification**: Mount with OS and check
4. **Stress Tests**: Large volumes, edge cases

---

## References

- Microsoft FAT32 Specification
- "FAT: General Overview of On-Disk Format"
- dosfstools (mkfs.fat implementation)
- Linux kernel fs/fat implementation

---

## Current Status

**Phase**: 3.2.1 - Starting
**Next**: Define FAT32 structures
**Estimated Completion**: 2 weeks

---

*Document Version: 1.0*
*Created: February 2026*
