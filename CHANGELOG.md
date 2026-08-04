# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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