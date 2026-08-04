# Open Partition Manager — User Manual

## Installation

### From source (Linux)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
sudo cmake --install build        # installs opm, headers, man page
```

### GUI

```bash
cmake -S . -B build-gui -DCMAKE_BUILD_TYPE=Release -DBUILD_GUI=ON -DBUILD_TESTS=OFF
cmake --build build-gui -j$(nproc)
./build-gui/bin/opm-gui
```

Requires Qt 5.15+ or Qt 6. The GUI needs root to perform operations.

### Packages

- **DEB / RPM**: `cpack -G DEB` / `cpack -G RPM` after configuring
- **Docker**: `docker build -t opm .` (CLI image with optional security tools)
- **Windows**: cross-compile with MinGW or use the CI workflow
- **macOS**: `brew install cmake ninja && cmake -S . -B build ...`

## Safety model

- All destructive commands support `--dry-run` (create/delete/resize).
- Operations validate before applying: overlap checks, alignment, size limits.
- `opm recover` never touches partition data — it only rewrites the table.
- `opm align --fix` copies data before moving entries.
- `opm convert` refuses conversions that would lose information (>4 partitions
  or beyond the 2 TiB limit when targeting MBR).

**Always back up important data before partition operations.**

## Common tasks

```bash
# List disks
opm list

# Create a GPT table + partition
opm mklabel /dev/sdb gpt
opm create /dev/sdb 2048 100G linux data

# Format + verify
opm format /dev/sdb ext4 2048 100G mydata
opm check /dev/sdb 2048

# Set the volume label
opm label /dev/sdb 2048 NEWLABEL

# Resize / move (data-preserving)
opm resize /dev/sdb 1 200G
opm move /dev/sdb 1 4096

# Convert the table in place
opm convert /dev/sdb gpt

# Fix alignment
opm align /dev/sdb --fix

# Recover a lost partition table
opm recover /dev/sdb --rebuild

# Recover deleted files on FAT32
opm undelete /dev/sdb 2048            # list
opm undelete /dev/sdb 2048 --restore 1

# Reset a Linux password (edit /etc/shadow on the target root)
opm reset-password --linux /mnt/root/etc/shadow alice newpass

# LUKS / BitLocker / UEFI (requires the matching tool installed)
opm luks open /dev/sdb1 crypted
opm bitlocker unlock /dev/sdc /mnt/bl
opm boot-repair --uefi
```

## Troubleshooting

| Symptom | Cause / fix |
|---------|-------------|
| `Permission denied` | Run as root (`sudo opm ...`); GUI needs root too |
| `cryptsetup is not installed` | Optional feature — `apt install cryptsetup` (also `dislocker`, `efibootmgr`, `chntpw`) |
| `No partition table found` | Disk is blank or table is corrupt — try `opm recover /dev/sdX --rebuild` |
| `check` fails on a fresh format | Layout limits (e.g. exFAT needs ≥512 MB); use a larger partition |
| `Device busy` | Unmount the volume first (`umount /mnt/point`) |
| CLI E2E tests need `C:\tmp` on Windows | CI creates it; locally `mkdir C:\tmp` |

## License

GPL-3.0 — free for personal, commercial, educational, and government use.
