# BrutePin v1.1 — Pure Native Android PIN Brute Force

```text
  ██████  ██████  ██    ██ ████████ ███████  ██████  ██ ███    ██
  ██   ██ ██   ██ ██    ██    ██    ██      ██       ██ ████   ██
  ██████  ██████  ██    ██    ██    █████   ██   ███ ██ ██ ██  ██
  ██   ██ ██   ██ ██    ██    ██    ██      ██    ██ ██ ██  ██ ██
  ██████  ██   ██  ██████     ██    ███████  ██████  ██ ██   ████
  Pure Native — no ADB binary, direct USB communication
  =================================================================
  Android PIN Brute Force Tool | Author: Aniipid | v1.1
```

**BrutePin** is a complete, standalone Android PIN brute-force toolkit. It implements the **ADB protocol natively in C** — no `adb` binary, no `libusb`, no TCP/IP, no network. Communication happens **directly via USB cable** using Linux `usbdevfs` ioctls (`/dev/bus/usb/`). Written in **C (core) + C++ (AI/strategy) + Python (orchestrator)** with 165+ source files, 17 exploit engines, and real-time screen monitoring.

---

## Table of Contents

- [Features](#features)
- [Architecture Overview](#architecture-overview)
- [Flow Charts](#flow-charts)
- [Prerequisites](#prerequisites)
- [Installation](#installation)
  - [Linux](#linux)
  - [Windows (Cross-Compile)](#windows-cross-compile)
  - [macOS (Cross-Compile)](#macos-cross-compile)
- [Quick Start](#quick-start)
- [Usage & Commands](#usage--commands)
- [How It Works](#how-it-works)
- [Project Structure](#project-structure)
- [Technical Details](#technical-details)
- [Troubleshooting](#troubleshooting)
- [License](#license)

---

## Features

### Core
- **Native ADB Protocol** — implements ADB USB protocol (A_CNXN, A_OPEN, A_OKAY, A_WRTE, A_CLSE) without external `adb` binary
- **No TCP/IP** — pure USB devicefs communication via `/dev/bus/usb/`
- **No libusb** — uses raw `USBDEVFS_CONTROL` and `USBDEVFS_BULK` ioctls
- **Multi-mode Connection** — ADB → USB mode switch → Fastboot → Recovery ADB → AOA HID
- **Command Pipelining** — joins shell commands with `; ` for 10x fewer round-trips
- **RSA Key Injection** — automatic RSA public key push for ADB authorization

### Detection (23-stage deep scan)
- **166 DeviceInfo fields** — vendor, model, chipset, SoC, CPU, GPU, RAM, storage, display, battery, telephony, connectivity, sensors, biometrics, camera, audio, power, security
- **10 unlock detection methods** — mKeyguardShowing, mCurrentFocus, password_type, dumpsys lock_settings, gatekeeper files, mWakefulness, lockout deadline, screenshot, window policy
- **8 database detection layers** — dumpsys lock_settings, settings get, gatekeeper files, ADB cross-check, SQLite extraction, bootloop detection

### Brute Force
- **4-phase smart strategy** — Phase 0: AI Markov chain (256 PINs), Phase 1: Common PINs from breaches (50+), Phase 2: Device-specific (IMEI/serial/phone/DoB), Phase 3: Sequential full range
- **14 brand grid presets** — AOSP, Samsung, Xiaomi, OPPO, Vivo, Huawei, Motorola, Sony, Nokia, ASUS, Tecno, Infinix, Advan, Honor
- **Adaptive timing** — 70/30 weighted moving average, doubles on lockout, 0.95x after 20 consecutive OKs, clamped 1ms–5000ms
- **4 input methods** — Touch /dev/input/event, sendevent, keyevent, GPIO buttons
- **Real-time screen monitor** — background thread polls every 2s with 11 detection methods

### Exploit Engines (17 total)
- **9 C kernel/hardware modules**: kernel_direct, usb_device_raw, touch_fb_exploit, gpio_button_exploit, i2c_bus_exploit, spmi_exploit, devmem_exploit, sysfs_hw_exploit, pinctrl_exploit
- **5 C exploit engines**: usb_gadget_exploit, adb_auth_bypass, fastboot_exploit, usb_stack_exploit, lock_screen_bypass
- **8 C++ orchestration engines**: kernel_exploit_orchestrator, hw_control_expert, gesture_synthesis, pin_entry_expert, soc_exploit_engine, trustzone_pwn, hardware_scanner_deep, auto_pwn_chain

### Other
- **AOA HID fallback** — Android Open Accessory HID keyboard brute works without ADB (Android 3.1+)
- **Direct sendevent** — 10-50x faster than shell `input` command
- **State save/resume** — saves every 100 iterations
- **Cross-platform build** — Linux, Windows (mingw-w64), macOS (osxcross)

---

## Prerequisites

### Hardware
- **USB cable** to connect the target Android device
- **A USB port** on the host machine
- Target device must be powered on (any mode: normal, fastboot, recovery)

### Linux Host
- GCC 8+ or Clang 8+ (for building)
- Linux kernel with `usbdevfs` support (all mainstream kernels)
- Permissions to access `/dev/bus/usb/` (root or udev rule)
- `make`, `git` (for cloning)

### Android Device
- **USB Debugging** enabled (best) — or any of these modes:
  - Fastboot/Bootloader mode
  - Recovery mode (with ADB)
  - Any mode with USB connectivity (for AOA HID)

---

## Installation

### Linux

```bash
# Clone the repository
git clone https://github.com/aniipid/BrutePin.git
cd BrutePin

# Build the C/C++ binary
cd core && make clean && make -j$(nproc)

# Set up udev rules for USB access (requires sudo)
sudo ./connect.sh --udev

# Verify the build
./brutepin --help
```

### Windows (Cross-Compile)

```bash
# From Linux, cross-compile for Windows:
cd core
make windows
# Output: brutepin.exe
```

Requirements: `mingw-w64` (`apt install mingw-w64`)

### macOS (Cross-Compile)

```bash
# From Linux, cross-compile for macOS:
cd core
make macos
# Output: brutepin_macos
```

Requirements: `osxcross` toolchain installed at `~/osxcross/`

---

## Quick Start

```bash
# 1. Connect your Android device via USB

# 2. Run the Python orchestrator:
python3 brute.py

# 3. Or use the C binary directly (with ADB):
./core/brutepin --batch --pin-len 4

# 4. Auto-pwn chain (deep scan + exploit + brute):
./core/brutepin --auto-pwn

# 5. Non-ADB mode (USB-only):
./core/brutepin --conn-mgr
```

---

## Usage & Commands

Run `./core/brutepin --help` for the full command list:

| Command | Description |
|---------|-------------|
| `--batch` | Full auto mode: deep scan, profile, smart brute with progress |
| `--try-pin PIN` | Try a single PIN, detect brand/grid, report result |
| `--enter-pin PIN` | Enter PIN on device without brute (test input methods) |
| `--pin-len N` | Set PIN length (default: 4, max: 8) |
| `--smart` | Enable 4-phase smart strategy (AI + common + device + sequential) |
| `--auto-pwn` | 6-phase auto pwn (scan, exploit, control, bypass, brute, cleanup) |
| `--deep-scan` | Run 23-stage deep scan and show device info |
| `--deep-scan-adb` | Deep scan + C++ hardware scanner + fill all DeviceInfo fields |
| `--conn-mgr` | Connection manager with multi-mode fallback chain |
| `--recovery-info` | Detect recovery mode and extract /data info |
| `--recovery-boot` | Attempt to boot into recovery mode |
| `--hid-brute` | AOA HID keyboard brute (no ADB needed, Android 3.1+) |
| `--gadget-exploit` | Force USB gadget mode for ADB |
| `--bypass-adb` | Run ADB authorization bypass chain (10 methods) |
| `--fb-ex` | Run fastboot exploit (OEM unlock per brand) |
| `--ls-bypass` | Run lock screen bypass chain (22 methods) |
| `--ls-method N` | Run specific lock screen bypass method (0-21) |
| `--hardware-probe` | Probe all hardware subsystems |
| `--data-forensics` | Extract and analyze SQLite data from device |
| `--exploit-chain` | Run full exploit chain (kernel + hardware + bypass) |
| `--neural-gen` | Generate PIN candidates using neural/AI methods |
| `--security-bypass` | Run security subsystem bypass (SELinux/root) |
| `--adb-shell CMD` | Execute ADB shell command and show output |
| `--list-methods` | List all lock screen bypass methods |
| `--list-commands` | Show all available commands |
| `--sql` | Interactive SQL shell for device databases |
| `--save FILE` | Save detected device info to JSON file |
| `--fast` | Fast mode: no deep scan, skip strategy, sequential only |
| `--no-color` | Disable ANSI color output |
| `--no-watch` | Disable background screen monitoring |
| `--resume FILE` | Resume brute from saved state file |
| `--verbose` | Verbose output with debug logging |
| `--help` | Show help message |
| `--version` | Show version information |

---

## How It Works

### Native ADB Protocol

BrutePin implements the ADB protocol from scratch in C without any external dependencies:

1. **USB Discovery** — scans `/dev/bus/usb/` for devices with Android vendor IDs
2. **ADB Handshake** — sends `A_CNXN` (connect) with system identity and features
3. **Authentication** — generates RSA-1024 keypair, signs tokens, sends `A_AUTH` packets
4. **Service Opening** — opens `A_OPEN` for `shell:`, `exec:`, or `sync:` services
5. **Data Transfer** — sends `A_WRTE` / receives `A_OKAY` in a pipelined loop
6. **Command Execution** — sends shell commands through the open service stream

### Detection System

23 consecutive detection stages populate 166+ DeviceInfo fields:
- 15 `getprop` calls for build properties
- Network state (WiFi, IP, MAC)
- Sysfs reads (/proc/cpuinfo, /proc/meminfo)
- Lock settings database (password type, PIN length, lockout)
- Running services, hardware info via dumpsys
- Accounts (Google, Samsung, Xiaomi)
- Telephony (IMEI, SIM, operator, network)
- Boot mode, debug flags, SELinux, dm-verity
- SoC details, display panel, camera, audio
- Power management, battery health
- Connectivity (WiFi, BT, NFC), storage
- Security features (Verified Boot, AVB)
- Sensors, filesystem, bulk getprop

### PIN Entry Methods

1. **Touch Direct** (/dev/input/event*) — fastest (~50/s). Directly writes input_event structures.
2. **sendevent** (ADB shell) — uses Android sendevent tool (~20/s)
3. **keyevent** (ADB shell) — uses input keyevent for digits + ENTER (~5/s)
4. **GPIO Buttons** — simulates physical button presses for locked devices

### Grid Detection

14 brand presets with auto-detection per screen resolution:
AOSP, Samsung, Xiaomi/Redmi/POCO, OPPO/OnePlus/Realme, Vivo/iQOO, Huawei, Motorola, Sony, Nokia, ASUS/ROG, Tecno, Infinix, Advan, Honor

---

## Project Structure

```text
BrutePin/
├── README.md              # Full documentation
├── brute.py               # Python orchestrator
├── connect.sh             # USB udev detection + permission setup
│
├── core/                  # C/C++ core (71 .c + 26 .cpp + 98 .h)
│   ├── Main & Orchestration
│   │   ├── main.c         # Argument parsing, command dispatch
│   │   ├── engine_bridge.c/h  # C++ engine bridge (extern "C")
│   │   └── brute.c/h      # Brute force orchestrator
│   │
│   ├── ADB & USB Core
│   │   ├── adb_native.c/h # Native ADB protocol implementation
│   │   ├── adb.c/h        # ADB shell/service abstraction
│   │   ├── connection_mgr.c/h # Multi-mode USB connection
│   │   ├── adb_auth_bypass.c/h # 10 ADB auth bypass methods
│   │   └── usb_stack_exploit.c/h # USB kernel exploits
│   │
│   ├── Detection
│   │   ├── detect.c/h     # 23-stage detection functions
│   │   ├── deep_scan.c/h  # Deep hardware/software scanner
│   │   ├── database.c/h   # Lock settings database detection
│   │   ├── screen_watch.c/h   # Background screen state monitor
│   │   └── device_profile.cpp/h # Brand auto-detect + grid preset
│   │
│   ├── Exploit Engines
│   │   ├── fastboot_exploit.c/h # Fastboot protocol + OEM unlock
│   │   ├── lock_screen_bypass.c/h # 22 bypass methods
│   │   ├── recovery_exploit.c/h # Recovery mode data extraction
│   │   ├── usb_hid_brute.c/h   # AOA HID keyboard brute
│   │   ├── usb_gadget_exploit.c/h # Gadget mode forcing
│   │   └── kernel_direct.c/h # Direct kernel memory access
│   │
│   ├── Hardware Access
│   │   ├── input_precision.c/h # Accurate touch/sendevent input
│   │   ├── touch_fb_exploit.c/h # Framebuffer + touch injection
│   │   ├── gpio_button_exploit.c/h # GPIO button simulation
│   │   ├── i2c_bus_exploit.c/h # I2C bus register access
│   │   ├── spmi_exploit.c/h # SPMI (Qualcomm PMIC) access
│   │   ├── devmem_exploit.c/h # Physical memory R/W
│   │   ├── sysfs_hw_exploit.c/h # Sysfs hardware control
│   │   └── pinctrl_exploit.c/h # Pin controller access
│   │
│   ├── C++ Expert Engines
│   │   ├── kernel_exploit_orchestrator.cpp/h # Kernel exploit chain
│   │   ├── hw_control_expert.cpp/h # Hardware control abstraction
│   │   ├── gesture_synthesis.cpp/h # Advanced gesture generation
│   │   ├── pin_entry_expert.cpp/h # Expert PIN brute engine
│   │   ├── soc_exploit_engine.cpp/h # SoC-specific exploits
│   │   ├── trustzone_pwn.cpp/h # TrustZone/TEE exploitation
│   │   ├── hardware_scanner_deep.cpp/h # Deep hardware scanner
│   │   └── auto_pwn_chain.cpp/h # 6-phase auto pwn chain
│   │
│   ├── Strategy & Progress
│   │   ├── strategy_engine.cpp/h # 4-phase brute strategy
│   │   ├── adaptive_timer.cpp/h # Dynamic delay with lockout avoid
│   │   ├── progress_engine.cpp/h # ETA, rate, state save/resume
│   │   └── neural_cracker.cpp/h # AI PIN generation (6 methods)
│   │
│   ├── Analysis & Forensics
│   │   ├── hardware_probe.cpp/h # 12-subsystem hardware probe
│   │   ├── data_forensics.cpp/h # SQLite binary parser + PIN recovery
│   │   ├── exploit_chain.cpp/h  # 46+ step exploit chain
│   │   └── security_bypass.cpp/h # 8 kernel exploit methods
│   │
│   └── Build
│       ├── Makefile           # Linux + Windows + macOS targets
│       └── brutepin           # Compiled binary (~1.3MB)
│
├── python/
│   └── brute_py3.py        # Standalone Python alternative
│
└── docs/
    └── images/             # Generated flowchart images
```

---

## Technical Details

### ADB Protocol Implementation

```text
A_CNXN:
  version: 0x01000001 (ADB v1)
  maxdata: 4096 bytes
  identity: "device::" (or "host::")
  payload: "features=...,shell_v2=1,cmd,stat_v2,ls_v2..."

A_AUTH:
  SIGNATURE (RSASSA-PKCS1-v1_5 with SHA256)
  RSAPUBLICKEY (base64-encoded X.509 SubjectPublicKeyInfo)
  Each key is 2048-bit RSA

Shell Service:
  A_OPEN(local_id, "shell:") -> A_OKAY(remote_id)
  A_WRTE(remote_id, "command\n") -> A_OKAY(local_id)
  A_WRTE(local_id, "output...") -> A_OKAY(remote_id)
```

### Brute Force Strategy

```text
Phase 0: AI Markov Chain
  - Markov model trained on breach data
  - Kneser-Ney smoothing for unseen PINs
  - Temperature-adjusted probability sampling
  - Bayesian prior from device context
  - Up to 256 candidates generated

Phase 1: Common PINs (50)
  - 1234, 0000, 1111, 2222, 3333, ...
  - Years (1990-2024), dates (0101-1231)
  - Repeated digits, sequential, patterns
  - Breach frequency sorted

Phase 2: Device-specific
  - Last 6 digits of IMEI
  - Last 6 digits of serial number
  - Last 6 digits of phone number
  - DoB (DDMM, MMDD, YYYY, DDMMYYYY)
  - Device model number variants

Phase 3: Sequential (brute force)
  - 0000 to 9999 (for 4-digit)
  - 000000 to 999999 (for 6-digit)
  - Adaptive skip of already-tested PINs
```

### Input Methods Comparison

| Method | Speed | Reliability | Root | ADB |
|--------|-------|-------------|------|-----|
| Touch /dev/input/event* | ~50/s | High | Yes | Yes |
| sendevent (ADB shell) | ~20/s | Medium | No | Yes |
| keyevent (ADB shell) | ~5/s | Very High | No | Yes |
| GPIO buttons | ~2/s | Low | Yes | Yes |
| AOA HID | ~3/s | High | No | No |

---

## Troubleshooting

### Common Issues

**1. "Permission denied" accessing USB**
```bash
echo 'SUBSYSTEM=="usb", MODE="0666", GROUP="plugdev"' | sudo tee /etc/udev/rules.d/51-android.rules
sudo udevadm control --reload-rules
```

**2. "No device found"**
- Check USB cable connection
- Try different USB port (USB 2.0 recommended)
- Run `lsusb` to verify device is detected
- Run `./core/brutepin --conn-mgr` for debug info

**3. "ADB unauthorized"**
- Device may need RSA key confirmation on screen
- Use `--bypass-adb` flag to attempt auto-authorization
- Or use `--conn-mgr` for non-ADB methods
- Try recovery mode ADB

**4. "Device remains locked after PIN entry"**
- Check lock type: is it a PIN, pattern, or password?
- Try `--try-pin 1234` to verify PIN entry works
- Check screen state: device must be awake
- Use `--ls-bypass --ls-method 0` to test bypass

**5. "Build errors"**
```bash
sudo apt install build-essential libc6-dev linux-headers-$(uname -r)
sudo apt install g++
cd core && make clean && make -j$(nproc)
```

**6. "Segmentation fault"**
- Ensure the binary is run as root if using hardware access
- Try `--no-color` flag
- Try with a different USB port
- Run with `--verbose` for debug output

**7. "Slow PIN entry"**
- Adaptive timer may have detected lockout and doubled delays
- Run with `--fast` flag to disable adaptive timing
- Touch method is fastest (ensure root for /dev/input/event*)

**8. "AOA HID not working"**
- Device must support Android Open Accessory protocol (Android 3.1+)
- Some OEMs disable AOA, try Samsung/Huawei specifics
- Use `--conn-mgr` to verify AOA support

**9. "Can't detect lock state"**
- Some devices hide lock state from ADB
- System uses 10 detection methods in cascade
- If all fail, device is assumed LOCKED

**10. "PIN found but device still locked"**
- Enter the PIN manually on the device
- Some devices need to exit lockout before PIN works
- Wait for lockout countdown (shown as "!" in status)

---

## License

This project is provided for **educational and security research purposes only**. Unauthorized access to devices you do not own is illegal. The author assumes no liability for misuse.

**Use responsibly. Only test on devices you own.**

---

*BrutePin v1.1 — Built with C, C++, and Python for the Android security research community.*
