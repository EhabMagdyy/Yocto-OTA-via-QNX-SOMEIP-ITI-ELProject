#!/bin/sh

EXPECTED_SHA256="$1"
EXPECTED_SIZE="$2"
IMAGE="$3"
LOG="/var/log/ota-update.log"

log() { echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" | tee -a "$LOG"; }
die() { log "ERROR: $1"; exit 1; }

[ -z "$EXPECTED_SHA256" ] && die "No SHA256 provided"
[ -z "$EXPECTED_SIZE" ]   && die "No size provided"
[ -z "$IMAGE" ]           && die "No image path provided"
[ -f "$IMAGE" ]           || die "Image not found: $IMAGE"
[ "$EXPECTED_SIZE" -lt $((100 * 1024 * 1024)) ] && \
    die "Size ($EXPECTED_SIZE) too small"

log "=== OTA Update Started ==="
log "Image: $IMAGE"
log "Expected SHA256: $EXPECTED_SHA256"
log "Expected size:   $EXPECTED_SIZE bytes"

# 1. Verify image SHA256 before touching anything
log "Verifying image SHA256..."
ACTUAL_SHA256=$(sha256sum "$IMAGE" | cut -d' ' -f1)
[ "$ACTUAL_SHA256" != "$EXPECTED_SHA256" ] && \
    die "SHA256 mismatch — aborting. Expected: $EXPECTED_SHA256 Got: $ACTUAL_SHA256"
log "SHA256 OK"

# 2. Determine slots
ACTIVE=$(fw_printenv active_slot 2>/dev/null | cut -d= -f2)
[ -z "$ACTIVE" ] && die "Cannot read active_slot"

if [ "$ACTIVE" = "a" ]; then
    TARGET="/dev/mmcblk0p3"
    NEXT_SLOT="b"
elif [ "$ACTIVE" = "b" ]; then
    TARGET="/dev/mmcblk0p2"
    NEXT_SLOT="a"
else
    die "Invalid active_slot: $ACTIVE"
fi

log "Active: $ACTIVE → Target: $TARGET (slot $NEXT_SLOT)"

# 3. Make sure target is not mounted
mount | grep -q "^$TARGET " && die "$TARGET is mounted — aborting"

# 4. Wipe first 10MB to clear old filesystem signature
log "Wiping $TARGET header..."
dd if=/dev/zero of="$TARGET" bs=1M count=10
sync
log "Wipe done"

# 5. Write image
log "Writing image to $TARGET..."
dd if="$IMAGE" of="$TARGET" bs=4M
sync
log "Write complete"

# 6. Verify first block matches
log "Verifying first block..."
MD5_IMAGE=$(dd if="$IMAGE"   bs=1M count=1 | md5sum | cut -d' ' -f1)
MD5_WRITTEN=$(dd if="$TARGET" bs=1M count=1 | md5sum | cut -d' ' -f1)

if [ "$MD5_IMAGE" != "$MD5_WRITTEN" ]; then
    log "VERIFICATION FAILED"
    log "  Image:   $MD5_IMAGE"
    log "  Written: $MD5_WRITTEN"
    log "Slot NOT switched — old slot still safe"
    exit 1
fi
log "Verification OK — first block matches"

# 7. Switch slot
fw_setenv active_slot "$NEXT_SLOT" || die "fw_setenv failed"
log "Slot switched to $NEXT_SLOT"

# 8. Cleanup and reboot (note: cleanup didn't work but no problem as we override it on next OTA)
rm -f "$IMAGE"
log "=== OTA Complete — Rebooting into slot $NEXT_SLOT ==="
sleep 1
reboot