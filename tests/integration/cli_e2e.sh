#!/usr/bin/env bash
# End-to-end CLI test on a virtual disk image (no root needed).
# Exercises: mklabel, create, format, check, fsinfo, resize, delete for
# MBR + GPT and all four filesystems.
set -u

OPM="${1:-opm}"
IMG="$(mktemp /tmp/opm-e2e-XXXXXX.img)"
trap 'rm -f "$IMG"' EXIT
fail=0

check() {
    local desc="$1"; shift
    local out
    out="$("$@" 2>&1)"
    local rc=$?
    if [ $rc -ne 0 ]; then
        echo "FAIL [$desc]: rc=$rc"
        echo "$out" | head -5
        fail=1
    else
        echo "ok [$desc]"
    fi
}

expect_fail() {
    local desc="$1"; shift
    if "$@" >/dev/null 2>&1; then
        echo "FAIL [$desc]: expected error but succeeded"
        fail=1
    else
        echo "ok [$desc]"
    fi
}

truncate -s 256M "$IMG"

# --- GPT flow ---
check "gpt mklabel"       "$OPM" mklabel "$IMG" gpt
check "gpt create p1"     "$OPM" create "$IMG" 2048 60M linux p1
check "gpt create p2"     "$OPM" create "$IMG" 124928 60M linux p2
check "gpt create p3"     "$OPM" create "$IMG" 247808 40M swap p3
check "gpt format fat32"  "$OPM" format "$IMG" fat32 2048 60M MEDIA
check "gpt check fat32"   "$OPM" check "$IMG" 2048
check "gpt format ntfs"   "$OPM" format "$IMG" ntfs 124928 60M WIN
check "gpt check ntfs"    "$OPM" check "$IMG" 124928
check "gpt format ext4"   "$OPM" format "$IMG" ext4 247808 40M DATA
check "gpt check ext4"    "$OPM" check "$IMG" 247808
check "gpt fsinfo ext4"   "$OPM" fsinfo "$IMG" 247808
check "gpt resize p3"     "$OPM" resize "$IMG" 3 20M
check "gpt delete p2"     "$OPM" delete "$IMG" 2

# --- MBR flow ---
check "mbr mklabel"       "$OPM" mklabel "$IMG" mbr
check "mbr create p1"     "$OPM" create "$IMG" 2048 50M linux p1
check "mbr create p2"     "$OPM" create "$IMG" 104448 50M fat32 p2
expect_fail "overlap create" "$OPM" create "$IMG" 2048 60M linux
check "mbr format ext4"   "$OPM" format "$IMG" ext4 2048 50M MBRDATA
check "mbr check ext4"    "$OPM" check "$IMG" 2048
check "mbr move p2"       "$OPM" move "$IMG" 2 307200
check "mbr delete p1"     "$OPM" delete "$IMG" 1

# --- negative cases ---
expect_fail "bad size suffix"  "$OPM" create "$IMG" 2048 10Z linux
expect_fail "unknown fs"       "$OPM" format "$IMG" btrfs 2048 10M
expect_fail "fs on empty"      "$OPM" check "$IMG" 400000

# --- filesystem conversion (FAT32 -> NTFS, data-preserving) ---
check "mklabel gpt (convert)"  "$OPM" mklabel "$IMG" gpt
check "create conv p1"         "$OPM" create "$IMG" 2048 60M fat32 p1
check "format conv fat32"      "$OPM" format "$IMG" fat32 2048 60M CONVLABEL
check "convert-fs p1 -> ntfs"  "$OPM" convert-fs "$IMG" 1 ntfs
check "fsinfo after convert"   sh -c "\"$OPM\" fsinfo \"$IMG\" 2048 | grep -q NTFS"
check "check ntfs after conv"  "$OPM" check "$IMG" 2048
expect_fail "convert again"    "$OPM" convert-fs "$IMG" 1 ntfs
expect_fail "convert to ext4"  "$OPM" convert-fs "$IMG" 1 ext4

# --- JSON output mode ---
BKDIR="$(mktemp -d /tmp/opm-e2e-bk-XXXXXX)"
check "json list" sh -c "\"$OPM\" list --json | python3 -c 'import json,sys; json.load(sys.stdin)'"
check "json info" sh -c "\"$OPM\" info \"$IMG\" --json | python3 -c 'import json,sys; d=json.load(sys.stdin); assert \"size\" in d'"
check "json backup info" sh -c "\"$OPM\" backup create \"$IMG\" \"$BKDIR/bak.img\" --block-size 64M >/dev/null 2>&1 && \"$OPM\" backup info \"$BKDIR/bak.img\" --json | python3 -c 'import json,sys; d=json.load(sys.stdin); assert d[\"mode\"]==\"full\"'"
check "json backup list" sh -c "\"$OPM\" backup list \"$BKDIR\" --json | python3 -c 'import json,sys; d=json.load(sys.stdin); assert d[\"count\"]==1'"
rm -rf "$BKDIR"

if [ "$fail" -eq 0 ]; then
    echo "ALL E2E CLI TESTS PASSED"
else
    echo "E2E CLI TESTS FAILED"
    exit 1
fi
