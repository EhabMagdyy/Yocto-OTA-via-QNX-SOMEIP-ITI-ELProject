# OTA Phase 1 — Yocto RPi3 A/B Slot Switching

## Overview

Phase 1 goal: Host → RPi3 (Yocto) via SCP → write rootfs to inactive partition → reboot into it.

---

## Partition Layout

4-partition layout on the SD card:

| Partition | Label | Size | Purpose |
|---|---|---|---|
| mmcblk0p1 | boot | 300MB | FAT32 boot files + U-Boot env |
| mmcblk0p2 | rootfs_a | 2GB | Active rootfs (slot A) |
| mmcblk0p3 | rootfs_b | 2GB | Inactive rootfs (slot B) |
| mmcblk0p4 | staging | 2.5GB | Temporary storage for OTA image |

---

## Yocto Configuration Changes

### `local.conf`
```bitbake
SERIAL_CONSOLES = "115200;ttyAMA0"
RPI_USE_U_BOOT = "1"
BOOT_DELAY = "2"
# both will not be added, add manually
RPI_EXTRA_CONFIG = "dtoverlay=miniuart-bt core_freq=250"
```

### `core-image-custom.bb`
```bitbake
# Add fw-utils and u-boot (u-boot package installs /etc/fw_env.config automatically)
IMAGE_INSTALL:append = " libubootenv-bin u-boot"

# minimum size of rootfs (in KB) => 1GB
IMAGE_ROOTFS_SIZE ?= "1048576"
```

### `.wks` file — `meta-customimage/wic/rpi3-ab.wks`
```
part /boot --source bootimg-partition --ondisk mmcblk0 --fstype=vfat --label boot --active --align 4 --size 300
part /     --source rootfs --ondisk mmcblk0 --fstype=ext4 --label rootfs_a --align 4 --size 2000
part --ondisk mmcblk0 --fstype=ext4 --label rootfs_b --align 4 --size 2000
part --ondisk mmcblk0 --fstype=ext4 --label staging  --align 4 --size 2500
```

Set in `core-image-custom.bb`:
```bitbake
WKS_FILE = "rpi3-ab.wks"
```

### Mount staging partition permanently — `meta-customimage/recipes-core/base-files/base-files_%.bbappend`
```bitbake
do_install:append() {
    echo "/dev/mmcblk0p4  /mnt/staging  ext4  defaults  0  2" \
        >> ${D}${sysconfdir}/fstab
    mkdir -p ${D}/mnt/staging
}
```

---

## Build Commands

```bash
cd ~/Documents/ITI_9Months/Yocto
# Source the build environment
source poky/oe-init-build-env build-rpi
# Build the image
bitbake core-image-custom -k
```

---

## Flash Commands

```bash
# Check where is your SD card
lsblk

cd ~/Documents/ITI_9Months/Yocto/shared-build/tmp-glibc/deploy/images/raspberrypi3-64/
# Unmount SD card partitions
sudo umount /dev/sdb1 /dev/sdb2 /dev/sdb3 /dev/sdb4 2>/dev/null
# Wipe the disk
sudo wipefs -a /dev/sdb
sudo dd if=/dev/zero of=/dev/sdb bs=1M count=10 conv=fsync
# Flash the full WIC image (creates all 4 partitions)
sudo bmaptool copy core-image-custom-raspberrypi3-64.rootfs.wic.bz2 /dev/sdb
# Eject & remove the SD Card
sudo eject /dev/sdb
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

# Check staging is mounted
df -h | grep staging
# Expected: /dev/mmcblk0p4 ... /mnt/staging
```

---

## A/B Boot Script (`boot.scr`)

The default `boot.scr` from `meta-raspberrypi` has no A/B logic. Replace it with a custom one that reads `active_slot` and sets the correct root partition.

### `boot.cmd` content
```bash
fdt addr ${fdt_addr} && fdt get value bootargs /chosen bootargs

if test "${active_slot}" = "a"; then
    setenv rootpart 2
else
    setenv rootpart 3
fi

setenv bootargs "${bootargs} dwc_otg.lpm_enable=0 root=/dev/mmcblk0p${rootpart} rootfstype=ext4 rootwait net.ifnames=0 console=ttyAMA0,115200"

fatload mmc 0:1 ${kernel_addr_r} Image
if test ! -e mmc 0:1 uboot.env; then saveenv; fi
booti ${kernel_addr_r} - ${fdt_addr}
```

### Compile and deploy immediately (no rebuild needed)

```bash
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

## OTA Update

### How it works

```
Host                                  RPi3
────                                  ────
build new rootfs (bitbake)
calculate SHA256
scp image ──────────────────────────► /mnt/staging/ota_image.ext4
trigger ota.sh via ssh ─────────────► verify SHA256
                                      dd image → inactive partition
                                      verify first block (md5)
                                      fw_setenv active_slot
                                      reboot into new slot
```

### Important notes

- The host has no direct access to SD card partitions — everything goes through SSH
- BusyBox `dd` does not support `status=progress`, `conv=fsync`, or `conv=notrunc` — use plain `dd if=... of=... bs=4M`
- `/tmp` is tmpfs (RAM-backed ~452MB) — never use it for the image, always use `/mnt/staging`
- `fw_env.config` must exist in every rootfs slot — solved by adding `u-boot` to `IMAGE_INSTALL`
- Use a good 5V/3A power supply — undervoltage during write corrupts the partition

---

### Run the OTA update

```bash
# On host — make executable once
chmod +x send-ota.sh

# On RPi
ip addr add 192.168.50.100/24 dev eth0

# Run update
./send-ota.sh 192.168.50.100

# Monitor progress on RPi3
ssh root@192.168.50.100 'tail -f /var/log/ota-update.log'
```

### After reboot — verify

```bash
fw_printenv active_slot      # → active_slot=b
mount | grep "on / "         # → /dev/mmcblk0p3 on / ...
```

### Rollback via serial if boot fails

```
U-Boot> setenv active_slot a
U-Boot> saveenv
U-Boot> boot
```

---

## Key Files Summary

| File | Location | Purpose |
|---|---|---|
| `rpi3-ab.wks` | `meta-customimage/wic/` | 4-partition disk layout |
| `boot.cmd` | `meta-customimage/recipes-bsp/rpi-u-boot-scr/files/` | A/B boot logic |
| `base-files_%.bbappend` | `meta-customimage/recipes-core/base-files/` | Adds staging fstab entry |
| `fw_env.config` | installed by `meta-raspberrypi` + `u-boot` package | Points fw_printenv to `/boot/uboot.env` |
| `uboot.env` | `/boot/uboot.env` on FAT partition | Stores `active_slot` variable |
| `ota.sh` | `/usr/bin/ota.sh` on RPi3 | Writes image and switches slot |
| `send-ota.sh` | host laptop | Calculates SHA256, SCPs image, triggers update |

---

## Important Notes

- **`u-boot` must be in `IMAGE_INSTALL`** — it installs `/etc/fw_env.config` into every rootfs slot. Without it you get `Cannot initialize environment` after switching slots.
- **Never use `bmaptool` on a single partition** — it writes full disk images including partition tables.
- **`u-boot.bin` is renamed to `kernel8.img`** by `IMAGE_BOOT_FILES` — this is how RPi firmware loads U-Boot as the 64-bit bootloader.
- **`fw_env.config` must point to `/boot/uboot.env`** — `meta-raspberrypi` uses file-based env storage on the FAT partition, not a raw disk offset.
- **`CMDLINE_ROOT_PARTITION` must be removed** from `local.conf` — it hardcodes NFS root and overrides everything.
- **BusyBox `dd` limitations** — no `status=progress`, no `conv=fsync`, no `conv=notrunc`. Use plain `dd if=... of=... bs=4M`.
- **`/tmp` is RAM-backed (tmpfs ~452MB)** — always store the OTA image on `/mnt/staging` (mmcblk0p4).
- **Undervoltage kills writes** — use a good 5V/3A power supply for the RPi3.