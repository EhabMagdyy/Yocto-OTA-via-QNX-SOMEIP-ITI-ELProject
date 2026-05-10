#!/bin/bash
set -e

RPI3_IP="${1:?Usage: ./send-ota.sh <rpi3-ip>}"
DEPLOY_DIR=/home/ehab/Documents/ITI_9Months/Yocto/shared-build/tmp-glibc/deploy/images/raspberrypi3-64
IMAGE=$(readlink -f "$DEPLOY_DIR/core-image-custom-raspberrypi3-64.rootfs.ext4")

[ -f "$IMAGE" ] || { echo "ERROR: Image not found"; exit 1; }

IMAGE_SIZE=$(stat -c%s "$IMAGE")
echo "[1/3] Image: $IMAGE"
echo "      Size:  $IMAGE_SIZE bytes ($(( IMAGE_SIZE / 1024 / 1024 )) MB)"

echo "[2/3] Calculating SHA256..."
SHA256=$(sha256sum "$IMAGE" | cut -d' ' -f1)
echo "      SHA256: $SHA256"

echo "[3/3] Copying image to RPi3 staging partition..."
scp "$IMAGE" root@$RPI3_IP:/mnt/staging/ota_image.ext4

echo "Triggering OTA update..."
ssh root@$RPI3_IP "nohup /usr/bin/ota.sh $SHA256 $IMAGE_SIZE /mnt/staging/ota_image.ext4 > /var/log/ota-update.log 2>&1 &"

echo "Done. Monitor with:"
echo "  ssh root@$RPI3_IP 'tail -f /var/log/ota-update.log'"