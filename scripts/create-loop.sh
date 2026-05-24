#!/usr/bin/env bash
set -euo pipefail

IMAGE=${1:-/tmp/myfs.img}
SIZE_MB=${2:-64}

if [[ -e "$IMAGE" ]]; then
  echo "Image already exists: $IMAGE" >&2
  echo "Remove it manually if you want to recreate it." >&2
  exit 1
fi

dd if=/dev/zero of="$IMAGE" bs=1M count="$SIZE_MB" status=progress
LOOP=$(sudo losetup --find --show "$IMAGE")
echo "$LOOP"
echo "Created image: $IMAGE"
echo "Attached loop device: $LOOP"
