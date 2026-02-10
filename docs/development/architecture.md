---
sidebar_position: 1
---

# Architecture Overview

## Layer Structure

```
User Interface (CLI → GUI)
    ↓
Operation Management (Transactions)
    ↓
Core Operations (Partition/FS)
    ↓
Filesystem Implementations
    ↓
Partition Table (MBR/GPT)
    ↓
Abstraction (Disk I/O)
    ↓
Platform (Linux/Win/Mac)
```

## Key Components

### Transaction System
Every operation is transactional with automatic rollback.

### Filesystem Implementations
- FAT32: From scratch (~2,060 LOC)
- EXT4: From scratch (~2,100 LOC)
- NTFS: From scratch (~1,020 LOC)

### Safety Features
- Validation before operations
- Automatic rollback
- Progress monitoring
- Data protection

## Technology Stack
- C++17
- CMake 3.16+
- Google Test
- Docusaurus (docs)
