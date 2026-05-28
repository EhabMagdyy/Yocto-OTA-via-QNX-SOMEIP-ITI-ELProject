# 🍓 Yocto Target Node — OTA Deployment Architecture

This document details the receiving end of the Over-The-Air (OTA) pipeline running on the Raspberry Pi 3B+ (Yocto). The Yocto board acts as the SOME/IP Server, securely accepting the firmware payload from the QNX edge gateway, orchestrating the A/B partition swap, and reporting execution status back to the client.

---

## 🌊 Complete System Flow

```plain
[ QNX Edge Gateway ]                           [ Yocto System ]
         │
         │ 1. triggerOta(sha256, size)
         ├───────────────────────────► [ yoctoService (C++ SOME/IP Server) ]
         │                                       │
         │ 2. scp payload                        │ (Locks 'busy' state, listens on port 30500)
         ├───────────────────────────► [ /mnt/staging/ota_image.ext4 ]
         │                                       │
         │ 3. ssh nohup ota.sh                   │ (Spawns Log Monitor Thread)
         ├───────────────────────────► [ /usr/bin/ota.sh (Flashing Script) ]
         │                                       │
         │                                       ├─► 1. Verify SHA256 Checksum
         │                                       ├─► 2. Read U-Boot Active Slot (A/B)
         │                                       ├─► 3. Wipe Inactive Partition Header
         │                                       ├─► 4. Flash Ext4 Image (dd)
         │                                       ├─► 5. Verify First Block MD5
         │                                       ├─► 6. Update U-Boot Env (fw_setenv)
         │                                       │
         │ 4. otaExecutionStatus("success")      │ (Log Monitor reads "=== OTA Complete")
         ◄──────────────────────────── [ yoctoService Broadcasts Success ]
                                                 │
                                       [ Hardware Reboot into New Rootfs ]
```

---


## 📡 Phase 1: Middleware Orchestration (C++ / SOME/IP)

1. The yoctoService C++ application runs as a background daemon, implementing the commonapi.Ota interface. Its primary job is to coordinate the network state and bridge the gap between the incoming network payload and the local shell execution.

2. Service Registration: The daemon registers as a SOME/IP Server over TCP port 30500 and broadcasts its availability via UDP Multicast Service Discovery.

3. Handshake & Locking: When QNX calls triggerOta, Yocto validates the checksum format and sets a thread-safe atomic busy flag to true, preventing any concurrent update attempts.

4. Live Telemetry Receiver: Yocto exposes the updateStatus fire-and-forget method. QNX calls this method continuously during the SCP file transfer to pipe live status updates ("scp_started", "scp_done") directly into Yocto's application memory for console tracking.

5. The Log Watchdog: The moment an update is accepted, Yocto spawns a detached background thread (logMonitorThread). This thread tail-reads /var/log/ota-update.log. When it detects the keyword "ERROR:", it broadcasts a failure event. When it detects "=== OTA Complete", it fires the otaExecutionStatus("success") broadcast event back to QNX to close the deployment loop.

---

## ⚙️ Phase 2: Dual-Bank Flashing Execution (Shell)
> Once the 1GB .ext4 image is fully staged in /mnt/staging/, QNX triggers the local /usr/bin/ota.sh script. This script handles the highly sensitive low-level block writes and U-Boot environment variables required for an A/B partition swap.

1. Pre-Flight Validation: The script recalculates the SHA256 hash of the fully transferred image against the expected hash provided by QNX. If a single bit was corrupted over SCP, the script instantly aborts.

2. Target Slot Determination: The script uses fw_printenv active_slot to query the U-Boot bootloader. It determines whether the system is currently running on partition A (/dev/mmcblk0p2) or B (/dev/mmcblk0p3), and securely sets the target variable to the inactive offline partition.

3. Safety Unmount & Header Wipe: It ensures the target partition is unmounted, then uses dd if=/dev/zero to wipe the first 10MB of the target partition. This destroys any residual filesystem signatures, ensuring a clean slate.

4. Raw Block Write: The payload is flashed directly to the block device using dd bs=4M. The sync command ensures all OS page caches are flushed to physical storage.

5. Block-Level Verification: Before trusting the flash, the script calculates an MD5 hash of the first 1MB of the original image, and compares it against an MD5 hash of the first 1MB of the physical SD card partition. If they do not match, the system aborts, leaving the active slot completely untouched and safe.

6. Bootloader Handoff: The script uses fw_setenv active_slot to permanently flip the boot flag to the newly flashed partition.

7. Reboot Sequence: The script logs "=== OTA Complete — Rebooting into slot..." (which triggers the C++ broadcast back to QNX), waits 1 second for the network buffer to flush, and executes a hard reboot.