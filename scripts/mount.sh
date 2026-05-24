#!/usr/bin/env bash
set -euo pipefail

DISK=${1:?Usage: scripts/mount.sh /dev/loopX [mountpoint]}
MNT=${2:-/mnt}

sudo mkdir -p "$MNT"
sudo mount -t myfs "$DISK" "$MNT"
echo "Mounted $DISK to $MNT"
