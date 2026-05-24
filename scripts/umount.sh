#!/usr/bin/env bash
set -euo pipefail

MNT=${1:-/mnt}

sudo umount "$MNT"
sudo rmmod myfs || true
echo "Unmounted $MNT and unloaded myfs if it was loaded"
