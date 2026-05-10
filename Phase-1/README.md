# OTA Phase 1 — Yocto RPi3 A/B Slot Switching

## Overview

Phase 1 goal: Host → RPi3 (Yocto) via SCP → write rootfs to inactive partition → reboot into it.

---

## Partition Layout

3-partition layout on the SD card:

| Partition | Label | Size | Purpose |
|---|---|---|---|
| mmcblk0p1 | boot | 300MB | FAT32 boot files + U-Boot env |
| mmcblk0p2 | rootfs_a | 3GB | Active rootfs (slot A) |
| mmcblk0p3 | rootfs_b | 3GB | Inactive rootfs (slot B) |

---

## Yocto Configuration Changes

### `local.conf`
```bitbake
SERIAL_CONSOLES = "115200;ttyAMA0"
RPI_USE_U_BOOT = "1"
BOOT_DELAY = "2"
RPI_EXTRA_CONFIG = "core_freq=250"
```

### `core-image-custom.bb`
```bitbake
# Add fw-utils and set image types
IMAGE_INSTALL:append = " libubootenv-bin"
PREFERRED_PROVIDER_u-boot-fw-utils = "libubootenv"

# Increase rootfs size (100MB default is too small)
IMAGE_ROOTFS_SIZE ?= "3145728"   # 3GB in KB
```

### `.wks` file — `meta-customimage/wic/rpi3-ab.wks`
```
part /boot --source bootimg-partition --ondisk mmcblk0 --fstype=vfat --label boot --active --align 4 --size 300
part /     --source rootfs --ondisk mmcblk0 --fstype=ext4 --label rootfs_a --align 4 --size 3000
part --ondisk mmcblk0 --fstype=ext4 --label rootfs_b --align 4 --size 3000
```

Set in `core-image-custom.bb`:
```bitbake
WKS_FILE = "rpi3-ab.wks"
```

---

## Build Commands

```bash
cd ~/Documents/ITI_9Months/Yocto

# Source the build environment
source poky/oe-init-build-env buid-rpi

# Build the image
bitbake core-image-custom

# Build U-Boot only (if needed)
bitbake u-boot

# Build the boot script only (if needed)
bitbake rpi-u-boot-scr
```

---

## Flash Commands

```bash
# Check where is your SD card
lsblk

cd ~/Documents/ITI_9Months/Yocto/shared-build/tmp-glibc/deploy/images/raspberrypi3-64/

# Unmount SD card partitions
sudo umount /dev/sdb1 /dev/sdb2 /dev/sdb3 2>/dev/null

# Wipe the disk
sudo wipefs -a /dev/sdb
sudo dd if=/dev/zero of=/dev/sdb bs=1M count=10 conv=fsync

# Flash the full WIC image (creates all 3 partitions)
sudo bmaptool copy core-image-custom-raspberrypi3-64.rootfs.wic.bz2 /dev/sdb

# Eject & remove the SD Card
sudo eject /dev/sdb
```

## in `config.txt`
### Leave space around `cpu_freq=250` it wan't working without it i guess!!
```

core_freq=250

```

---

## U-Boot First Boot Setup (Serial Console)

Connect USB-TTL adapter to RPi3 GPIO:
- TX → Pin 8 (GPIO14)
- RX → Pin 10 (GPIO15)
- GND → Pin 6

Open serial terminal on host:
```bash
sudo picocom -D /dev/ttyUSB0 -b 115200
```

On boot, **hit any key** to stop autoboot at the `=>` prompt, then:
```
U-Boot> setenv active_slot a
U-Boot> saveenv
U-Boot> printenv active_slot
U-Boot> boot
```

Expected output: `Saving Environment to FAT... OK`

---

## Verify from Linux (RPi3)

```bash
# Read the active slot variable
fw_printenv active_slot
# Expected: active_slot=a

# Check which partition is mounted as root
mount | grep mmcblk0
# Expected: /dev/mmcblk0p2 on / ...
```

---

## A/B Boot Script (`boot.scr`)

The default `boot.scr` from `meta-raspberrypi` has no A/B logic. Replace it with a custom one that reads `active_slot` and sets the correct root partition.

### Compile and deploy immediately (no rebuild needed)

On the host:
```bash
# Create you boot script, compile it & put it in the boot partition
~/Documents/ITI_9Months/EmbeddedLinux/u-boot/tools/mkimage -C none -A arm64 -T script -d boot.cmd boot.scr
sudo cp boot.scr /media/ehab/boot/boot.scr
sudo sync
```

### Make it permanent in Yocto

Create `meta-customimage/recipes-bsp/rpi-u-boot-scr/files/boot.cmd` with the content above, then:

```bitbake
# meta-customimage/recipes-bsp/rpi-u-boot-scr/rpi-u-boot-scr_%.bbappend
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
RPI_U_BOOT_SCR = "boot.cmd"
```

---

## Test Slot Switch

```bash
# On RPi3 — switch to slot B and reboot
fw_setenv active_slot b
reboot

# After reboot, verify
fw_printenv active_slot     # → active_slot=b
mount | grep mmcblk0        # → /dev/mmcblk0p3 on / ...
```

---


