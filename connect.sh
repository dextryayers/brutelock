#!/usr/bin/env bash
# BrutePin Connect — USB detection + udev setup
set -e

CORE_DIR="$(cd "$(dirname "$0")/core" && pwd)"
C_BIN="$CORE_DIR/brutepin"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; BOLD='\033[1m'; NC='\033[0m'

# Android vendor IDs
VENDORS="18d1 04e8 2717 12d1 22d9 2d95 1004 054c 22b8 0421 2a70 0e8d 05c6"

detect_usb_sysfs() {
    local sp="/sys/bus/usb/devices"
    [[ -d "$sp" ]] || return 1
    for name in "$sp"/*; do
        local vid
        vid="$(cat "$name/idVendor" 2>/dev/null || true)"
        for v in $VENDORS; do
            if [[ "$vid" == "$v" ]]; then
                local man prod serial
                man="$(cat "$name/manufacturer" 2>/dev/null || echo Unknown)"
                prod="$(cat "$name/product" 2>/dev/null || echo Device)"
                serial="$(cat "$name/serial" 2>/dev/null || echo "")"
                echo "vid=$vid man=$man prod=$prod serial=$serial"
                return 0
            fi
        done
    done
    return 1
}

detect_usb_lsusb() {
    if command -v lsusb &>/dev/null; then
        local out
        out="$(lsusb 2>/dev/null | grep -i '18d1\|04e8\|2717\|12d1\|22d9\|2d95' | head -1 || true)"
        [[ -n "$out" ]] && { echo "$out"; return 0; }
    fi
    return 1
}

setup_udev() {
    local rule="/etc/udev/rules.d/51-android.rules"
    if [[ -f "$rule" ]]; then
        grep -q "brutepin" "$rule" && return 0
    fi
    [[ $EUID -ne 0 ]] && return 0  # non-root, skip udev
    cat > "$rule" << 'RULES'
# BrutePin: allow raw USB access for Android devices
SUBSYSTEM=="usb", ATTR{idVendor}=="18d1", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="04e8", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="2717", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="12d1", MODE="0666"
RULES
    udevadm control --reload-rules 2>/dev/null || true
    udevadm trigger 2>/dev/null || true
}

build_c_binary() {
    if [[ -x "$C_BIN" ]]; then
        return 0
    fi
    echo "  [!] Building C engine..."
    (cd "$CORE_DIR" && make -j"$(nproc)" 2>&1) || {
        echo "  [-] Build failed."
        return 1
    }
    echo "  [+] Build OK: $C_BIN"
}

wait_for_device() {
    local timeout="${1:-30}"
    local waited=0
    while [[ $waited -lt $timeout ]]; do
        if detect_usb_sysfs &>/dev/null; then
            return 0
        fi
        printf "."
        sleep 2
        waited=$((waited + 2))
    done
    return 1
}

main() {
    # Ensure C binary
    build_c_binary || return 1

    # udev setup
    setup_udev

    # Detect USB
    local device_info
    device_info="$(detect_usb_sysfs)" || device_info="$(detect_usb_lsusb)" || true
    if [[ -z "$device_info" ]]; then
        return 1
    fi

    # Output for Python parsing: "vid=18d1 man=Xiaomi prod=Redmi serial=..."
    echo "USB_DETECTED $device_info"

    # Verify C binary can reach device
    local result
    result="$("$C_BIN" --adb-shell "echo ADB_OK" 2>/dev/null)" || true
    if [[ "$result" == "ADB_OK" ]]; then
        echo "ADB_READY"
        return 0
    fi
    echo "ADB_UNAVAILABLE"
    return 0
}

main "$@"
