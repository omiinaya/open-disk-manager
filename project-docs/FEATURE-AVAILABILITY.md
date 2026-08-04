# Feature Availability

## Single Edition, Complete Access

**Open Partition Manager** follows the true open-source philosophy: **one version with all features**. Unlike commercial partition managers that lock features behind paywalls, everything that exists in this project is available to every user for free.

| Tool | Free Version | Paid Versions | Total Cost |
|------|-------------|---------------|------------|
| **EaseUS Partition Master** | Limited features | Pro ($69.95), Server ($259), Unlimited ($599), Technician ($799) | Up to $799 |
| **AOMEI Partition Assistant** | Limited features | Pro ($49.95), Server ($199), Unlimited ($399), Technician ($699) | Up to $699 |
| **MiniTool Partition Wizard** | Limited features | Pro ($59), Server ($159), Enterprise ($399), Technician ($699) | Up to $699 |
| **Acronis Disk Director** | No free version | Home ($49.99), Server (custom pricing) | $49.99+ |
| **Our Open Source Tool** | ✅ **COMPLETE** | **N/A - Everything Free** | **$0** |

---

## Actual Implementation Status (August 2026)

> This table reflects what the code **actually implements** today, not aspirations.
> Status legend: ✅ implemented & tested · 🚧 partial / honest-error fallback · 📋 planned

### Partition Table Operations

| Feature | Status | Notes |
|---------|--------|-------|
| MBR read / parse / validate | ✅ | Protective-MBR detection, disk signature, extended partitions |
| GPT read / parse / validate | ✅ | CRC32-verified headers, backup GPT, UTF-16 names, GUIDs |
| GPT create / delete / resize / commit | ✅ | Primary + backup sync, protective MBR, CRC recompute |
| GPT recover from backup | ✅ | `GPTTable::recover()` + `restoreFromBackup()` |
| MBR create / delete / resize | ✅ | 1MiB alignment, overlap validation |
| Move partition (data-preserving) | ✅ | Copy sectors → re-create entry → drop old entry |
| mklabel (MBR / GPT) | ✅ | Wipes stale GPT when labeling MBR |
| APM / BSD disklabel | 📋 | Planned |

### Filesystems

| Filesystem | Format | Check | Resize | Notes |
|-----------|--------|-------|--------|-------|
| FAT32 | ✅ | ✅ | ✅ | 5-stage fsck, boot checksum, dual FAT |
| NTFS | ✅ | ✅ | ✅ | Boot sector, MFT, $Bitmap/$LogFile/$UpCase |
| ext4 | ✅ | ✅ | ✅ | Superblock+GDT+journal, GDT CRC16, backup superblocks |
| exFAT | ✅ | ✅ | ✅ | Boot checksum, allocation bitmap, upcase table |
| swap | 📋 | 📋 | 📋 | Planned |
| FS conversion (FAT32↔NTFS etc.) | 📋 | 📋 | 📋 | Planned |

### Disk Operations

| Feature | Status | Notes |
|---------|--------|-------|
| Sector-by-sector disk clone | ✅ | With verification |
| Partition copy | ✅ | Checksum verify |
| Clone with resize | ✅ | `cloneDiskWithResize` |
| Secure erase | ✅ | Zeros, Random, DoD 5220.22-M, Gutmann, NIST 800-88 |
| Benchmark | ✅ | Sequential/random IOPS, latency, MB/s |
| SMART read | 🚧 | `DiskIO::readSMART` exposed; device support varies |

### CLI (opm)

| Command | Status |
|---------|--------|
| `list` / `info` / `read` | ✅ |
| `mklabel <mbr\|gpt>` | ✅ |
| `create` / `delete` / `resize` / `move` | ✅ |
| `format <fat32\|ntfs\|ext4\|exfat>` | ✅ |
| `check` / `fsinfo` | ✅ |
| Progress indicators / scripting | 📋 |

### GUI (Qt, `-DBUILD_GUI=ON`)

| Feature | Status |
|---------|--------|
| Disk tree (real enumeration) | ✅ |
| Create / Delete / Resize / Format / Clone / Secure Erase dialogs | ✅ (wired to core) |
| Benchmark dialog | ✅ (real benchmark) |
| Visual partition map / drag-and-drop | 📋 |
| Wizards (clone, migrate OS, bootable media, recovery) | 📋 |
| Operation queue visualization / undo-redo | 📋 |

### Boot & Recovery

| Feature | Status | Notes |
|---------|--------|-------|
| Live USB creation from ISO | ✅ | Writes + verifies + syncs |
| Verify live USB | ✅ | Boot sig + ISO9660 check |
| MBR boot-signature repair | ✅ | Backup first, honest failure when boot code empty |
| GPT restore from backup | ✅ | CRC-verified recovery |
| Bootloader install (syslinux/GRUB2) | 🚧 | Honest error: stage files not bundled; use distro installer |
| ISO mount / unmount | ✅ | Real `mount(2)`/`umount(2)` on Linux |
| ISO extraction (ISO9660) | ✅ | PVD + directory walk, recursive, progress |
| ISO creation | ✅ | via xorriso/genisoimage/mkisofs |
| USB device detection | ✅ | sysfs scan, vendor/model/size/FS/label |
| Password reset (Windows/Linux) | 📋 | Planned |
| Data recovery (undelete, PhotoRec) | 📋 | Planned |
| Partition recovery / rebuild | 📋 | Planned |

### Encryption (planned, not yet implemented)

| Feature | Status |
|---------|--------|
| BitLocker detect / unlock / resize | 📋 |
| LUKS detect / open / close | 📋 |
| VeraCrypt detect / mount | 📋 |

### Enterprise (planned, not yet implemented)

| Feature | Status |
|---------|--------|
| Dynamic disks (Windows) | 📋 |
| LVM (Linux) | 📋 |
| RAID detection (mdadm/hardware) | 📋 |
| Windows Storage Spaces | 📋 |

### Conversion (planned, not yet implemented)

| Feature | Status |
|---------|--------|
| MBR → GPT | 📋 |
| GPT → MBR | 📋 |
| 4K alignment optimization | 📋 (alignment checks exist) |

---

## Usage Rights

**Personal Use**: ✅ Unlimited machines, all features
**Commercial Use**: ✅ Business deployment, all features
**Educational Use**: ✅ Schools and universities, all features
**Government Use**: ✅ Public sector, all features
**Service Providers**: ✅ Client computers, all features

**NO RESTRICTIONS**: No machine limits, no user limits, no deployment limits.

---

## License: GPL-3.0

This ensures the software stays open source forever and cannot be made proprietary.

---

## Roadmap

See `project-docs/open-partition-manager-roadmap.md` for the detailed phase
breakdown. Unimplemented features above are tracked there; contributions are
welcome on any of them.

*Last updated: August 2026 — reflects actual code state, not aspirations.*
