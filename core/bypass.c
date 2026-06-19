#include "bypass.h"
#include "adb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void detect_bypass(DeviceInfo *info) {
    char *tmp, buf[128];
    int sdk = atoi(info->sdk);

    if (info->bypass_list[0]) info->bypass_list[0] = 0;

    if (sdk > 0 && sdk <= 22) {
        append_str(info->bypass_list, sizeof(info->bypass_list),
                   "EmergencyCall bypass (Android 5.x-) ");
        append_str(info->bypass_list, sizeof(info->bypass_list),
                   "[adb shell am start -a android.intent.action.CALL_BUTTON] ");
    }
    if (sdk > 0 && sdk <= 24) {
        append_str(info->bypass_list, sizeof(info->bypass_list),
                   "Camera crash bypass (Android 6-7) ");
        append_str(info->bypass_list, sizeof(info->bypass_list),
                   "[adb shell am start -a android.media.action.STILL_IMAGE_CAMERA] ");
    }
    if (sdk > 0 && sdk >= 24 && sdk <= 28) {
        append_str(info->bypass_list, sizeof(info->bypass_list),
                   "Fingerprint bypass (Android 7-9 PIN reset) ");
    }

    tmp = adb_shell("adb shell getprop ro.boot.bootreason 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        snprintf(buf, sizeof(buf), "Boot reason: %s", tmp);
        append_str(info->bypass_list, sizeof(info->bypass_list), buf);
    }

    tmp = adb_shell("adb shell getprop ro.bootmode 2>/dev/null");
    trim_newline(tmp);
    if (strcmp(tmp, "recovery") == 0) {
        append_str(info->bypass_list, sizeof(info->bypass_list),
                   "Already in recovery mode - ADB may have root ");
    }

    tmp = adb_shell("adb shell getprop ro.debuggable 2>/dev/null");
    trim_newline(tmp);
    if (strcmp(tmp, "1") == 0) {
        append_str(info->bypass_list, sizeof(info->bypass_list),
                   "Root/debuggable build - 'adb root' available ");
    }

    tmp = adb_shell("adb shell which sqlite3 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        append_str(info->bypass_list, sizeof(info->bypass_list),
                   "sqlite3 available - direct DB access ");
    }

    tmp = adb_shell("adb shell \"su -c 'id' 2>/dev/null\"");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strstr(tmp, "uid=0")) {
        append_str(info->bypass_list, sizeof(info->bypass_list),
                   "Root access - full system access ");
    }

    tmp = adb_shell("adb shell getprop ro.oem_unlock_supported 2>/dev/null");
    trim_newline(tmp);
    if (strcmp(tmp, "1") == 0) {
        append_str(info->bypass_list, sizeof(info->bypass_list),
                   "OEM unlock supported - fastboot unlock possible ");
    }

    tmp = adb_shell("adb shell dumpsys package android 2>/dev/null | grep 'com.android.emergency' | head -1");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        append_str(info->bypass_list, sizeof(info->bypass_list),
                   "Emergency app installed - potential vector ");
    }

    if (strlen(info->bypass_list) == 0) {
        strncpy(info->bypass_list, "None identified", sizeof(info->bypass_list)-1);
    }
}

void print_bypass_info(const DeviceInfo *info) {
    if (strlen(info->bypass_list) && strcmp(info->bypass_list, "None identified")) {
        printf("  " BOLD "%-16s" RESET ":\n", "Bypass Vectors");
        char copy[BIG_STR];
        strncpy(copy, info->bypass_list, sizeof(copy)-1);
        char *line = strtok(copy, " ");
        while (line) {
            printf("    %s\n", line);
            line = strtok(NULL, " ");
        }
    } else {
        printf("  " BOLD "%-16s" RESET ": %s\n", "Bypass", info->bypass_list);
    }
}
