#!/bin/bash
set -e

QNX_IP="${1:?Usage: ./send-ota.sh <qnx-rpi4-ip>}"

DEPLOY_DIR=/home/ehab/Documents/ITI_9Months/Yocto/shared-build/tmp-glibc/deploy/images/raspberrypi3-64
IMAGE=$(readlink -f "$DEPLOY_DIR/core-image-custom-raspberrypi3-64.rootfs.ext4")

[ -f "$IMAGE" ] || { echo "ERROR: Image not found"; exit 1; }

IMAGE_SIZE=$(stat -c%s "$IMAGE")

echo "[1/4] Generating OTA UUID..."
OTA_UUID=$(uuidgen)
echo "      UUID: $OTA_UUID"

echo "[2/4] Calculating SHA256..."
SHA256=$(sha256sum "$IMAGE" | cut -d' ' -f1)
echo "      SHA256: $SHA256"

echo "[3/4] Image Information"
echo "      Image:  $IMAGE"
echo "      Size:   $IMAGE_SIZE bytes ($(( IMAGE_SIZE / 1024 / 1024 )) MB)"

echo "[4/4] Copying image + manifest to QNX RPi4..."

# Create manifest
MANIFEST=$(mktemp)
cat > "$MANIFEST" << EOF
uuid=$OTA_UUID
sha256=$SHA256
size=$IMAGE_SIZE
timestamp=$(date -u +%Y-%m-%dT%H:%M:%SZ)
image=ota_image.ext4
EOF

SSH_KEY="/home/ehab/.ssh/id_ed25519"

SSH_OPTS="-o StrictHostKeyChecking=no \
          -o PasswordAuthentication=no \
          -o PreferredAuthentications=publickey \
          -o IdentitiesOnly=yes \
          -o ConnectTimeout=10 \
          -o LogLevel=ERROR \
          -i $SSH_KEY"

# Small file — quiet scp
scp $SSH_OPTS "$MANIFEST" root@$QNX_IP:/tmp/ota_manifest.txt

# Large file — pv sends clean percentages to stderr
pv -n "$IMAGE" | ssh $SSH_OPTS root@$QNX_IP "cat > /tmp/ota_image.ext4"

ssh $SSH_OPTS root@$QNX_IP "/usr/bin/qnx-ota-validate.sh"

rm -f "$MANIFEST"

echo
echo "======================================"
echo "OTA SENT TO QNX"
echo "======================================"
echo "UUID:    $OTA_UUID"
echo "SHA256:  $SHA256"
echo "SIZE:    $IMAGE_SIZE bytes"
echo "TARGET:  $QNX_IP"
echo "======================================"
echo "Monitor QNX validation:"
echo "  ssh root@$QNX_IP 'tail -f /var/log/qnx-ota.log'"