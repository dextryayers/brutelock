#include "debug.h"
#include "adb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void detect_debug(DeviceInfo *info) {
    char *tmp;

    tmp = adb_shell("adb shell getprop ro.boot.veritymode 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->verity_mode, tmp, sizeof(info->verity_mode)-1);
    else {
        tmp = adb_shell("adb shell getprop ro.boot.verifiedbootstate 2>/dev/null");
        trim_newline(tmp);
        if (strlen(tmp) > 0) snprintf(info->verity_mode, sizeof(info->verity_mode), "vbmeta:%s", tmp);
    }

    tmp = adb_shell("adb shell getprop ro.boot.verifiedbootstate 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->verified_boot, tmp, sizeof(info->verified_boot)-1);

    tmp = adb_shell("adb shell getprop ro.boot.untrustedboot 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        if (strcmp(tmp, "1") == 0) strncpy(info->bootloader_unlock, "Unlocked", sizeof(info->bootloader_unlock)-1);
        else strncpy(info->bootloader_unlock, "Unknown", sizeof(info->bootloader_unlock)-1);
    } else {
        tmp = adb_shell("adb shell getprop ro.boot.flash.locked 2>/dev/null");
        trim_newline(tmp);
        if (strcmp(tmp, "1") == 0) strncpy(info->bootloader_unlock, "Locked", sizeof(info->bootloader_unlock)-1);
        else if (strcmp(tmp, "0") == 0) strncpy(info->bootloader_unlock, "Unlocked", sizeof(info->bootloader_unlock)-1);
    }

    tmp = adb_shell("adb shell cat /proc/last_kmsg 2>/dev/null | tail -20");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->last_kmsg, tmp, sizeof(info->last_kmsg)-1);

    tmp = adb_shell("adb shell dumpsys dropbox 2>/dev/null | grep -E 'data_app_crash|crash|system_app_crash' | tail -5");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->recent_crashes, tmp, sizeof(info->recent_crashes)-1);

    tmp = adb_shell("adb shell dumpsys thermalservice 2>/dev/null | head -15");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->thermal_info, tmp, sizeof(info->thermal_info)-1);

    tmp = adb_shell("adb shell dumpsys batterystats 2>/dev/null | head -10");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->battery_stats, tmp, sizeof(info->battery_stats)-1);

    tmp = adb_shell("adb shell settings get global stay_on_while_plugged_in 2>/dev/null");
    trim_newline(tmp);
    if (strcmp(tmp, "1") == 0) append_str(info->proc_stats, sizeof(info->proc_stats), "StayAwake(AC) ");
    else if (strcmp(tmp, "2") == 0) append_str(info->proc_stats, sizeof(info->proc_stats), "StayAwake(USB) ");
    else if (strcmp(tmp, "3") == 0) append_str(info->proc_stats, sizeof(info->proc_stats), "StayAwake(AC+USB) ");
    else if (strcmp(tmp, "4") == 0) append_str(info->proc_stats, sizeof(info->proc_stats), "StayAwake(Wireless) ");
    else if (strcmp(tmp, "7") == 0) append_str(info->proc_stats, sizeof(info->proc_stats), "StayAwake(All) ");

    tmp = adb_shell("adb shell getprop ro.debuggable 2>/dev/null");
    trim_newline(tmp);
    if (strcmp(tmp, "1") == 0) append_str(info->proc_stats, sizeof(info->proc_stats), "Debuggable");
}

void print_debug_info(const DeviceInfo *info) {
    if (strlen(info->verity_mode)) printf("  " BOLD "%-16s" RESET ": %s\n", "Verity Mode", info->verity_mode);
    if (strlen(info->verified_boot)) printf("  " BOLD "%-16s" RESET ": %s\n", "Verified Boot", info->verified_boot);
    if (strlen(info->bootloader_unlock)) printf("  " BOLD "%-16s" RESET ": %s\n", "Bootloader", info->bootloader_unlock);
    if (strlen(info->proc_stats)) printf("  " BOLD "%-16s" RESET ": %s\n", "Debug Flags", info->proc_stats);
    if (strlen(info->last_kmsg)) {
        printf("  " BOLD "%-16s" RESET ":\n", "Last Kernel Msg");
        char copy[BIG_STR];
        strncpy(copy, info->last_kmsg, sizeof(copy)-1);
        char *line = strtok(copy, "\n");
        while (line) {
            if (strlen(line) > 0) printf("    %s\n", line);
            line = strtok(NULL, "\n");
        }
    }
    if (strlen(info->recent_crashes)) {
        printf("  " BOLD "%-16s" RESET ":\n", "Recent Crashes");
        printf("    %s\n", info->recent_crashes);
    }
    if (strlen(info->thermal_info)) {
        printf("  " BOLD "%-16s" RESET ":\n", "Thermal");
        char copy[BIG_STR];
        strncpy(copy, info->thermal_info, sizeof(copy)-1);
        char *line = strtok(copy, "\n");
        while (line) {
            if (strlen(line) > 0) printf("    %s\n", line);
            line = strtok(NULL, "\n");
        }
    }
}
