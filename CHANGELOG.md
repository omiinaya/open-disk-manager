# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.4.2] - 2026-08-04

NVMe SMART/Health support (real gap: SMART was ATA-only and not even exposed in the CLI).

### Added
- **`opm smart <device>`** — read SMART data:
  - NVMe devices (`nvme*`): NVMe Get Log Page admin command (opcode 0x02),
    Log Page ID 0x02 SMART/Health Information (the 512-byte log every NVMe
    device must support). Structured parse (`NvmeSmartInfo`):
    critical-warning bits, temperature (K + C), available spare + threshold,
    percentage used, data units read/written, host read/write commands,
    controller busy time, power cycles, power-on hours, unsafe shutdowns,
    media/integrity errors, error-log entries, warning/critical temp time.
  - ATA devices: `HDIO_GET_IDENTITY` with model/serial/firmware decode.
- `DiskIO::readNvmeSMART()` — Linux ioctl path (`NVME_IOCTL_ADMIN_CMD`,
  auto-adapts to the kernel's `nvme_admin_cmd` layout via `nvme_passthru_cmd`).

### Tests
- New `tests/test_smart.cpp` (5 tests): full-log field decode, all warning
  bits + counters, short-buffer rejection, zero-temperature handling, and
  render-line spot checks. 275 total unit tests.

## [0.4.1] - 2026-08-04

NTFS file-undelete (MFT scan + in-place restore + data export).

### Added
- **NTFS undelete**: `opm undelete <device> <start>` now works on NTFS volumes in
  addition to FAT32. It scans the MFT for records whose IN_USE flag is clear and
  extracts `$FILE_NAME` (name, size, parent) + the non-resident `$DATA` run list:
  - `opm undelete <dev> <start>` — list deleted files (live records are excluded),
  - `opm undelete <dev> <start> --restore <i>` — in-place restore: verifies all the
    file's clusters are still free in `$Bitmap` (refuses if the data was overwritten),
    marks them used, sets IN_USE back, and re-inserts the `$FILE_NAME` index entry
    into the parent directory's `$INDEX_ROOT` (resident index), then keeps the volume
    fsck-clean,
  - `opm undelete <dev> <start> --export <dir>` — writes the reconstructed cluster
    runs out to a host directory (`<record>_<name>`); the reliable recovery path for
    any volume, including large/non-resident parent indexes.

### Fixed
- (none — new feature only)

### Tests
- New `tests/test_ntfs_undelete.cpp` (3 tests): scan finds a deleted record while live
  ones are excluded + export recovers data verbatim; in-place restore re-links the
  record and the volume still passes `checkNTFS`; refuses restore when the cluster
  data was overwritten (reformatted volume no longer reports the record).
- E2E CLI: `undelete ntfs scan` block added (post-conversion NTFS volume reports 0
  deleted files). 270 total unit tests.

## [0.4.0] - 2026-08-04

On-disk filesystem conversion + a real NTFS writer + a run-list-aware NTFS fsck.

### Added
- **Data-preserving FAT32 → NTFS conversion**: `opm convert-fs <device> <partition> ntfs`
  (and `convertFAT32ToNTFS` / `convertPartitionToNTFS` in the core library). The NTFS
  cluster size is chosen to match the FAT32's, so every FAT data cluster maps 1:1 to an
  NTFS LCN — **file contents are never moved**, only metadata is rewritten into the freed
  reserved+FAT region (falling back to the largest contiguous free run on fuller volumes).
  Spec vault:
  - Real MFT records with `$STANDARD_INFORMATION`, `$FILE_NAME`, non-resident `$DATA`
    run lists that reuse the original cluster chains, and `$INDEX_ROOT` /
    `$INDEX_ALLOCATION` (`INDX` fixup buffers) directory trees.
  - Fixup/update-sequence numbers applied to every 1024-byte MFT record.
  - Correct system files: `$MFT`, `$MFTMirr`, `$LogFile` (RSTR), `$Volume` (label carried
    over from the FAT32 volume label), `$AttrDef`, `$Bitmap`, `$Boot`, `$BadClus`,
    `$Secure`, `$UpCase`, `$Extend`.
  - Never partial: the whole plan validates (free space, alignment, tree build) before
    any sector is written; foreign filesystems are rejected up front instead of being
    misread as a corrupt FAT32.
- **Run-list-aware NTFS fsck (`checkNTFS`)**: `$Bitmap` and `$LogFile` locations are now
  resolved from the MFT records' `$DATA` run lists when present (the real NTFS
  behaviour), falling back to the fixed geometry used by `formatNTFS`. Previously the
  checker could only validate the format's exact layout; it now also validates converted
  volumes and correctly checks that every system file's clusters are marked used in
  `$Bitmap`.

### Fixed
- **GPT header sector overflow (`gpt.cpp`)**: `writePrimaryGPT`/`writeBackupGPT` wrote a
  512-byte sector from a 92-byte packed `GPTHeaderRaw` stack struct, over-reading 420
  bytes of adjacent stack memory. Now zero-pads to a full 512-byte buffer. (Caught by
  ASan during converter testing.)
- **NTFS format `$Bitmap` (incomplete)**: `createBitmap` only marked the first ~10
  clusters used; it did not mark the clusters claimed by `$Bitmap` itself, `$LogFile`,
  `$UpCase`, or the MFT mirror. The checker now requires those, and the format marks
  them all — matching real NTFS allocation semantics.

### Tests
- New `tests/test_convert_fs.cpp` (5 tests): data-preservation check (file bytes
  identical at the same physical offset after conversion), independent MFT-walk
  verification of the resulting `$FILE_NAME`/`$DATA` run lists, label propagation,
  partition-table front end, foreign-FS rejection, and refusal to re-convert.
- E2E CLI: `convert-fs` block added to `cli_e2e.sh` (convert, post-convert fsck,
  re-convert refusal, unsupported-target refusal).

## [0.3.1] - 2026-08-04

Backup polish + Windows GUI packaging.

### Added
- **Backup compression**: `opm backup create/incremental/differential --compress` —
  per-block sparse (ZERO) + RLE encoding, self-contained codec (no zlib dependency,
  MinGW sysroot lacks it). All-zero free-space blocks collapse to a 1-byte marker
  (measured 319× smaller on a sparse 4 MiB image). Backward compatible with
  pre-compression images via a header flags bit.
- **Backup retention**: `opm backup list <dir>` (newest-first listing of a backup
  set) and `opm backup prune <dir> --keep-full N [--older-than DAYS]` — chain-safe
  pruning that keeps the N most recent fulls (and their still-valid incrementals/
  differentials) or drops images by age.
- **ustar hard links**: `opm backup files` now tracks (dev,ino) during the walk; a
  repeated inode is archived as a hard-link entry (data stored once, dedup).
  GNU-tar interop verified (`hrw` entry). Restore recreates the link via `link(2)`
  with a traversal-safe target.
- **ustar device nodes**: char/block nodes archived with major/minor; restored via
  `mknod` (honest error without CAP_MKNOD; test skips when not root).
- **Windows GUI cross-build + packaging**: `cmake/mingw-toolchain.cmake` +
  `packaging/build-windows-bundle.sh` (QT_HOST_PATH = native Qt for host tools,
  CMAKE_PREFIX_PATH = Windows Qt kit; windeployqt under Wine; MinGW runtime DLLs
  included). New CI job `windows-gui-packaging` produces and uploads the bundle.
- Tests: 251 → 258 (compression 2, retention 3, tar hard links 1, device nodes 1).
  Full Windows suite under Wine: 258 run / 251 pass / 7 documented platform skips.

## [0.3.0] - 2026-08-04

Competitor-gap release: the features that commercial partition managers
(Paragon HDM, Acronis) ship that this project previously lacked — a real
backup engine, merge, and expanded wiping — are now implemented, tested, and
documented honestly.

### Added
- **Image backup engine**: `opm backup create/incremental/differential/restore/info/verify`
  — OPMIMG format with per-block SHA-256, block bitmap, atomic commit; incremental
  stores only changed blocks, differential requires a full base.
- **File-level backup**: `opm backup files/listfiles/extract` — self-contained POSIX
  ustar, GNU-tar interop verified both directions, path-traversal guard.
- **Backup scheduling**: `opm backup schedule add|list|remove|show|run` — plain-text
  registry plus cron-line and systemd user-timer generation.
- **Merge partitions**: `opm merge <dev> <numA> <numB>` — adjacent validation; empty-right
  table grow for any filesystem; FAT32→FAT32 data-preserving move.
- **Secure erase v2**: `opm wipe` — 12 standards (Zeros, Random, DoD 5220.22-M, DoD ECE,
  Gutmann, NIST Clear/Purge, RCMP TSSIT, VSITR, GOST P50739, US Army AR380, ATA-erase).
- **SSD TRIM**: `opm trim` — BLKDISCARD; implemented previously-declared-but-missing
  `DiskIO::trim/supportsTRIM/readSMART`.
- Tests: 230 → 251 (backup 5, schedule 4, tar 3, merge 4, wipe 5).

## [0.2.0] - 2026-08-04

The August 2026 audit-fix release: the CLI surface and GPT modification layer
that the docs overclaimed were made real, every P0/P1/P2 audit item was fixed
and verified end-to-end, and the GUI was wired to the real core library.

### Added
- Full CLI command set: `create`, `delete`, `resize`, `move`, `format`,
  `check`, `fsinfo`, `mklabel`, `cryptinfo`, `align`, `lvm`, `raid`, `i18n`
- `--dry-run` mode for create/delete/resize
- Real GPT create/delete/resize/commit with primary + backup sync and CRC recompute
- GPT recover-from-backup (`GPTTable::recover` + `restoreFromBackup`)
- Boot: MBR boot-signature repair, GPT restore-from-backup, ISO9660
  extraction, ISO mount/unmount/create, USB device detection
- Encryption detection (BitLocker `FVE-FS`, LUKS v1/v2) via `opm cryptinfo`
- LVM detection (`opm lvm`) and software RAID detection (`opm raid`)
- 4K/1MiB alignment reporting (`opm align`)
- i18n message-catalog framework + Spanish seed catalog
- `DiskIO` support for regular-file images (root-free testing)
- GitHub Actions CI workflow
- 204 unit tests + root-free CLI end-to-end integration test

### Fixed
- NTFS boot sector struct was 519 bytes (overran the 512-byte sector);
  MFT fields were shifted 7 bytes off spec offsets — restructured to the spec
- NTFS system-file cluster collisions ($LogFile overwrote $Bitmap/$UpCase) —
  shared allocation helpers for format/check agreement
- FAT32 layout `calculate()` produced format/check-divergent cluster counts —
  added a convergence loop; corrected cluster-size selection for <8GB volumes
- FAT32 format never wrote the FSInfo sector — now written and validated
- exFAT filesystem detection checked the wrong boot-sector offset (name is at
  offset 3, not 0); exFAT bitmap check used wrong bit indexing
- ext4 `start_sector * block_size` offset bug across 21 sites (now
  `start_sector * bytes_per_sector`)
- ext4 journal not linked into the superblock (s_journal_inum/s_journal_uuid)
- ext4 GDT checksums zeroed despite GDT_CSUM feature — now computed (CRC16)
- ext4 format left stale filesystem signatures in the boot area — cleared
- MBR `createPartition` had no overlap check; fresh-table OOB on empty disks;
  delete/resize matched by an unset partition number
- `PartitionTable::load` threw on corrupt GPT — now returns nullptr (recoverable)
- ISO9660 extraction infinite-recursed on `.`/`..` records (0x00/0x01 names)
- boot.cpp stubs returned fake `Result::ok()` for unimplemented operations —
  honest errors + real implementations where feasible
- GUI: omitted dialog sources, nonexistent `EraseMethod` enum values, fake
  success popups, fabricated disk tree, mock benchmark numbers — all replaced
  with real core operations and honest reporting

### Removed
- 30,718 tracked `node_modules` / build / `.docusaurus` files from git history

### Changed
- Repo hygiene: `.gitignore` added (and the `core` rule fixed so it no longer
  excludes `src/core/*.cpp`); tracked files 31,186 → 148
- Docs corrected to reflect actual state (FEATURE-AVAILABILITY,
  PROJECT_STATUS_V2, roadmap checkboxes)
- Version bumped from 0.1.0 to 0.2.0

## [0.1.0] - 2026-05-23

Initial alpha release: partition table reading (MBR/GPT), FAT32/ext4/NTFS/exFAT
filesystem operations, disk clone/secure-erase/benchmark engine, CLI
(`list`/`info`/`read`), Qt GUI framework.