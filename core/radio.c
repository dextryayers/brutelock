#include "radio.h"
#include "adb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void detect_radio(DeviceInfo *info) {
    char *tmp, buf[BIG_STR];

    tmp = adb_shell("adb shell dumpsys wifi 2>/dev/null | grep -iE 'WiFi|version|802.11' | head -3");
    trim_newline(tmp);
    if (strstr(tmp, "802.11ax") || strstr(tmp, "ax")) strncpy(info->wifi_ver, "WiFi 6 (802.11ax)", sizeof(info->wifi_ver)-1);
    else if (strstr(tmp, "802.11ac") || strstr(tmp, "ac")) strncpy(info->wifi_ver, "WiFi 5 (802.11ac)", sizeof(info->wifi_ver)-1);
    else if (strstr(tmp, "802.11be") || strstr(tmp, "be") || strstr(tmp, "7")) strncpy(info->wifi_ver, "WiFi 7 (802.11be)", sizeof(info->wifi_ver)-1);
    else if (strstr(tmp, "802.11n")) strncpy(info->wifi_ver, "WiFi 4 (802.11n)", sizeof(info->wifi_ver)-1);
    if (strlen(info->wifi_ver) == 0) {
        tmp = adb_shell("adb shell getprop ro.vendor.wifi.version 2>/dev/null");
        trim_newline(tmp);
        if (strlen(tmp) > 0) snprintf(info->wifi_ver, sizeof(info->wifi_ver), "v%s", tmp);
    }

    tmp = adb_shell("adb shell dumpsys bluetooth_manager 2>/dev/null | grep -iE 'version|controller' | head -3");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        char *p = strstr(tmp, "version:");
        if (p) { p += 8; while (*p == ' ') p++; char *q = strchr(p, '\n'); if (q) *q = 0; strncpy(info->bt_ver, p, sizeof(info->bt_ver)-1); }
        else strncpy(info->bt_ver, tmp, sizeof(info->bt_ver)-1);
    } else {
        tmp = adb_shell("adb shell getprop ro.vendor.bt.version 2>/dev/null");
        trim_newline(tmp);
        if (strlen(tmp) > 0) snprintf(info->bt_ver, sizeof(info->bt_ver), "BT %s", tmp);
    }

    tmp = adb_shell("adb shell dumpsys location 2>/dev/null | grep -iE 'gps|gnss|glonass|galileo|beidou' | head -5");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        buf[0] = 0;
        if (strstr(tmp, "gps") || strstr(tmp, "GPS")) append_str(buf, sizeof(buf), "GPS ");
        if (strstr(tmp, "glonass") || strstr(tmp, "GLONASS")) append_str(buf, sizeof(buf), "GLONASS ");
        if (strstr(tmp, "galileo") || strstr(tmp, "Galileo")) append_str(buf, sizeof(buf), "Galileo ");
        if (strstr(tmp, "beidou") || strstr(tmp, "BeiDou")) append_str(buf, sizeof(buf), "BeiDou ");
        if (strlen(buf) > 0) strncpy(info->gps_type, buf, sizeof(info->gps_type)-1);
        else strncpy(info->gps_type, tmp, sizeof(info->gps_type)-1);
    }

    tmp = adb_shell("adb shell dumpsys nfc 2>/dev/null | grep -iE 'NFC|SE|secure element|HCE' | head -3");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        buf[0] = 0;
        if (strstr(tmp, "hce") || strstr(tmp, "HCE")) append_str(buf, sizeof(buf), "HCE ");
        if (strstr(tmp, "se") || strstr(tmp, "secure")) append_str(buf, sizeof(buf), "SE ");
        if (strlen(buf) > 0) snprintf(info->nfc_feat, sizeof(info->nfc_feat), "%s(%s)", info->nfc_state, buf);
        else strncpy(info->nfc_feat, info->nfc_state, sizeof(info->nfc_feat)-1);
    }

    tmp = adb_shell("adb shell getprop ro.vendor.usb.version 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) snprintf(info->usb_ver, sizeof(info->usb_ver), "USB %s", tmp);
    else {
        tmp = adb_shell("adb shell getprop sys.usb.controller 2>/dev/null");
        trim_newline(tmp);
        if (strstr(tmp, "mt")) snprintf(info->usb_ver, sizeof(info->usb_ver), "USB 2.0 (MTK)");
        else if (strlen(tmp) > 0) snprintf(info->usb_ver, sizeof(info->usb_ver), "%s", tmp);
    }

    tmp = adb_shell("adb shell dumpsys bluetooth_manager 2>/dev/null | grep -iE 'name:|address:' | head -2");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->bt_info, tmp, sizeof(info->bt_info)-1);
}

void print_radio_info(const DeviceInfo *info) {
    if (strlen(info->wifi_ver)) printf("  " BOLD "%-20s" RESET ": %s\n", "WiFi", info->wifi_ver);
    if (strlen(info->bt_ver)) printf("  " BOLD "%-20s" RESET ": %s\n", "Bluetooth", info->bt_ver);
    if (strlen(info->gps_type)) printf("  " BOLD "%-20s" RESET ": %s\n", "GPS/GNSS", info->gps_type);
    if (strlen(info->nfc_feat)) printf("  " BOLD "%-20s" RESET ": %s\n", "NFC Features", info->nfc_feat);
    else if (strlen(info->nfc_state)) printf("  " BOLD "%-20s" RESET ": %s\n", "NFC", info->nfc_state);
    if (strlen(info->usb_ver)) printf("  " BOLD "%-20s" RESET ": %s\n", "USB Version", info->usb_ver);
    if (strlen(info->bt_info)) printf("  " BOLD "%-20s" RESET ": %s\n", "BT Name/Addr", info->bt_info);
}
