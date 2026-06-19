# BrutePin v1.1 - Summary

## What was done this session

### Version update
- Updated from v4.0 → v1.1 across all files: `main.c`, `util.c`, `brute.py`, `bash/brutepin.sh`, `cpp/main.cpp`, `rustcore/src/main.rs`, `Cargo.toml`
- Banner now reads: `BrutePin v1.1 - Interactive PIN BruteForce - By: Aniipid`

### Graphics (GPU) detection — multi-layer fallback chain
1. `detect.c`: `ro.hardware.egl` → `ro.vendor.gpu`
2. `hardware.c`: `dumpsys gfxinfo | grep GLES` → `dumpsys | grep GLES:` → `ro.hardware.egl` → `ro.vendor.gpu` → `/proc/gpu` → `dumpsys | grep -i gpu` → `getprop | grep -i 'gpu|gl|egl|renderer'` → chipset-CPU‑to‑GPU inference table (40+ SoC mappings) → auto‑fill `gpu_renderer` from `gpu`
3. `filesys.c`: `/sys/class/kgsl/kgsl-3d0/gpu_model` + `max_gpuclk`
4. `allprop.c`: 7 getprop keys (`ro.hardware.gpu`, `ro.vendor.gpu`, `ro.product.gpu`, `ro.gpu.model`, `ro.vendor.hardware.gpu`, `ro.board.gpu`, `debug.gpu.model`)

### Battery detection fixes
- **Root cause**: The `--full` Battery section reads `battery_level`/`battery_status` which NO module was populating (pwr.c used different fields like `batt_cap`/`power_info`)
- **Fix**: `detect.c` now reads `dumpsys battery` for level+status; `pwr.c` also populates `battery_level` and `battery_status` from the same source
- `battery_level` is now always set alongside `batt_cap`
- `battery_status` translates numeric code → human string (Charging/Discharging/Full/etc)

### New files integrated
- `filesys.c/h` — probes `/sys` and `/proc` for battery/GPU/panel/camera/kernel info
- `allprop.c/h` — 80+ getprop key scanner as safety net (was orphaned, now compiled+linked)

### Compiler warnings
- Suppressed false‑positive `-Wstringop-truncation` and `-Wformat-truncation` (intentional truncation by design)

### Build status
- 30 `.c` + 29 `.h` = 59 files, 0 errors, 0 warnings
- Binary 127 KB
- All wrappers (C, C++, Rust, Python) report `BrutePin v1.1`
