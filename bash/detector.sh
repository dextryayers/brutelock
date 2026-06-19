#!/bin/bash
# Device detection functions

detect_chipset() {
    local soc
    soc=$(adb shell getprop ro.soc.manufacturer 2>/dev/null)
    [[ -z "$soc" ]] && soc=$(adb shell getprop ro.chipname 2>/dev/null)
    [[ -z "$soc" ]] && soc=$(adb shell getprop ro.board.platform 2>/dev/null)
    [[ -z "$soc" ]] && soc=$(adb shell getprop ro.hardware 2>/dev/null)
    echo "[*] Chipset: ${soc:-unknown}"

    local sl="${soc,,}"
    case "$sl" in
        *mt*|*mediatek*) echo 1; return ;;
        *msm*|*sdm*|*sm*|*qcom*) echo 2; return ;;
        *exynos*) echo 3; return ;;
        *sc*|*spreadtrum*) echo 4; return ;;
        *gs*|*tensor*) echo 5; return ;;
        *kirin*|*hi*) echo 6; return ;;
        *bcm*) echo 7; return ;;
        *rk*|*rockchip*) echo 8; return ;;
        *sun*|*allwinner*) echo 9; return ;;
        *) echo 0; return ;;
    esac
}

detect_device() {
    echo ""
    echo "===================== DEVICE ====================="
    echo "  Vendor     : $(adb shell getprop ro.product.manufacturer 2>/dev/null)"
    echo "  Model      : $(adb shell getprop ro.product.model 2>/dev/null)"
    echo "  Chipset    : $(adb shell getprop ro.soc.manufacturer 2>/dev/null) / $(adb shell getprop ro.chipname 2>/dev/null)"
    echo "  Platform   : $(adb shell getprop ro.board.platform 2>/dev/null)"
    echo "  Android    : $(adb shell getprop ro.build.version.release 2>/dev/null) (SDK $(adb shell getprop ro.build.version.sdk 2>/dev/null))"
    echo "  Patch      : $(adb shell getprop ro.build.version.security_patch 2>/dev/null)"
    echo "  Serial     : $(adb shell getprop ro.serialno 2>/dev/null)"
    echo "  CPU        : $(adb shell getprop ro.product.cpu.abi 2>/dev/null)"
    echo "  Screen     : $(adb shell wm size 2>/dev/null | sed 's/Physical size: //')"
    echo "  Lock       : $(detect_lock)"
    echo "=================================================="
}

detect_lock() {
    local ls
    ls=$(adb shell dumpsys lock_settings 2>/dev/null)
    [[ -z "$ls" ]] && ls=$(adb shell dumpsys locksettings 2>/dev/null)
    local ll="${ls,,}"
    if [[ "$ll" == *"pin"* ]]; then echo "PIN"; return; fi
    if [[ "$ll" == *"password"* ]]; then echo "Password"; return; fi
    if [[ "$ll" == *"pattern"* ]]; then echo "Pattern"; return; fi
    if [[ "$ll" == *"none"* ]]; then echo "None"; return; fi
    echo "PIN"
}

detect_pin_len() {
    local ls
    ls=$(adb shell dumpsys lock_settings 2>/dev/null)
    [[ -z "$ls" ]] && ls=$(adb shell dumpsys locksettings 2>/dev/null)
    local len
    len=$(echo "$ls" | grep -oP 'length\D*(\d+)' | grep -oP '\d+')
    if [[ -n "$len" ]]; then echo "$len"; return; fi
    local qual
    qual=$(echo "$ls" | grep -oP 'quality\D*(\d+)' | grep -oP '\d+')
    case "$qual" in
        131072|196608) echo 4 ;;
        262144|327680|393216) echo 6 ;;
        524288|*) echo 6 ;;
    esac
}
