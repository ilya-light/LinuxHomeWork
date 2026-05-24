#!/usr/bin/env bash
set -euo pipefail

DISK=${1:?Usage: scripts/load.sh /dev/loopX [sb1_sector] [sb2_sector] [max_filename_len] [file_size_sectors]}
SB1=${2:-0}
SB2=${3:-128}
MAX_NAME=${4:-32}
FILE_SIZE_SECTORS=${5:-4}

sudo insmod kernel/myfs.ko \
  disk_name="$DISK" \
  sb1_sector="$SB1" \
  sb2_sector="$SB2" \
  max_filename_len="$MAX_NAME" \
  file_size_sectors="$FILE_SIZE_SECTORS"

echo "myfs module loaded for $DISK"
