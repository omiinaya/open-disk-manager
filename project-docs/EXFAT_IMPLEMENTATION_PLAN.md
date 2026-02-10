# exFAT Implementation Plan

## Overview
exFAT is Microsoft's modern FAT replacement, optimized for flash drives and large volumes. Simpler than NTFS, supports files >4GB.

---

## Phase 3.5.1: exFAT Structure & Constants (Day 1)

### Goals
- Define exFAT data structures
- Understand allocation bitmap

### Tasks
- [ ] Define exFATBootSector structure
- [ ] Define exFATFSInfo structure
- [ ] Define directory entry structures
- [ ] Define allocation bitmap structure
- [ ] Document cluster heap

### Deliverables
```cpp
struct exFATBootSector { /* 512 bytes */ };
struct exFATChecksumSector { /* 512 bytes */ };
struct exFATDirectoryEntry { /* 32 bytes */ };
```

---

## Phase 3.5.2: Boot Sector (Day 2-3)

### Goals
- Create exFAT boot sector

### Tasks
- [ ] Calculate cluster size
- [ ] Calculate FAT offset
- [ ] Calculate cluster heap offset
- [ ] Fill boot sector
- [ ] Calculate checksum
- [ ] Write boot sector
- [ ] Write backup boot sector
- [ ] Write checksum sector

---

## Phase 3.5.3: Allocation Bitmap (Day 4)

### Goals
- Create cluster allocation bitmap

### Tasks
- [ ] Calculate bitmap size
- [ ] Create bitmap buffer
- [ ] Mark system clusters
- [ ] Write bitmap
- [ ] Update boot sector

---

## Phase 3.5.4: FAT Table (Day 5)

### Goals
- Create FAT (simpler than FAT32)

### Tasks
- [ ] Create FAT buffer
- [ ] Entry 0: Media type
- [ ] Entry 1: Reserved
- [ ] Entry 2: Root directory (EOC)
- [ ] Write FAT

---

## Phase 3.5.5: Root Directory (Day 6)

### Goals
- Create root directory
- Set volume label

### Tasks
- [ ] Create volume label entry
- [ ] Create allocation bitmap entry
- [ ] Create upcase table entry
- [ ] Create root cluster
- [ ] Mark cluster used
- [ ] Write directory

---

## Phase 3.5.6: Integration (Day 7)

### Goals
- Complete format operation

### Tasks
- [ ] Implement `exFATFileSystem::create()`
- [ ] CLI format command

---

## Phase 3.5.7: Check & Resize (Day 8-10)

### Goals
- Verify and resize exFAT

### Tasks
- [ ] Check boot sector
- [ ] Verify bitmap
- [ ] Check FAT
- [ ] Expand exFAT

---

## Implementation Order

### Week 1
- Day 1: Phase 3.5.1 (Structures)
- Day 2-3: Phase 3.5.2 (Boot Sector)
- Day 4: Phase 3.5.3 (Bitmap)
- Day 5: Phase 3.5.4 (FAT)
- Day 6: Phase 3.5.5 (Root Directory)
- Day 7: Phase 3.5.6 (Integration)
- Day 8-10: Phase 3.5.7 (Check & Resize)

---

## Key Constants

```cpp
constexpr uint8_t EXFAT_OEM_ID[8] = {'E', 'X', 'F', 'A', 'T', ' ', ' ', ' '};
constexpr uint16_t EXFAT_BOOT_SIGNATURE = 0xAA55;
constexpr uint32_t EXFAT_EOC = 0xFFFFFFFF;
constexpr uint32_t EXFAT_FREE = 0x00000000;
constexpr uint32_t EXFAT_BAD = 0xFFFFFFF7;
```

---

## Current Status

**Phase**: 3.5.1 - Starting
**Estimated**: 1-2 weeks

---

*Document Version: 1.0*
*Created: February 2026*
