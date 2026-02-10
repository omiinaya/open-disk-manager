---
sidebar_position: 4
---

# Build from Source

## Prerequisites

```bash
# Ubuntu/Debian
sudo apt-get install build-essential cmake git libblkid-dev

# CentOS/RHEL/Fedora
sudo dnf install gcc-c++ cmake git libblkid-devel
```

## Build

```bash
git clone https://github.com/openpartitionmanager/opm.git
cd opm
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Test

```bash
./tests/opm_tests
```

## Install

```bash
sudo make install
```
