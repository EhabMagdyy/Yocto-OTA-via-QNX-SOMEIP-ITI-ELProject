# 🖥️ OTA Dashboard Host-Side Orchestrator

This repository contains the Host System architecture for the OTA deployment pipeline. The host machine acts as the orchestrator, wrapping complex deployment scripts into a user-friendly Qt6/QML graphical interface to securely dispatch Yocto rootfs updates to the QNX edge gateway.

---

## 📌 The Role of the Host System

The host environment (the operator's desktop) is responsible for initiating the OTA lifecycle. It does not perform the update itself; rather, it prepares the payload, establishes secure communication with the target device, pushes the data, and remotely triggers the validation sequence. 

**Primary Responsibilities:**
* **Payload Verification:** Ensures the selected `.ext4` firmware image exists and calculates its cryptographic hashes and file metadata before transmission.
* **Network Orchestration:** Manages SSH tunnels and securely transfers large binary files without interactive password prompts.
* **Process Lifecycle Management:** Maintains strict control over deployment threads. If an operator cancels the operation, all underlying network transfers (e.g., orphaned `scp` or `ssh` sessions) are cleanly terminated.
* **Operator Feedback:** Provides real-time, human-readable logging and visual progress tracking to the operator.

---

## 🏗️ Component Architecture

The host-side logic is divided into three distinct layers:

### 1. Presentation Layer (QML / UI) | `Dashboard/`
Driven by `Main.qml`, this layer handles the visual interface. It processes user inputs (selecting the `.ext4` image, entering the QNX IP address) and reacts to state changes. It binds directly to the C++ backend properties to animate the deployment console and progress bars seamlessly.

### 2. Middleware & Process Control (C++)
The `OtaProcess` class (`backend.cpp` / `backend.hpp`) acts as the bridge between the QML frontend and the underlying Linux operating system. 
* Wraps `QProcess` to spawn the bash scripts asynchronously. 
* Captures standard output (`stdout`) to feed the visual terminal.
* Parses standard error (`stderr`) to extract raw integer values for the progress bar.

### 3. Execution Engine (Bash Script) | `send-ota.sh`
The `send-ota.sh` script is the underlying workhorse. It handles the actual file system interactions, metadata generation, and SSH/SCP networking required to push the payload to the QNX target.

---

## ⚙️ Execution Workflow

When the operator clicks **"Send OTA"**, the system executes the following sequence:

| Phase | Component | Action / Logic |
| :--- | :--- | :--- |
| **1. Initialization** | `Main.qml` | UI validates that an image and IP are provided, clears the console, and invokes `otaProcess.start()`. |
| **2. Process Spawning** | `backend.cpp` | `QProcess` launches `/bin/bash` targeting `send-ota.sh`, passing the QNX IP and Image path as arguments. |
| **3. Metadata Gen** | `send-ota.sh` | Generates a unique `OTA_UUID` using `uuidgen`, calculates the `SHA256` checksum of the `.ext4` image, and calculates the exact byte size. |
| **4. Manifest Creation**| `send-ota.sh` | Writes the UUID, SHA256, size, timestamp, and target filename into a temporary `ota_manifest.txt` file. |
| **5. Pre-Flight Send** | `send-ota.sh` | Executes a quiet `scp` command to push the lightweight manifest file to `/tmp/` on the QNX target. |
| **6. Payload Streaming**| `send-ota.sh` | Pipes the heavy rootfs image through `pv -n` (Pipe Viewer) directly into an `ssh` stream, saving it as `/tmp/ota_image.ext4` on QNX. |
| **7. Progress Parsing** | `backend.cpp` | Listens to the `stderr` output of `pv -n` (which emits bare integers 0-100) and emits the `progressChanged` signal to update the UI progress bar. |
| **8. Remote Trigger** | `send-ota.sh` | Once the transfer finishes, the script fires an SSH command to execute `/usr/bin/qnx-ota-validate.sh` on the QNX board. |

---

## 🛠️ Key Technical Implementations

### Recursive Process Tree Termination
If the operator presses "Cancel" mid-transfer, simply killing the parent bash script is insufficient, as it leaves orphaned `ssh` and `pv` child processes running in the background. The backend implements a robust `killProcessTree()` function that recursively traverses the Linux `/proc/<pid>/task/<pid>/children` directory. It ensures every child and sub-child process spawned by the deployment is aggressively terminated (`SIGKILL`), completely freeing up system and network resources.

### Split-Stream Telemetry
The host effectively decouples human-readable logs from system metrics using standard Linux output streams. `send-ota.sh` utilizes standard `echo` statements for status updates (captured by `stdout` and printed to the GUI console). Meanwhile, the `pv -n` utility is forced to emit its raw percentage integers via `stderr`. The C++ backend effortlessly separates these streams, preventing log pollution while maintaining a perfectly synchronized progress bar.

### Unattended SSH Architecture
To ensure the GUI never hangs waiting for a terminal password prompt, the `send-ota.sh` script enforces strict SSH configurations (`-o BatchMode=yes`, `-o StrictHostKeyChecking=no`, `-o PasswordAuthentication=no`). It relies entirely on a predefined Ed25519 public key, allowing the host to slide the payload through the network headlessly.