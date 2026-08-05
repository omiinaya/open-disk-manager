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
| **Merge adjacent partitions** | ✅ | Empty-right table grow (any FS) + FAT32→FAT32 data move (`opm merge`) |
| mklabel (MBR / GPT) | ✅ | Wipes stale GPT when labeling MBR |
| APM / BSD disklabel | 📋 | Planned |

### Filesystems

| Filesystem | Format | Check | Resize | Notes |
|-----------|--------|-------|--------|-------|
| FAT32 | ✅ | ✅ | ✅ | 5-stage fsck, boot checksum, dual FAT |
| NTFS | ✅ | ✅ | ✅ | Boot sector, MFT, $Bitmap/$LogFile/$UpCase |
| ext4 | ✅ | ✅ | ✅ | Superblock+GDT+journal, GDT CRC16, backup superblocks |
| exFAT | ✅ | ✅ | ✅ | Boot checksum, allocation bitmap, upcase table |
| swap | ✅ | ✅ | — | SWAPSPACE2 v1, detection fixed (spans 4K page) |
| FS conversion (FAT32→NTFS) | ✅ | `opm convert-fs <dev> <n> ntfs` — data-preserving, never moves file contents |

### Disk Operations

| Feature | Status | Notes |
|---------|--------|-------|
| Sector-by-sector disk clone | ✅ | With verification |
| Partition copy | ✅ | Checksum verify |
| Clone with resize | ✅ | `cloneDiskWithResize` |
| Secure erase | ✅ | **12 standards**: Zeros, Random, DoD 5220.22-M, DoD ECE, Gutmann, NIST Clear, NIST Purge, RCMP TSSIT, VSITR, GOST P50739, US Army AR380, ATA-erase |
| SSD TRIM | ✅ | `opm trim` — BLKDISCARD, honest error on non-block |
| Benchmark | ✅ | Sequential/random IOPS, latency, MB/s |
| SMART read | ✅ | `opm smart` — ATA HDIO identity + **NVMe Get Log Page 0x02 SMART/Health** (structured parse: temp, spare, % used, power cycles/hours, unsafe shutdowns, media errors) |

### CLI (opm)

| Command | Status |
|---------|--------|
| `list` / `info` / `read` | ✅ |
| `mklabel <mbr\|gpt>` | ✅ |
| `create` / `delete` / `resize` / `move` | ✅ (all with `--dry-run`) |
| `merge <dev> <numA> <numB>` | ✅ | Empty-right grow + FAT32→FAT32 data move |
| `convert <mbr\|gpt>` | ✅ | `set-active`, `hide`, `unhide` | ✅ |
| **`convert-fs <dev> <n> <ntfs>`** | ✅ | **FAT32 → NTFS, data-preserving** (file data never moved; metadata rebuilt into freed FAT region) |
| `format <fat32\|ntfs\|ext4\|exfat\|swap>` | ✅ |
| `check` / `fsinfo` / `label` | ✅ |
| `recover [--rebuild]` | ✅ | `undelete [--restore]` | ✅ |
| `align [--fix]` / `cryptinfo` / `luks` / `bitlocker` | ✅ |
| `boot-repair --uefi` / `reset-password --linux\|--windows` | ✅ |
| `lvm` / `raid` | ✅ | `i18n` | ✅ |
| `wipe [--method]` / `trim` | ✅ | 12 erase standards + BLKDISCARD |
| **`smart`** | ✅ | **NVMe SMART/Health log (Get Log Page 0x02) + ATA HDIO identity** |
| **`backup create/incremental/differential/restore/info/verify`** | ✅ | Image backup (OPMIMG) + `--compress` |
| **`backup files/listfiles/extract`** | ✅ | File-level backup (ustar tar, hard links + device nodes) |
| **`backup schedule add/list/remove/show/run`** | ✅ | Cron + systemd-timer generation |
| **`backup list` / `backup prune`** | ✅ | Backup-set listing + retention (`--keep-full`, `--older-than`) |
| Progress indicators / scripting | ✅ | embedded progress; `--json` on list/info/backup info/backup list |

### GUI (Qt, `-DBUILD_GUI=ON`)

| Feature | Status |
|---------|--------|
| Disk tree (real enumeration) | ✅ |
| Create / Delete / Resize / Format / Clone / Secure Erase dialogs | ✅ (wired to core) |
| Benchmark dialog | ✅ (real benchmark) |
| Visual partition map (click select, double-click properties) | ✅ |
| **Operation log dock** | ✅ | **Dark/light theme** (persisted) | ✅ |
| **Status bar progress** for long operations | ✅ |
| **Wizards (clone, migrate OS, bootable media, recovery)** | ✅ |
| **Windows GUI build** | ✅ | `opm-gui.exe` via MinGW + Qt 6.9.3; windeployqt bundle in CI (`windows-gui-packaging` job) |
| Drag-and-drop resize/move | 📋 | (dialog-driven today) |
| Operation queue visualization / undo-redo | 📋 | (core queue exists; GUI panel planned) |

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
| Password reset (Windows via chntpw / Linux native shadow) | ✅ |
| Undelete files (FAT + NTFS) | ✅ | (`opm undelete` — FAT32 0xE5 entries; NTFS MFT scan + `--restore`/`--export`) |
| Partition recovery / rebuild | ✅ | (`opm recover` — signature scan + MBR rebuild) |

### Encryption

| Feature | Status |
|---------|--------|
| BitLocker detect | ✅ | LUKS detect (v1+v2) | ✅ |
| BitLocker unlock (password/recovery, dislocker) | ✅ |
| LUKS open / close / status (cryptsetup) | ✅ |
| VeraCrypt detect / mount | 📋 |

### Enterprise

| Feature | Status |
|---------|--------|
| Dynamic disks (Windows) | 📋 |
| LVM (Linux) | ✅ | (`opm lvm` — PV/VG/LV detection) |
| RAID detection (mdadm) | ✅ | (`opm raid` — /proc/mdstat + superblock scan) |
| Windows Storage Spaces | 📋 |

### Conversion

| Feature | Status |
|---------|--------|
| MBR → GPT | ✅ | (`opm convert ... gpt`) |
| GPT → MBR | ✅ | (`opm convert ... mbr`, >4/2TiB refused) |
| 4K alignment **optimization** | ✅ | (`opm align --fix`, data-preserving) |

---

## Backup & Restore

| Feature | Status | Notes |
|---------|--------|-------|
| **Image backup** (block-level) | ✅ | `opm backup create` — OPMIMG format, SHA-256 per block, atomic commit |
| **Incremental** | ✅ | `opm backup incremental` — stores only changed blocks vs. base |
| **Differential** | ✅ | `opm backup differential` — changed blocks vs. a full base |
| **Restore** | ✅ | `opm backup restore` — full + incremental apply on top of base |
| **Verify** | ✅ | `opm backup verify` — re-hash stored blocks, detect corruption |
| **Compression** | ✅ | `opm backup create ... --compress` — sparse ZERO + RLE per-block, no external deps |
| **Retention** | ✅ | `opm backup list/prune` — keep N fulls or drop by age; chain-safe |
| **File-level backup** | ✅ | `opm backup files` — self-contained ustar tar (GNU-tar interop verified) |
| **Hard links / device nodes** | ✅ | `backup files` preserves hard links (dedup) + char/block nodes (mknod on restore) |
| **File extract** | ✅ | `opm backup extract` — path-traversal guard |
| **Scheduling** | ✅ | `opm backup schedule` — registry + cron line + systemd user timer |
| **Schedule GUI panel** | ✅ | Tools → Backup Schedules (view/add/remove; same registry as CLI) |
| **Windows PE recovery media** | 📋 | WinPE is Windows-only; `opm bootable` covers Linux live USB |

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
