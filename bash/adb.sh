#!/bin/bash
# ADB functions

detect_platform() {
    case "$(uname -s)" in Linux*) echo "linux";;
        Darwin*) echo "macos";;
        CYGWIN*|MINGW*|MSYS*) echo "windows";;
        *) echo "unknown";;
    esac
}

adb_setup() {
    local plat
    plat=$(detect_platform)
    echo "[*] Platform: $plat"

    if command -v adb &>/dev/null; then
        adb start-server 2>/dev/null
        return 0
    fi

    echo "[!] ADB not found"
    case "$plat" in
        linux)
            for pkgmgr in "apt install -y adb" "pacman -S --noconfirm android-tools" "dnf install -y android-tools"; do
                sudo $pkgmgr 2>/dev/null && break
            done
            ;;
        macos) echo "  brew install android-platform-tools" ;;
        windows) echo "  winget install Google.PlatformTools" ;;
    esac
    command -v adb &>/dev/null || { echo "[-] ADB required"; exit 1; }
    adb start-server 2>/dev/null
}

adb_check() {
    local state
    state=$(adb get-state 2>/dev/null)
    [[ "$state" == "device" || "$state" == "recovery" ]]
}

adb_wait() {
    echo "[*] Waiting USB"
    while true; do
        adb_check && { echo -e "\n[+] Connected."; return 0; }
        echo -n "."; sleep 2
    done
}

adb_list() {
    adb devices -l 2>/dev/null
}

adb_shell() {
    adb shell "$@" 2>/dev/null
}

adb_enter_pin() {
    local pin="$1"
    adb shell input keyevent 26 2>/dev/null
    sleep 0.3
    adb shell input swipe 300 1000 300 300 2>/dev/null
    sleep 0.3
    adb shell input text "$pin" 2>/dev/null
    adb shell input keyevent 66 2>/dev/null
    sleep 0.3
}

adb_unlocked() {
    local out
    out=$(adb shell dumpsys window 2>/dev/null)
    [[ "$out" == *"mDreamingSleepToken=null"* ]]
}

adb_reboot() {
    adb reboot 2>/dev/null
}
