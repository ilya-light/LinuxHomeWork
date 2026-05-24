#!/usr/bin/env bash
set -euo pipefail

DISK=${1:?Usage: scripts/test.sh /dev/loopX [mountpoint]}
MNT=${2:-/mnt}

make -C userspace
sudo mkdir -p "$MNT"
sudo mount -t myfs "$DISK" "$MNT"
ls -la "$MNT" | head
sudo userspace/myfsctl test "$MNT"
sudo userspace/myfsctl mapping "$MNT" file_000000
sudo userspace/myfsctl hashes "$MNT" | head
sudo userspace/myfsctl zero "$MNT"
sudo userspace/myfsctl test "$MNT"
sudo umount "$MNT"
