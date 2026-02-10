# NTFS Implementation Plan

## Overview
NTFS is the native Windows filesystem with advanced features including journaling, compression, encryption, and ACLs. This is the most complex implementation but ESSENTIAL for Windows compatibility.

---

## Phase 3.4.1: NTFS Structure & Constants (Day 1-3)

### Goals
- Define all NTFS data structures
- Understand MFT (Master File Table)
- Document attribute types

### Tasks
- [ ] Define NTFS_BOOT_SECTOR structure
- [ ] Define FILE_RECORD_SEGMENT_HEADER (MFT record)
- [ ] Define ATTRIBUTE_RECORD_HEADER
- [ ] Define standard attribute types
- [ ] Define bitmap structures
- [ ] Document MFT layout
- [ ] Create NTFS constants header

### Deliverables
```cpp
struct NTFSBootSector { /* 512 bytes */ };
struct MFTRecord { /* 1024 bytes */ };
struct AttributeHeader { /* variable */ };
struct ResidentAttribute { /* ... */ };
struct NonResidentAttribute { /* ... */ };
```

### Test
```cpp
TEST(NTFSStructureTest, BootSectorSize) {
    EXPECT_EQ(sizeof(NTFSBootSector), 512);
}

TEST(NTFSStructureTest, MFTRecordSize) {
    EXPECT_EQ(sizeof(MFTRecord), 1024);
}
```

---

## Phase 3.4.2: Boot Sector Creation (Day 4-5)

### Goals
- Create valid NTFS boot sector
- Write to disk

### Tasks
- [ ] Calculate cluster size (sectors per cluster)
- [ ] Calculate total sectors
- [ ] Calculate MFT location (cluster)
- [ ] Calculate MFT mirror location
- [ ] Fill boot sector fields
- [ ] Generate serial number
- [ ] Calculate checksum
- [ ] Write boot sector
- [ ] Verify readback

### Deliverables
```cpp
Result createBootSector(uint64_t size, uint8_t sectors_per_cluster);
Result writeBootSector(uint64_t sector);
uint32_t calculateBootChecksum(const uint8_t* data);
```

### Test
```cpp
TEST(NTFSTest, CreateBootSector) {
    // Create 100GB test image
    // Create boot sector
    // Verify "NTFS    " at offset 3
    // Verify checksum
}
```

---

## Phase 3.4.3: MFT Record Structure (Day 6-8)

### Goals
- Create MFT record structure
- Handle attributes

### Tasks
- [ ] Create MFT record header
- [ ] Implement attribute parsing
- [ ] Handle resident attributes
- [ ] Handle non-resident attributes
- [ ] Update sequence handling
- [ ] Fixup array
- [ ] Write MFT record
- [ ] Read MFT record

### Deliverables
```cpp
Result createMFTRecord(MFTRecord& record);
Result writeAttribute(MFTRecord& record, uint32_t attr_type, 
                       const void* data, uint32_t size);
Result readAttribute(const MFTRecord& record, uint32_t attr_type,
                      void* buffer, uint32_t size);
```

### Test
```cpp
TEST(NTFSTest, MFTRecordHeader) {
    // Verify "FILE" magic
    // Verify attribute offset
    // Test resident attribute
}
```

---

## Phase 3.4.4: $MFT File (Day 9-12)

### Goals
- Create Master File Table
- Initialize system files

### Tasks
- [ ] Calculate MFT size
- [ ] Create $MFT record (MFT entry 0)
- [ ] Create $MFTMirr record (entry 1)
- [ ] Create $LogFile record (entry 2)
- [ ] Create $Volume record (entry 3)
- [ ] Create $AttrDef record (entry 4)
- [ ] Create $Root record (entry 5)
- [ ] Create $Bitmap record (entry 6)
- [ ] Create $Boot record (entry 7)
- [ ] Create $BadClus record (entry 8)
- [ ] Create $Secure record (entry 9)
- [ ] Create $UpCase record (entry 10)
- [ ] Create $Extend record (entry 11)
- [ ] Write MFT to disk

### Deliverables
```cpp
Result createSystemFiles();
Result createMFTFile(uint64_t mft_cluster, uint32_t clusters_per_mft);
```

### Test
```cpp
TEST(NTFSTest, SystemFiles) {
    // Verify $MFT record
    // Verify $Volume record
    // Verify system file count
}
```

---

## Phase 3.4.5: Attributes (Day 13-16)

### Goals
- Create all standard attributes
- Implement attribute creation

### Tasks
- [ ] $STANDARD_INFORMATION attribute
- [ ] $FILE_NAME attribute
- [ ] $DATA attribute
- [ ] $BITMAP attribute
- [ ] $VOLUME_INFORMATION attribute
- [ ] $VOLUME_NAME attribute
- [ ] $INDEX_ROOT attribute
- [ ] $INDEX_ALLOCATION attribute
- [ ] Resident vs non-resident logic

### Deliverables
```cpp
Result createStandardInfoAttr(uint64_t& attr);
Result createFileNameAttr(const std::string& name, uint64_t& attr);
Result createDataAttr(uint64_t size, bool resident, uint64_t& attr);
```

### Test
```cpp
TEST(NTFSTest, Attributes) {
    // Create resident attribute
    // Create non-resident attribute
    // Verify attribute header
}
```

---

## Phase 3.4.6: $Bitmap (Day 17-18)

### Goals
- Create cluster bitmap
- Mark system clusters

### Tasks
- [ ] Calculate bitmap size
- [ ] Create bitmap buffer
- [ ] Mark boot sector
- [ ] Mark MFT clusters
- [ ] Mark system file clusters
- [ ] Mark $Bitmap cluster
- [ ] Write $Bitmap file
- [ ] Update $Bitmap attribute

### Deliverables
```cpp
Result createBitmap(uint64_t total_clusters);
Result markCluster(uint64_t cluster, bool used);
```

### Test
```cpp
TEST(NTFSTest, Bitmap) {
    // Verify system clusters marked
    // Verify free cluster count
}
```

---

## Phase 3.4.7: $LogFile (Journal) (Day 19-21)

### Goals
- Create NTFS journal
- Initialize log file structure

### Tasks
- [ ] Calculate journal size
- [ ] Create restart pages
- [ ] Initialize log page header
- [ ] Create log file records
- [ ] Set up client records
- [ ] Write $LogFile
- [ ] Update $LogFile record in MFT

### Deliverables
```cpp
Result createLogFile(uint64_t size);
Result writeLogFile(uint64_t cluster);
```

### Test
```cpp
TEST(NTFSTest, LogFile) {
    // Verify restart pages
    // Verify log structure
}
```

---

## Phase 3.4.8: Root Directory (Day 22-24)

### Goals
- Create root directory
- Add volume label

### Tasks
- [ ] Create $Root record
- [ ] Create INDEX_ROOT attribute
- [ ] Add volume label entry
- [ ] Set timestamps
- [ ] Write directory record
- [ ] Update MFT
- [ ] Update $Bitmap

### Deliverables
```cpp
Result createRootDirectory(const std::string& label);
Result addIndexEntry(uint64_t mft_ref, const std::string& name);
```

### Test
```cpp
TEST(NTFSTest, RootDirectory) {
    // Verify $Root record
    // Verify volume label
    // Verify index entries
}
```

---

## Phase 3.4.9: $UpCase (Day 25)

### Goals
- Create uppercase table
- Required for NTFS

### Tasks
- [ ] Create 64KB uppercase table
- [ ] Convert ASCII to Unicode uppercase
- [ ] Create $UpCase record
- [ ] Write $UpCase file

### Deliverables
```cpp
Result createUpCaseTable();
```

---

## Phase 3.4.10: Integration (Day 26-30)

### Goals
- Complete format operation
- All pieces together

### Tasks
- [ ] Implement `NTFSFileSystem::create()`
- [ ] Parameter validation
- [ ] Step-by-step format
- [ ] Progress callbacks
- [ ] Error handling
- [ ] Rollback on failure
- [ ] CLI format command

### Deliverables
```cpp
Result NTFSFileSystem::create(disk, start, size, label, cluster_size) {
    // 1. Validate parameters
    // 2. Calculate layout
    // 3. Create boot sector
    // 4. Create MFT
    // 5. Create system files
    // 6. Create $Bitmap
    // 7. Create $LogFile
    // 8. Create root directory
    // 9. Create $UpCase
    // 10. Write MFT mirror
}
```

### Test
```cpp
TEST(NTFSIntegrationTest, FormatPartition) {
    // Create 100GB test image
    // Format as NTFS
    // Mount with Windows or ntfs-3g
}
```

---

## Phase 3.4.11: NTFS Check (Day 31-35)

### Goals
- Verify NTFS integrity
- Detect corruption

### Tasks
- [ ] Check boot sector
- [ ] Verify MFT
- [ ] Check $Bitmap
- [ ] Verify journal
- [ ] Check attributes
- [ ] Verify fixups
- [ ] Check clusters
- [ ] Report errors

### Deliverables
```cpp
Result NTFSFileSystem::check(disk, start, repair, errors);
```

---

## Phase 3.4.12: NTFS Resize (Day 36-40)

### Goals
- Expand NTFS
- Update all structures

### Tasks
- [ ] Calculate new layout
- [ ] Extend $Bitmap
- [ ] Update boot sector
- [ ] Extend MFT if needed
- [ ] Mark new clusters free
- [ ] Update volume info

### Deliverables
```cpp
Result NTFSFileSystem::resize(disk, start, new_size);
```

---

## Implementation Order

### Week 1-2: Foundation
- Day 1-3: Phase 3.4.1 (Structures)
- Day 4-5: Phase 3.4.2 (Boot Sector)
- Day 6-8: Phase 3.4.3 (MFT Records)

### Week 3-4: Core Components
- Day 9-12: Phase 3.4.4 ($MFT)
- Day 13-16: Phase 3.4.5 (Attributes)
- Day 17-18: Phase 3.4.6 ($Bitmap)
- Day 19-21: Phase 3.4.7 ($LogFile)

### Week 5-6: Completion
- Day 22-24: Phase 3.4.8 (Root Directory)
- Day 25: Phase 3.4.9 ($UpCase)
- Day 26-30: Phase 3.4.10 (Integration)
- Day 31-35: Phase 3.4.11 (Check)
- Day 36-40: Phase 3.4.12 (Resize)

---

## Key Constants

```cpp
// NTFS Magic
constexpr uint8_t NTFS_OEM_ID[8] = {'N', 'T', 'F', 'S', ' ', ' ', ' ', ' '};
constexpr uint32_t MFT_RECORD_MAGIC = 0x454C4946;  // "FILE"
constexpr uint32_t INDX_RECORD_MAGIC = 0x58444E49; // "INDX"

// MFT Record Numbers
constexpr uint64_t MFT_MFT = 0;
constexpr uint64_t MFT_MIRR = 1;
constexpr uint64_t MFT_LOGFILE = 2;
constexpr uint64_t MFT_VOLUME = 3;
constexpr uint64_t MFT_ATTRDEF = 4;
constexpr uint64_t MFT_ROOT = 5;
constexpr uint64_t MFT_BITMAP = 6;
constexpr uint64_t MFT_BOOT = 7;
constexpr uint64_t MFT_BADCLUS = 8;
constexpr uint64_t MFT_SECURE = 9;
constexpr uint64 = MFT_UPCASE = 10;
constexpr uint64_t MFT_EXTEND = 11;

// Attribute Types
constexpr uint32_t ATTR_STANDARD_INFO = 0x10;
constexpr uint32_t ATTR_ATTRIBUTE_LIST = 0x20;
constexpr uint32_t ATTR_FILE_NAME = 0x30;
constexpr uint32_t ATTR_OBJECT_ID = 0x40;
constexpr uint32_t ATTR_SECURITY_DESC = 0x50;
constexpr uint32_t ATTR_VOLUME_NAME = 0x60;
constexpr uint32_t ATTR_VOLUME_INFO = 0x70;
constexpr uint32_t ATTR_DATA = 0x80;
constexpr uint32_t ATTR_INDEX_ROOT = 0x90;
constexpr uint32_t ATTR_INDEX_ALLOCATION = 0xA0;
constexpr uint32_t ATTR_BITMAP = 0xB0;
constexpr uint32_t ATTR_REPARSE_POINT = 0xC0;
constexpr uint32_t ATTR_EA_INFO = 0xD0;
constexpr uint32_t ATTR_EA = 0xE0;
constexpr uint32_t ATTR_LOGGED_UTILITY_STREAM = 0x100;

// Flags
constexpr uint16_t FILE_RECORD_FLAG_IN_USE = 0x01;
constexpr uint16_t FILE_RECORD_FLAG_DIR = 0x02;
```

---

## MFT Record Structure

```
+0:   "FILE" magic (4 bytes)
+4:   Update sequence offset (2 bytes)
+6:   Update sequence count (2 bytes)
+8:   Log file sequence number (8 bytes)
+16:  Sequence number (2 bytes)
+18:  Hard link count (2 bytes)
+20:  Offset to first attribute (2 bytes)
+22:  Flags (2 bytes)
+24:  Used size of MFT entry (4 bytes)
+28:  Allocated size of MFT entry (4 bytes)
+32:  Base file record (8 bytes)
+40:  Next attribute ID (2 bytes)
+42:  XP fixup (2 bytes)
+44:  Update sequence number (2 bytes)
+46:  Update sequence array (variable)
+?:   Attributes (variable)
+?:   End marker (0xFFFFFFFF)
```

---

## Testing Strategy

1. **Unit Tests**: Each component
2. **Integration**: Full format
3. **Windows**: Mount on Windows
4. **ntfs-3g**: Mount on Linux
5. **chkdsk**: Run Windows check
6. **Stress**: Large volumes

---

## References

- NTFS Documentation (Microsoft)
- Linux ntfs-3g source
- "Inside NTFS"
- NTFS-3G Technical Reference
- Linux kernel ntfs driver

---

## Complexity Notes

NTFS is significantly more complex than FAT32/ext4:
- **MFT**: Complex multi-record structure
- **Attributes**: Variable size, resident/non-resident
- **Journaling**: $LogFile structure
- **Indexes**: B-tree structure for directories
- **Security**: $Secure, $SDS attributes
- **Unicode**: All names stored in UTF-16

**But we WILL implement it fully** as committed.

---

## Current Status

**Phase**: 3.4.1 - Starting
**Next**: Define NTFS structures
**Estimated Completion**: 6-8 weeks
**Priority**: HIGH (not skipped)

---

## Commitment

**THIS IS NOT SKIPPED.**

Despite complexity, NTFS is:
- Standard Windows filesystem
- Essential for OS migration
- Required for professional tool
- We'll implement every component

---

*Document Version: 1.0*
*Created: February 2026*
*Commitment: Full NTFS Implementation*
