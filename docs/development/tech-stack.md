---
sidebar_position: 2
---

# Technology Stack

## Core Technologies

| Technology | Purpose |
|------------|---------|
| C++17 | Primary language |
| CMake 3.16+ | Build system |
| Google Test | Unit testing |
| Docusaurus | Documentation |

## Platform Support

| Platform | Status |
|----------|--------|
| Linux | ✅ Primary |
| Windows | 📋 Phase 7 |
| macOS | 📋 Phase 7 |

## Dependencies

### Required
- libblkid-dev (Linux device identification)
- CMake 3.16+
- C++17 compiler

### Optional
- Google Test (for tests)
- Qt (for GUI - Phase 6)

## Build Tools

```bash
# Standard build
mkdir build && cd build
cmake ..
make -j$(nproc)

# With tests
cmake -DBUILD_TESTS=ON ..
make -j$(nproc)
```
