---
sidebar_position: 3
---

# Quick Start

## List Devices

```bash
sudo opm list
```

## Create a Partition

```bash
sudo opm create /dev/sdb --size 50G --type ext4
```

## Format a Partition

```bash
sudo opm format /dev/sdb1 --filesystem ext4 --label "MyData"
```

## Resize a Partition

```bash
sudo opm resize /dev/sdb1 --size 100G
```

## Check Filesystem

```bash
sudo opm check /dev/sdb1
```
