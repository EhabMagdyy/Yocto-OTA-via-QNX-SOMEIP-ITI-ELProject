# 🚀 OTA Update Process — End-to-End Guide

> **Full pipeline:** Host Dashboard → `send-ota.sh` → QNX Validation → SOME/IP Handshake → RPi3 Flashing → New Rootfs Running

---

## Table of Contents

- [System Overview](#system-overview)
- [Architecture Diagram](#architecture-diagram)
- [Hardware Requirements](#hardware-requirements)
- [Network Topology](#network-topology)
- [Phase 1 — Host: Launch the Dashboard](#phase-1--host-launch-the-dashboard)
- [Phase 2 — Host: Send OTA Package](#phase-2--host-send-ota-package)
- [Phase 3 — QNX RPi4: Validate the Package](#phase-3--qnx-rpi4-validate-the-package)
- [Phase 4 — SOME/IP Handshake (QNX ↔ Yocto RPi3)](#phase-4--someip-handshake-qnx--yocto-rpi3)
- [Phase 5 — Yocto RPi3: Flash & Switch Slot](#phase-5--yocto-rpi3-flash--switch-slot)
- [Phase 6 — Reboot & Verification](#phase-6--reboot--verification)
- [A/B Slot Layout](#ab-slot-layout)
- [Security & Integrity Checks](#security--integrity-checks)
- [Log Files Reference](#log-files-reference)
- [Troubleshooting](#troubleshooting)
- [Author](#author)

---

## System Overview

This project implements a **secure, over-the-air (OTA) rootfs update** system spanning three machines:

| Role | Board | OS | IP |
|------|-------|----|----|
| **Host / Operator** | Development PC | Linux (Ubuntu) | — |
| **Gateway / Validator** | Raspberry Pi 4B | QNX 8.0 | `192.168.50.100` |
| **Target / ECU** | Raspberry Pi 3B+ | Yocto (custom) | `192.168.50.50` |

The host operator uses a Qt/QML **Dashboard GUI** to select a built rootfs image and a target QNX IP, then fires `send-ota.sh`. The QNX board acts as a secure gateway — it validates the package integrity and uses **CommonAPI SOME/IP** to negotiate with the Yocto RPi3 service before instructing it to write and boot the new firmware.

---

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                        HOST (Dev Machine)                           │
│                                                                     │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │              Qt/QML OTA Dashboard (appDashboard)             │   │
│  │                                                              │   │
│  │  [Browse .ext4]  [QNX IP Field]  [Send OTA]  [Cancel]        │   │
│  │         │               │             │                      │   │
│  │         └───────────────┴─────────────┘                      │   │
│  │                         │                                    │   │
│  │               spawns send-ota.sh                             │   │
│  └─────────────────────────┼────────────────────────────────────┘   │
│                            │                                        │
│         send-ota.sh:       │                                        │
│         1. uuidgen         │                                        │
│         2. sha256sum       │                                        │
│         3. write manifest  │                                        │
│         4. scp manifest ───┼──────────────────────────────────────► │
│         5. pv | ssh ───────┼──────────────────────── image ───────► │
│         6. ssh validate ───┼──────────────────────── trigger ─────► │
└────────────────────────────┼────────────────────────────────────────┘
                             │  SSH / SCP  (Ethernet)
                             ▼
┌────────────────────────────────────────────────────────────────────┐
│                    QNX RPi4B  (192.168.50.100)                     │
│                                                                    │
│  /tmp/ota_manifest.txt     /tmp/ota_image.ext4                     │
│           │                        │                               │
│           └──────────┬─────────────┘                               │
│                      ▼                                             │
│            qnx-ota-validate.sh                                     │
│            ├─ UUID format check                                    │
│            ├─ Replay attack check  (used-uuids.txt)                │
│            ├─ File size check                                      │
│            └─ SHA256 integrity check                               │
│                      │                                             │
│                      ▼ (all checks pass)                           │
│             runQnx.sh  →  qnxService (CommonAPI SOME/IP Client)    │
│                      │                                             │
│      ┌───────────────┴───────────────────────────────┐             │
│      │  SOME/IP over Ethernet (UDP SD + TCP data)    │             │
│      │                                               │             │
│      │  triggerOta(sha256, size)  ──────────────────►│             │
│      │  ◄─────────────────────── "accepted"          │             │
│      │                                               │             │
│      │  updateStatus("scp_started", ...)  ──────────►│             │
│      │  updateStatus("scp_done",    ...)  ──────────►│             │
│      │  updateStatus("flashing",    ...)  ──────────►│             │
│      └───────────────────────────────────────────────┘             │
│                      │                                             │
│     SCP image ───────┼──────────────────────────────────────────►  │
│     SSH ota.sh ──────┼──────────────────────────────────────────►  │
└──────────────────────┼─────────────────────────────────────────────┘
                       │  SCP + SSH  (Ethernet)
                       ▼
┌────────────────────────────────────────────────────────────────────┐
│                  Yocto RPi3B+  (192.168.50.50)                     │
│                                                                    │
│  yoctoService (CommonAPI SOME/IP Server)                           │
│  ├─ receives triggerOta → validates sha256 format → "accepted"     │
│  ├─ receives updateStatus events → tracks busy state               │
│  └─ fires otaExecutionStatus broadcast → QNX (success/failed)      │
│                                                                    │
│  rpi-ota.sh  (triggered by QNX via SSH)                            │
│  ├─ SHA256 verify image                                            │
│  ├─ Read active_slot from U-Boot env                               │
│  ├─ Determine inactive slot (target partition)                     │
│  ├─ dd wipe 10MB header of target partition                        │
│  ├─ dd write new rootfs to target partition                        │
│  ├─ MD5 verify first block                                         │
│  └─ fw_setenv active_slot → switch boot slot                       │
│                      │                                             │
│                   reboot                                           │
│                      │                                             │
│                      ▼                                             │
│         U-Boot reads new active_slot                               │
│         → boots from updated partition                             │
│         → NEW ROOTFS IS RUNNING ✓                                  │
└────────────────────────────────────────────────────────────────────┘
```

---

## Hardware Requirements

- Raspberry Pi 4B running **QNX 8.0** (gateway/validator)
- Raspberry Pi 3B+ running **Yocto Linux** (target ECU)
- Development PC running **Ubuntu Linux** (host)
- Ethernet connection between the two RPis devices
- SD card on RPi3 with A/B partitioned layout (see [A/B Slot Layout](#ab-slot-layout))

---

## Yocto image partitions

## SD Card Partition Layout — Yocto RPi3

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        mmcblk0 — SD Card                                    │
│                                                                             │
│  ┌──────────────┬───────────────────┬───────────────────┬─────────────────┐ │
│  │    boot      │    rootfs_a       │    rootfs_b       │    staging      │ │
│  │  mmcblk0p1   │   mmcblk0p2       │   mmcblk0p3       │   mmcblk0p4     │ │
│  │   FAT32      │   ext4 · Slot A   │   ext4 · Slot B   │  ext4 · OTA tmp │ │
│  │              │                   │                   │                 │ │
│  │  U-Boot env  │                   │                   │  QNX SCPs image │ │
│  │  kernel, dtb │                   │                   │  here first     │ │
│  └──────────────┴───────────────────┴───────────────────┴─────────────────┘ │
│                             ▲                                               │
│                       active_slot = a                                       │
│                       (U-Boot env)                                          │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Network Topology

```
   Dev PC (Host)
        │
        │ SSH/SCP (any IP)
        │
   ┌────┴────────────────────────────────┐
   │ QNX RPi4 ── Ethernet ── Yocto RPi3 │
   │ 192.168.50.100       192.168.50.50  │
   └────────────────────────────────────┘
        Static IPs on dedicated eth0 link
        UDP multicast SD: 224.224.224.245:30490
        TCP data: 192.168.50.50:30500
```

Both RPi boards must be on the same subnet with **static IPs** and a multicast route for vsomeip service discovery:

```sh
# Run once per boot on Yocto RPi3
route add -net 224.0.0.0 netmask 240.0.0.0 dev eth0
```

---

## Phase 1 — Host: Launch the Dashboard

The **Qt/QML Dashboard** (`appDashboard`) is the operator interface running on the host machine.

### What it does

- Provides a GUI to select a Yocto-built `.ext4` rootfs image from disk
- Accepts the QNX device IP address
- Spawns `send-ota.sh` as a child process and streams live logs to the built-in terminal
- Parses `pv -n` stderr output to drive a real-time progress bar
- Supports cancellation (kills the entire process tree including SSH/SCP children)

### Launch

```sh
cd Dashboard/build
./appDashboard
```

The Dashboard opens on `1000×720`. Select your rootfs `.ext4` image using **Browse Files**, enter the QNX IP, then press **Send OTA**.

```
┌────────────────────────────────────────────────────────────────────┐
│  ↑ OTA Updater          New Rootfs Deployment Tool      ● Idle     │
├──────────────────┬──────────────────────────┬──────────────────────┤
│  Target Device   │  Firmware Image          │  Actions             │
│  ─────────────   │  ─────────────────────   │  ─────────────────   │
│  QNX IP:         │  💾 core-image.ext4      │  [Send OTA] [Cancel] │
│  192.168.1.15    │  [Browse Files]          │                      │
├──────────────────┴──────────────────────────┴──────────────────────┤
│  Deployment Console                                      ● IDLE    │
│  ┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄      │  
│  Logs will appear here...                                          │
├────────────────────────────────────────────────────────────────────┤
│  Transfer Progress                                           —     │
│  ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░                        │
└────────────────────────────────────────────────────────────────────┘
```

---

## Phase 2 — Host: Send OTA Package

`send-ota.sh <QNX_IP>` is invoked by the Dashboard. It prepares and ships the update to the QNX board.

### Step-by-step flow

```
send-ota.sh
│
├─ [1/4] Generate UUID        → uuidgen  (e.g. a1b2c3d4-...)
│
├─ [2/4] Calculate SHA256     → sha256sum core-image.ext4
│
├─ [3/4] Log image info       → path, size in MB
│
└─ [4/4] Ship to QNX
         ├─ scp manifest.txt  → root@QNX_IP:/tmp/ota_manifest.txt
         ├─ pv image.ext4     → root@QNX_IP:/tmp/ota_image.ext4
         │   (pv -n writes % to stderr → Dashboard progress bar)
         └─ ssh               → /usr/bin/qnx-ota-validate.sh
```

### Manifest format

```
uuid=a1b2c3d4-e5f6-4789-abcd-ef0123456789
sha256=<64-char hex>
size=<bytes>
timestamp=2026-05-27T10:00:00Z
image=ota_image.ext4
```

> **SSH Key Auth:** The script uses `~/.ssh/id_ed25519`. Password authentication is disabled. Set up key-based auth to QNX before running.

---

## Phase 3 — QNX RPi4: Validate the Package

Once `send-ota.sh` triggers `qnx-ota-validate.sh` over SSH, QNX performs a multi-layer integrity check before any data touches the target.

### Validation sequence

```
qnx-ota-validate.sh
│
├─ 1. Parse manifest          → extract UUID, SHA256, SIZE
├─ 2. UUID format check       → must match RFC 4122 v4 regex
├─ 3. Replay attack check     → UUID must NOT be in /var/log/qnx-ota-used-uuids.txt
├─ 4. File size check         → stat(image) == manifest SIZE
├─ 5. SHA256 integrity check  → sha256sum(image) == manifest SHA256
├─ 6. Record UUID             → append to used-uuids.txt
│
└─ All checks passed
   → exec runQnx.sh <sha256> <size>
      → qnxService (CommonAPI SOME/IP Client)
```

Any failure at steps 1–5 is logged to `/var/log/qnx-ota.log` and the script exits non-zero, aborting the update safely.

---

## Phase 4 — SOME/IP Handshake (QNX ↔ Yocto RPi3)

Communication between QNX (client) and the Yocto RPi3 (server) uses **CommonAPI SOME/IP** — a standardised automotive IPC framework defined via a FIDL interface.

### FIDL Interface (`ota.fidl`)

```
interface Ota {
    method triggerOta          // QNX → RPi3: "here comes an update"
        in:  sha256 (String), size (UInt64)
        out: status (String)   // "accepted" | "rejected"

    method updateStatus        // QNX → RPi3: progress/error updates
        in:  status (String), message (String)

    broadcast otaExecutionStatus   // RPi3 → QNX: final outcome
        out: status (String)       // "success" | "failed"
             message (String)
}
```

### Message sequence

```
QNX qnxService                          Yocto yoctoService
      │                                         │
      │──── service discovery (UDP SD) ────────►│
      │◄─── service offer ──────────────────────│
      │                                         │
      │──── triggerOta(sha256, size) ──────────►│
      │                                         │  validate sha256 format
      │                                         │  check busy flag
      │◄─── "accepted" ─────────────────────────│
      │                                         │
      │  [SCP image to RPi3 /mnt/staging/]      │
      │──── updateStatus("scp_started") ───────►│
      │──── updateStatus("scp_done") ──────────►│
      │                                         │
      │  [SSH: nohup rpi-ota.sh & ]             │
      │──── updateStatus("flashing") ──────────►│
      │                                         │  logMonitorThread watches
      │                                         │  /var/log/ota-update.log
      │                                         │
      │◄─── broadcast otaExecutionStatus ───────│
            ("success" | "failed", message)
```

### vsomeip configuration

| Parameter        | QNX (client)            | Yocto (server)          |
|------------------|-------------------------|-------------------------|
| Unicast IP       | `192.168.50.100`        | `192.168.50.50`         |
| SD multicast     | `224.224.224.245:30490` | `224.224.224.245:30490` |
| TCP data port    |           —             | `30500`                 |
| Service ID       | `0x1235`                | `0x1235`                |
| Instance         | `0x5679`                | `0x5679`                |

---

## Phase 5 — Yocto RPi3: Flash & Switch Slot

`rpi-ota.sh <sha256> <size> <image_path>` runs on the RPi3, triggered by QNX via SSH.

### Flash sequence

```
rpi-ota.sh
│
├─ Guard checks
│   ├─ SHA256 arg provided?
│   ├─ Size ≥ 100 MB?
│   └─ Image file exists?
│
├─ [1] Verify SHA256 of image on disk
│       sha256sum == expected → proceed
│       mismatch → abort (old slot safe)
│
├─ [2] Read active slot from U-Boot env
│       fw_printenv active_slot  → "a" or "b"
│
├─ [3] Select target partition
│       active=a  →  target=/dev/mmcblk0p3  (slot b)
│       active=b  →  target=/dev/mmcblk0p2  (slot a)
│
├─ [4] Safety: ensure target is not mounted
│
├─ [5] Wipe first 10 MB of target
│       dd if=/dev/zero of=<target> bs=1M count=10
│
├─ [6] Write image
│       dd if=<image> of=<target> bs=4M
│       sync
│
├─ [7] Verify first block (MD5 spot-check)
│       md5(image[0..1M]) == md5(target[0..1M]) → OK
│
├─ [8] Switch boot slot
│       fw_setenv active_slot <next_slot>
│
└─ reboot
```

The `yoctoService` background thread monitors `/var/log/ota-update.log` and fires the `otaExecutionStatus` broadcast event to QNX before the reboot happens.

---

## Phase 6 — Reboot & Verification

After `fw_setenv` switches the active slot and `reboot` is called:

```
RPi3 Power Cycle
      │
      ▼
  U-Boot starts
      │
      ├─ reads fw_env (active_slot=b  or  a)
      ├─ sets kernel cmdline: root=/dev/mmcblk0p3 (or p2)
      └─ loads kernel + dtb from boot partition
             │
             ▼
        Yocto Linux boots from NEW partition
             │
             ▼
        yoctoService starts (via runYocto.sh / init)
             │
             └─ ready to accept next OTA cycle
```

> If the new rootfs fails to boot, the old slot is **untouched** and can be restored by setting `fw_setenv active_slot <old>` from U-Boot prompt or a rescue boot.

---

## A/B Slot Layout

```
SD Card Partition Table (RPi3)
┌─────────────────────────────────────────────────────────────────┐
│  mmcblk0p1  │  mmcblk0p2         │  mmcblk0p3                  │
│  /boot      │  rootfs — Slot A   │  rootfs — Slot B            │
│  FAT32      │  ext4              │  ext4                        │
│  kernel,dtb │  current or next   │  current or next            │
│  u-boot.env │                    │                              │
└─────────────────────────────────────────────────────────────────┘
                    ▲                         ▲
                    │                         │
              active_slot=a           active_slot=b
              (U-Boot env)            (U-Boot env)
```

U-Boot reads `active_slot` from its environment partition and sets `root=` accordingly before loading the kernel.

---

## Security & Integrity Checks

| Check | Where | Purpose |
|-------|-------|---------|
| UUID generation | `send-ota.sh` | Unique transaction ID |
| SHA256 of image | `send-ota.sh` | Detect corruption before send |
| UUID format validation | `qnx-ota-validate.sh` | Reject malformed packages |
| Replay attack prevention | `qnx-ota-validate.sh` | Reject re-sent packages via used-UUID log |
| File size check | `qnx-ota-validate.sh` | Detect partial transfers |
| SHA256 re-verification | `qnx-ota-validate.sh` | Validate image after SCP |
| SHA256 re-verification | `rpi-ota.sh` | Validate image before writing to flash |
| Minimum size guard | `rpi-ota.sh` | Reject accidentally tiny images |
| Mount check | `rpi-ota.sh` | Never overwrite a live partition |
| First-block MD5 | `rpi-ota.sh` | Spot-check flash write integrity |
| CommonAPI validation | `yoctoService` | Reject invalid SHA256 format; reject when busy |
| SSH key-only auth | `send-ota.sh` | No password auth to QNX |

---

## Log Files Reference

| File | Machine | Contents |
|------|---------|----------|
| `/var/log/qnx-ota.log` | QNX | Validation steps and outcomes |
| `/var/log/qnx-ota-used-uuids.txt` | QNX | All consumed UUIDs (replay guard) |
| `/var/log/ota-update.log` | Yocto RPi3 | Full flash script output |
| Dashboard console | Host | Live `send-ota.sh` stdout + progress |

Monitor the flash in real-time from the host:
```sh
ssh root@192.168.50.50 'tail -f /var/log/ota-update.log'
```

---

## Final Dashboard Terminal Logs
```
[1/4] Generating OTA UUID...
      UUID: 6ec17256-b79a-498b-89a4-ef8f0699ce63
[2/4] Calculating SHA256...
      SHA256: 670e335fc4b4bfa26027d4257a5cf222a5b6c32a17ae2b0daf0d9fd72de6cd14
[3/4] Image Information
      Image:  /home/ehab/Documents/ITI_9Months/Yocto/shared-build/tmp-glibc/deploy/images/raspberrypi3-64/core-image-custom-raspberrypi3-64.rootfs-20260526005235.ext4
      Size:   1073741824 bytes (1024 MB)
[4/4] Copying image + manifest to QNX RPi4...
[2026-05-27 01:40:22] === QNX OTA Validation Started ===
[2026-05-27 01:40:22] UUID format OK
[2026-05-27 01:40:22] UUID is fresh
[2026-05-27 01:40:22] Size OK
[2026-05-27 01:40:22] Verifying SHA256...
[2026-05-27 01:41:14] SHA256 OK
[2026-05-27 01:41:14] UUID recorded
[2026-05-27 01:41:14] === Validation complete - Launching qnxService ===
[CAPI][INFO]
Loading configuration file /etc//commonapi-someip.ini
[CAPI][INFO] Loading configuration file '/etc/commonapi.ini'
[CAPI][INFO] Using default binding 'someip'
[CAPI][INFO] Using default shared library folder '/usr/local/lib/commonapi'
[CAPI][INFO]
2026-05-27 01:41:14.744198 qnxService [info] Using configuration file: "/system/capi-ota/qnx-config.json".
2026-05-27 01:41:14.744203 qnxService [info] Parsed vsomeip configuration in 1ms
2026-05-27 01:41:14.744205 qnxService [info] Configuration module loaded.
2026-05-27 01:41:14.744206 qnxService [info] Security disabled!
2026-05-27 01:41:14.744208 qnxService [info] Initializing vsomeip (3.5.5) application "qnxService".
2026-05-27 01:41:14.744211 qnxService [info] Instantiating routing manager [Host].
2026-05-27 01:41:14.744216 qnxService [info] create_routing_root: Routing root @ /var/vsomeip-0
2026-05-27 01:41:14.744223 qnxService [info] Service Discovery enabled. Trying to load module.
Registering function for creating "commonapi.Ota:v1_0" stub adapter.
2026-05-27 01:41:14.744529 qnxService [info] Service Discovery module loaded.
2026-05-27 01:41:14.744535 qnxService [info] Application(qnxService, 1236) is initialized (11, 100).
2026-05-27 01:41:14.744540 qnxService [info] Starting vsomeip application "qnxService" (1236) using 2 threads I/O nice 0
2026-05-27 01:41:14.744541 qnxService [info] REGISTER EVENT(1236): [1235.5679.947a:is_provider=false]
2026-05-27 01:41:14.744545 qnxService [info] main dispatch thread id from application: 1236 (qnxService) is: 6
2026-05-27 01:41:14.744547 qnxService [info] shutdown thread id from application: 1236 (qnxService) is: 7
2026-05-27 01:41:14.744545 qnxService [info] REQUEST(1236): [1235.5679:1.4294967295]
2026-05-27 01:41:14.744554 qnxService [info] SOME/IP routing ready.
2026-05-27 01:41:14.744556 qnxService [info] udp_server_endpoint_impl<multicast>: SO_RCVBUF is: 1703936 (1703936) local port:30490
2026-05-27 01:41:14.744560 qnxService [info] Watchdog is disabled!
2026-05-27 01:41:14.744563 qnxService [info] create_local_server: Listening @ /var/vsomeip-1236
[QNX] Client started. Waiting for RPi3 OTA server...
2026-05-27 01:41:14.744565 qnxService [info] io thread id from application: 1236 (qnxService) is: 5
2026-05-27 01:41:14.744565 qnxService [info] io thread id from application: 1236 (qnxService) is: 10
2026-05-27 01:41:14.744569 qnxService [info] vSomeIP 3.5.5 | (default)
2026-05-27 01:41:14.744897 qnxService [info] endpoint_manager_impl::create_remote_client: 192.168.50.50:30500 reliable: 1 using local port: 0
2026-05-27 01:41:14.744902 qnxService [warning] tce::connect:connecting to: local:192.168.50.100:61534 remote: 192.168.50.50:30500
2026-05-27 01:41:14.744911 qnxService [info] routing_manager_stub::on_offer_service: ON_OFFER_SERVICE(0000): [1235.5679:1.0]
[QNX] Connected to RPi3 Server! Subscribing to execution status broadcasts...
2026-05-27 01:41:14.749573 qnxService [info] SUBSCRIBE(1236): [1235.5679.1b5b:947a:1]
[QNX] RPi3 Server found! Requesting OTA transfer...
[QNX] RPi3 response: accepted
[QNX] scp /tmp/ota_image.ext4 root@192.168.50.50:/mnt/staging/ota_image.ext4
.
.
.
.
.
======================================================================
[QNX] Process complete! RPi3 is successfully flashing and rebooting.
======================================================================
.
.

```

---

## Troubleshooting

**`SHA256 mismatch` on QNX**
The image was corrupted during SCP. Re-run the Dashboard. Check network stability.

**`UUID already used - replay attack rejected`**
A package with the same UUID was already applied. The Dashboard generates a fresh UUID per run — this should not happen unless you are retrying a previously-sent exact manifest file.

**`active_slot` missing / invalid**
U-Boot environment is not set up. Flash U-Boot with a valid `fw_env.config` pointing to the environment partition and initialise `active_slot`.

**SOME/IP: qnxService can't find yoctoService**
- Verify the multicast route exists on Yocto: `route add -net 224.0.0.0 netmask 240.0.0.0 dev eth0`
- Confirm both boards can ping each other
- Check `VSOMEIP_CONFIGURATION` env var points to the correct JSON file

**Progress bar stuck at 0%**
`pv` is not installed on the host. Install with `sudo apt install pv`.

**SCP from QNX to RPi3 fails**
Ensure `root@192.168.50.50` has a trusted SSH key from the QNX board: `ssh-copy-id root@192.168.50.50` from QNX.

---

## Author

**Ehab** — Embedded Software Engineer
ITI 9-Month Professional Diploma — Embedded Systems Track

> ITI 9-Months Embedded Linux Course Project integrating Yocto Linux, QNX RTOS, CommonAPI SOME/IP, and Qt/QML into a production-grade OTA update pipeline for automotive-style embedded targets.

---

*Last updated: May 2026*
