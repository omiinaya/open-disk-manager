---
sidebar_position: 2
---

# Installation

## Quick Install

```bash
# Clone and build
git clone https://github.com/openpartitionmanager/opm.git
cd opm
mkdir build && cd build
cmake .. && make -j4
sudo make install
```

## Requirements

- Linux kernel 3.10+
- CMake 3.16+
- GCC 9+ or Clang 10+
- libblkid-dev

## Verify Installation

```bash
opm --version
```
