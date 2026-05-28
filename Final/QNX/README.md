# 🛡️ QNX Edge Gateway — OTA Architecture

This document outlines the Over-The-Air (OTA) update pipeline running on the QNX edge device. In this architecture, the QNX board acts as a secure mediator and validation gateway between the Host (operator dashboard) and the Yocto Server (the target node receiving the update).

---

## Phase 1: Host-to-QNX Validation Pipeline

> This phase handles the initial reception, cryptographic verification, and security screening of the incoming firmware package from the Host Operator Dashboard. It ensures that only fully verified, authorized, and fresh payloads are allowed to proceed to the internal target nodes.

### 🌊 Flow Diagram

```plain
┌────────────────┐                      ┌────────────────────┐
│ Host Dashboard │                      │ QNX /tmp/ Storage  │
└───────┬────────┘                      └─────────┬──────────┘
        │                                         │
        ├─ 1. SCP ota_manifest.txt ─────────────> │
        ├─ 2. SSH Stream ota_image.ext4 ────────> │
        │                                         │
        │    ┌───────────────────────┐            │
        ├─ 3. Trigger execution SSH ───> qnx-ota-validate.sh
        │    └───────────────────────┘            │
        │                                         ├─ Read Manifest (UUID, Size, Hash)
        │                                         │
        │                                         │  [Security & Integrity Checks]
        │   qnx logs is mapped in the dashboard   ├─► Regex match UUID v4 format
        │ <────────────────────────────────────── ├─► Check Anti-Replay Ledger
        │                                         ├─► Verify Actual File Size
        │                                         ├─► Verify Actual SHA256
        │                                         │
        │                                         ├─ Record new UUID as used 
        │                                         │
                                                  ▼
                                        Launch ./runQnx.sh
                                (Proceeds to SOME/IP Yocto Phase)
```

---

### ⚙️ Execution Logic & Security Checks

> The validation is driven by the /usr/bin/qnx-ota-validate.sh script, which is triggered remotely by the Host once the file transfer is complete.

1. Asset Verification: The script first checks that both the ota_manifest.txt and ota_image.ext4 have been successfully placed in the /tmp/ directory by the host.

2. Manifest Parsing: It extracts the expected UUID, SHA256, and SIZE variables from the manifest file.

3. Format Integrity: The UUID is checked against a strict Regular Expression to ensure it matches the standard UUID v4 format, preventing basic injection attempts.

4. Anti-Replay Protection: The script references /var/log/qnx-ota-used-uuids.txt. If the current UUID already exists in this ledger, the script immediately aborts. This prevents a malicious actor from capturing an old, valid OTA package and resending it to downgrade the system.

5. Payload Verification: * Compares the physical byte size of ota_image.ext4 against the manifest.

6. Calculates the actual SHA256 hash of the 1GB payload and strictly compares it to the host's expected hash.

7. Ledger Update & Handoff: Once all checks pass, the UUID is permanently recorded to the used-list, and the script launches the qnxService SOME/IP client, passing the validated SHA256 and SIZE forward to begin Phase 2.

---

## 🚀 Phase 2: QNX-to-Yocto Delivery Pipeline
> Once validated, the QNX board assumes the role of a SOME/IP Client (Proxy). It connects to the Yocto board, which runs as the SOME/IP Server (Stub) listening on port 30500. QNX controls the network transfer, pushes status telemetry, and waits for a final success broadcast from Yocto.

### 🌊 SOME/IP Deployment Flow

```plain
[ QNX Service (Client) ]                     [ Yocto Service (Server) ]
          │                                              │
          ├─ 1. triggerOta(sha256, size) ───────────────►│ (Checks busy state & checksum)
          │◄───────────────────────────────── "accepted" ┤
          │                                              │
          ├─ 2. updateStatus("scp_started") ────────────►│ (Logs to Yocto console)
          ├─ 3. scp -l 8000 ota_image.ext4 ─────────────►│ [ Yocto /mnt/staging/ ]
          ├─ 4. updateStatus("scp_done") ───────────────►│
          │                                              │
          ├─ 5. ssh nohup ota.sh ───────────────────────►│ (Triggers local flashing)
          ├─ 6. updateStatus("flashing") ───────────────►│
          │                                              │
          │                                              │ [ Yocto Log Monitor Thread ]
          │                                              │ (Reads /var/log/ota-update.log)
          │                                              │
          │◄──────── otaExecutionStatus("success") ──────┤ (Reads "=== OTA Complete")
          │                                              │
     [ Done ]                                       [ Rebooting into new slot ]
```

---

## ⚙️ Execution Logic & Telemetry
> This phase utilizes the Franca IDL (ota.fidl) definitions over a vsomeip multicast/unicast network configuration.

1. Service Discovery: qnxService uses multicast UDP to locate the yoctoService server. Once found, it establishes a reliable TCP connection.

2. The Handshake: QNX calls the triggerOta RPC method. Yocto verifies it is not currently busy, validates the SHA256 string format, and returns an accepted status.

3. Rate-Limited Transfer: QNX initiates a background thread to transfer the payload. It uses a rate-limited SCP command (scp -l 8000) to prevent the encryption overhead from starving the RPi3 CPU, ensuring vsomeip keep-alive heartbeats are not dropped.

4. Live Telemetry (updateStatus): Because the transfer and flashing happen asynchronously, QNX acts as a remote logger. It repeatedly fires the one-way updateStatus method to push state changes (scp_started, scp_done, flashing, error) directly into Yocto's application memory for console tracking.

5. Execution & The File Watchdog: QNX executes Yocto's internal ota.sh script via SSH. On the Yocto side, a detached C++ thread (logMonitorThread) continuously monitors /var/log/ota-update.log.

6. Final Broadcast Confirmation: The moment the Yocto shell script writes === OTA Complete to the log, the Yocto C++ monitor thread catches it and fires the otaExecutionStatus broadcast event back to QNX. QNX receives the success notification and closes its deployment context just as the Yocto board reboots into the newly flashed rootfs slot.
