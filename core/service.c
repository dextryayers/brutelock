#include "service.h"
#include "adb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void detect_services(DeviceInfo *info) {
    char *tmp;

    tmp = adb_shell("adb shell pm list packages 2>/dev/null | wc -l");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->installed_apps_count, tmp, sizeof(info->installed_apps_count)-1);

    tmp = adb_shell("adb shell dumpsys device_policy 2>/dev/null | grep 'Admin=' | head -10");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->device_admin_apps, tmp, sizeof(info->device_admin_apps)-1);

    tmp = adb_shell("adb shell settings get secure enabled_accessibility_services 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(tmp) < sizeof(info->accessibility_services))
        strncpy(info->accessibility_services, tmp, sizeof(info->accessibility_services)-1);

    tmp = adb_shell("adb shell dumpsys usb 2>/dev/null | grep 'UsbManager' -A 5 | head -10");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        if (strstr(tmp, "MTP") || strstr(tmp, "mtp")) strncpy(info->usb_mode, "MTP", sizeof(info->usb_mode)-1);
        else if (strstr(tmp, "charging")) strncpy(info->usb_mode, "Charging", sizeof(info->usb_mode)-1);
        else if (strstr(tmp, "ADB")) strncpy(info->usb_mode, "ADB", sizeof(info->usb_mode)-1);
        else strncpy(info->usb_mode, tmp, sizeof(info->usb_mode)-1);
    }

    tmp = adb_shell("adb shell dumpsys power 2>/dev/null | grep -E 'mScreenOn|mWakefulness|mScreenBrightness|mDockState' | head -5");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->power_info, tmp, sizeof(info->power_info)-1);

    tmp = adb_shell("adb shell ps -A 2>/dev/null | wc -l");
    trim_newline(tmp);
    if (strlen(tmp) > 0) snprintf(info->running_services, sizeof(info->running_services), "%s processes", tmp);

    tmp = adb_shell("adb shell dumpsys package 2>/dev/null | grep 'Packages:' | awk '{print $2}'");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->package_count, tmp, sizeof(info->package_count)-1);
}

void print_service_info(const DeviceInfo *info) {
    if (strlen(info->installed_apps_count)) printf("  " BOLD "%-16s" RESET ": %s\n", "Installed Apps", info->installed_apps_count);
    if (strlen(info->running_services)) printf("  " BOLD "%-16s" RESET ": %s\n", "Running", info->running_services);
    if (strlen(info->usb_mode)) printf("  " BOLD "%-16s" RESET ": %s\n", "USB Mode", info->usb_mode);
    if (strlen(info->power_info)) {
        printf("  " BOLD "%-16s" RESET ":\n    %s\n", "Power State", info->power_info);
    }
    if (strlen(info->device_admin_apps)) {
        printf("  " BOLD "%-16s" RESET ":\n", "Device Admin");
        char copy[BIG_STR];
        strncpy(copy, info->device_admin_apps, sizeof(copy)-1);
        char *line = strtok(copy, "\n");
        while (line) {
            printf("    %s\n", line);
            line = strtok(NULL, "\n");
        }
    }
}
