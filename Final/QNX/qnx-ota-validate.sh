#!/bin/bash
set -e

MANIFEST="/tmp/ota_manifest.txt"
IMAGE="/tmp/ota_image.ext4"
LOG="/var/log/qnx-ota.log"

log() { echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" | tee -a "$LOG"; }
die() { log "ERROR: $1"; exit 1; }

mkdir -p /var/log
log "=== QNX OTA Validation Started ==="

[ -f "$MANIFEST" ] || die "Manifest not found"
[ -f "$IMAGE" ]    || die "Image not found"

UUID=$(grep   "^uuid="   "$MANIFEST" | cut -d= -f2)
SHA256=$(grep "^sha256=" "$MANIFEST" | cut -d= -f2)
SIZE=$(grep   "^size="   "$MANIFEST" | cut -d= -f2)

[ -z "$UUID" ]   && die "UUID missing"
[ -z "$SHA256" ] && die "SHA256 missing"
[ -z "$SIZE" ]   && die "Size missing"

# UUID format check
echo "$UUID" | grep -qE \
    '^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$' || \
    die "Invalid UUID format"
log "UUID format OK"

# Replay check
USED="/var/log/qnx-ota-used-uuids.txt"
touch "$USED"
grep -qx "$UUID" "$USED" && die "UUID already used - replay attack rejected"
log "UUID is fresh"

# Size check
ACTUAL_SIZE=$(stat -c%s "$IMAGE")
[ "$ACTUAL_SIZE" != "$SIZE" ] && die "Size mismatch"
log "Size OK"

# SHA256 check
log "Verifying SHA256..."
ACTUAL_SHA256=$(sha256sum "$IMAGE" | cut -d' ' -f1)
[ "$ACTUAL_SHA256" != "$SHA256" ] && die "SHA256 mismatch"
log "SHA256 OK"

# Record UUID
echo "$UUID" >> "$USED"
log "UUID recorded"

log "=== Validation complete - Launching qnxService ==="

# Pass sha256 and size to qnxService - UUID not needed anymore
/system/capi-ota/runQnx.sh "$SHA256" "$SIZE"
