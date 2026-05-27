# 🖥️ OTA Dashboard — Build & Run Guide

> **Qt6/QML desktop application** — the host-side operator interface for triggering OTA updates to the QNX + Yocto target system.

---

## Navigation

- [What Is This App?](#what-is-this-app)
- [App Architecture](#app-architecture)
- [UI Overview Diagram](#ui-overview-diagram)
- [Prerequisites](#prerequisites)
- [Project Structure](#project-structure)
- [Building the App](#building-the-app)
  - [Option A — Qt Creator (Recommended)](#option-a--qt-creator-recommended)
  - [Option B — Command Line (CMake)](#option-b--command-line-cmake)
- [Configuration Before Running](#configuration-before-running)
- [Running the App](#running-the-app)
- [Using the Dashboard](#using-the-dashboard)
- [How the Backend Works](#how-the-backend-works)
- [Customisation](#customisation)
- [Troubleshooting](#troubleshooting)
- [Author](#author)

---

## What Is This App?

The **OTA Dashboard** is a Qt6/QML desktop application that gives the operator a graphical interface to deploy a new Yocto rootfs image to the embedded target system. It wraps the `send-ota.sh` shell script into a clean GUI, providing:

- File picker for selecting the `.ext4` rootfs image
- IP address input for the QNX gateway device
- Live deployment console (terminal-style log streaming)
- Real-time transfer progress bar (driven by `pv` percentage output)
- Process cancellation with full child-process tree kill

---

## App Architecture

```
┌─────────────────────────────────────────────────────────┐
│                  appDashboard (Qt6 App)                  │
│                                                          │
│  ┌──────────────────────────────────────────────────┐   │
│  │                  Main.qml (UI)                   │   │
│  │                                                  │   │
│  │  ┌─────────────┐  ┌──────────────┐  ┌────────┐  │   │
│  │  │ Target Card │  │ Image Card   │  │Actions │  │   │
│  │  │ (IP input)  │  │ (file picker)│  │ Card   │  │   │
│  │  └─────────────┘  └──────────────┘  └────────┘  │   │
│  │                                                  │   │
│  │  ┌────────────────────────────────────────────┐  │   │
│  │  │  Deployment Console (TextArea, monospace)  │  │   │
│  │  └────────────────────────────────────────────┘  │   │
│  │                                                  │   │
│  │  ┌────────────────────────────────────────────┐  │   │
│  │  │  Transfer Progress Bar + % pill            │  │   │
│  │  └────────────────────────────────────────────┘  │   │
│  └──────────────────────┬───────────────────────────┘   │
│                         │ QML context property           │
│                         ▼                                │
│  ┌──────────────────────────────────────────────────┐   │
│  │             OtaProcess (C++ Backend)              │   │
│  │                                                   │   │
│  │  QProcess wrapper                                 │   │
│  │  ├─ start(script, args)  → spawns send-ota.sh    │   │
│  │  ├─ stdout → logOutput signal → console TextArea  │   │
│  │  ├─ stderr (pv -n %) → progressChanged signal     │   │
│  │  ├─ kill() → killProcessTree() (recursive /proc)  │   │
│  │  └─ finished → runningChanged + final progress    │   │
│  └──────────────────────────────────────────────────┘   │
│                         │                                │
│                         ▼                                │
│                  send-ota.sh  (subprocess)               │
│                  └─ ssh/scp to QNX target                │
└─────────────────────────────────────────────────────────┘
```

**Key design choices:**
- `QProcess` stdout → log console (human-readable messages)
- `QProcess` stderr → progress bar only (`pv -n` emits bare integers 0–100)
- Cancel kills the entire Linux process group recursively via `/proc/<pid>/task/<pid>/children`
- QML binding to `OtaProcess` is done via `QQmlContext::setContextProperty` (no QML plugin needed)

---

## UI Overview Diagram

```
┌──────────────────────────────────────────────────────────────────────┐
│  ↑ OTA Updater                New Rootfs Deployment Tool   ● Idle   │
├──────────────────┬───────────────────────────┬──────────────────────┤
│  Target Device   │  Firmware Image           │  Actions             │
│                  │                           │                      │
│  QNX Device IP:  │  💾 No image selected     │  ┌──────┐ ┌───────┐  │
│  ┌────────────┐  │  Browse for .ext4 rootfs  │  │ Send │ │Cancel │  │
│  │192.168.1.15│  │  ┌─────────────────────┐  │  │ OTA  │ │       │  │
│  └────────────┘  │  │   Browse Files       │  │  └──────┘ └───────┘  │
│                  │  └─────────────────────┘  │  Select an image and  │
│                  │                           │  target to begin      │
├──────────────────┴───────────────────────────┴──────────────────────┤
│  ● Deployment Console                                       ● IDLE  │
│  ┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄  │
│  [1/4] Generating OTA UUID...                                        │
│        UUID: a1b2c3d4-...                                            │
│  [2/4] Calculating SHA256...                                         │
│        SHA256: 3f4a...                                               │
│  [3/4] Image Information                                             │
│        Image: core-image-custom-raspberrypi3-64.rootfs.ext4          │
│        Size:  524288000 bytes (500 MB)                               │
│  [4/4] Copying image + manifest to QNX RPi4...                      │
├──────────────────────────────────────────────────────────────────────┤
│  ▌ Transfer Progress                                          67 %  │
│  ██████████████████████████████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░     │
└──────────────────────────────────────────────────────────────────────┘
```

Visual states:
- **Idle** — Green status dot. Send OTA enabled when image + IP are set.
- **Running** — Amber pulsing dot. Cancel enabled. Progress bar animates.
- **Done** — Progress reaches 100%. Console shows exit code.

---

## Prerequisites

| Dependency | Version | Notes |
|------------|---------|-------|
| Qt6 | ≥ 6.8 | `Quick`, `QuickControls2`, `Dialogs` modules required |
| CMake | ≥ 3.16 | Ships with Qt Creator |
| C++ compiler | GCC / Clang with C++17 | |
| `pv` | any | `sudo apt install pv` — drives the progress bar |
| `ssh` / `scp` | OpenSSH | Pre-installed on Ubuntu |
| `uuidgen` | any | `sudo apt install uuid-runtime` |
| SSH key to QNX | `~/.ssh/id_ed25519` | Key-based auth; no password prompts |

### Install Qt6 on Ubuntu

```sh
sudo apt update
sudo apt install qt6-base-dev qt6-declarative-dev qt6-tools-dev cmake build-essential
```

Or download **Qt Creator** from [https://www.qt.io/download-qt-installer](https://www.qt.io/download-qt-installer) and install the Qt 6.8 component.

---

## Project Structure

```
Dashboard/
├── CMakeLists.txt       # CMake build definition
├── main.cpp             # App entry point; registers OtaProcess with QML context
├── backend.hpp          # OtaProcess class declaration (QObject, Q_PROPERTY)
├── backend.cpp          # OtaProcess implementation (QProcess, kill tree, progress)
├── Main.qml             # Full UI — all layout, animations, file dialog
└── build/               # Generated build directory (git-ignored)
```

---

## Building the App

### Option A — Qt Creator (Recommended)

1. Open **Qt Creator**
2. **File → Open File or Project** → select `Dashboard/CMakeLists.txt`
3. Choose a **Qt 6.8** kit when prompted
4. Click the **▶ Run** button (or **Ctrl+R**)

Qt Creator handles CMake configuration, compilation, and launch automatically.

---

### Option B — Command Line (CMake)

```sh
# 1. Navigate to the Dashboard directory
cd Dashboard

# 2. Create a build directory
mkdir -p build && cd build

# 3. Configure (replace path with your Qt6 installation if not in PATH)
cmake .. -DCMAKE_BUILD_TYPE=Release

# If Qt6 is not on PATH, specify it:
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.x/gcc_64

# 4. Build
cmake --build . -j$(nproc)

# 5. Run
./appDashboard
```

#### Verify Qt6 is found

```sh
qmake6 --version
# Qt version 6.x.x ...
```

---

## Configuration Before Running

Before launching, verify two paths in `Main.qml` match your environment:

### 1. Default image directory (file picker starting folder)

```qml
// Main.qml  ~line 350
FileDialog {
    currentFolder: "file:///home/ehab/Documents/ITI_9Months/Yocto/shared-build/tmp-glibc/deploy/images/raspberrypi3-64"
```

Change `ehab` to your username and adjust the Yocto deploy path accordingly.

### 2. Path to `send-ota.sh`

```qml
// Main.qml  ~line 230
otaProcess.start("bash", [
    "/home/ehab/Documents/ITI_9Months/Courses-Project/Embedded-Linux/Final/send-ota.sh",
    targetIP.text,
    root.selectedImage
])
```

Update this to the absolute path where `send-ota.sh` lives on your machine.

### 3. SSH key path in `send-ota.sh`

```sh
# send-ota.sh  ~line 30
SSH_KEY="/home/ehab/.ssh/id_ed25519"
```

Update to your SSH key path and ensure it is authorised on the QNX board.

---

## Running the App

```sh
cd Dashboard/build
./appDashboard
```

Or from Qt Creator: **Ctrl+R**

The window opens at 1000×720 (minimum 900×600). It is resizable.

---

## Using the Dashboard

### Step 1 — Select the Firmware Image

Click **Browse Files** in the *Firmware Image* card. Navigate to your Yocto deploy directory and select the `.ext4` rootfs image (e.g. `core-image-custom-raspberrypi3-64.rootfs.ext4`).

The card updates to show the filename and turns amber when a file is selected.

### Step 2 — Set the QNX Target IP

In the *Target Device* card, type the IP address of your QNX RPi4 board. Default is `192.168.1.15` — change it to match your network (typically `192.168.50.100`).

### Step 3 — Send OTA

Click **Send OTA**. The button is only enabled when both an image and IP are set and no process is running.

The Dashboard:
- Clears the console
- Spawns `send-ota.sh` as a subprocess
- Streams all stdout to the Deployment Console
- Parses `pv -n` stderr integers to animate the progress bar
- Shows an amber pulsing status dot while running

### Step 4 — Monitor Progress

Watch the **Deployment Console** for live output:

```
[1/4] Generating OTA UUID...
      UUID: a1b2c3d4-e5f6-4789-abcd-ef0123456789
[2/4] Calculating SHA256...
      SHA256: 3f4a9b...
[3/4] Image Information
      Image:  core-image-custom-raspberrypi3-64.rootfs.ext4
      Size:   524288000 bytes (500 MB)
[4/4] Copying image + manifest to QNX RPi4...
```

The progress bar fills as `pv` reports transfer percentage.

### Step 5 — Completion

On success the console shows:

```
======================================
OTA SENT TO QNX
======================================
UUID:    a1b2c3d4-...
SHA256:  3f4a9b...
SIZE:    524288000 bytes
TARGET:  192.168.50.100
======================================

--- Process finished (exit code: 0) ---
```

Progress reaches 100% and the status dot returns to green.

### Cancelling

Click **Cancel** at any time while the process is running. The backend calls `killProcessTree()` which recursively walks `/proc` to kill all child processes (including `ssh`, `scp`, and `pv`), then waits up to 2 seconds for clean exit.

---

## How the Backend Works

### `OtaProcess` class (`backend.hpp` / `backend.cpp`)

`OtaProcess` is a `QObject` subclass exposed to QML as a context property named `otaProcess`.

**Properties (Q_PROPERTY):**

| Property | Type | Description |
|----------|------|-------------|
| `running` | `bool` | True while `QProcess` state ≠ `NotRunning` |
| `progress` | `int` | 0–100, parsed from `pv -n` stderr |

**Signals:**

| Signal | Payload | Description |
|--------|---------|-------------|
| `logOutput` | `QString` | New stdout text to append to console |
| `runningChanged` | — | Process started or finished |
| `progressChanged` | `int` | New progress percentage |

**Invokable methods:**

| Method | Description |
|--------|-------------|
| `start(script, args)` | Starts `QProcess`; no-op if already running |
| `kill()` | Kills entire process tree via `/proc` |

**Process tree kill:**

```cpp
void killProcessTree(qint64 pid) {
    // Read /proc/<pid>/task/<pid>/children
    // Recursively kill each child
    // Then kill pid itself with SIGKILL
}
```

This ensures no orphaned `ssh` or `scp` processes are left behind when the operator cancels.

---

## Customisation

**Change default QNX IP** — edit the `TextField` default value in `Main.qml`:
```qml
TextField {
    id: targetIP
    text: "192.168.50.100"   // ← change here
```

**Change window size** — edit `Main.qml`:
```qml
width: 1000
height: 720
minimumWidth: 900
minimumHeight: 600
```

**Disable progress bar indeterminate mode** — the bar sweeps when `progress == 0` and the process is running. To disable, remove the `indeterminateAnim` `SequentialAnimation` block.

---

## Author

**Ehab** — Embedded Software Engineer
ITI 9-Month Professional Diploma — Embedded Systems Track

> Dashboard built with Qt 6.10 / QML as the host-side control interface for a multi-board OTA update pipeline (QNX + Yocto + CommonAPI SOME/IP).

---

*Last updated: May 2026*