#!/usr/bin/env bash
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
sudo userspace/myfsctl erase "$MNT"
sudo umount "$MNT"
sudo mount -t myfs "$DISK" "$MNT"
sudo userspace/myfsctl test "$MNT"
echo "erase успешно отработал"

sudo umount "$MNT"


