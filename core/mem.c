#include "mem.h"
#include "adb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void detect_mem(DeviceInfo *info) {
    char *tmp;

    tmp = adb_shell("adb shell dumpsys meminfo 2>/dev/null | grep -iE 'RAM:|LPDDR|DDR' | head -3");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->ram_type, tmp, sizeof(info->ram_type)-1);
    else {
        tmp = adb_shell("adb shell getprop ro.vendor.ram.type 2>/dev/null");
        trim_newline(tmp);
        if (strlen(tmp) > 0) snprintf(info->ram_type, sizeof(info->ram_type), "LPDDR%s", tmp);
        else {
            tmp = adb_shell("adb shell dumpsys meminfo 2>/dev/null | grep 'Total RAM' | head -1");
            trim_newline(tmp);
            if (strlen(tmp) > 0) {
                char *p = strstr(tmp, "RAM:");
                if (p) {
                    p += 4; while (*p == ' ') p++;
                    char *q = strchr(p, '\n');
                    if (q) *q = 0;
                    snprintf(info->ram_type, sizeof(info->ram_type), "%s (size)", p);
                }
            }
        }
    }

    tmp = adb_shell("adb shell df -h /data 2>/dev/null | tail -1 | awk '{print $2}'");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        tmp = adb_shell("adb shell mount 2>/dev/null | grep -E ' /data ' | head -1");
        trim_newline(tmp);
        if (strstr(tmp, "ext4")) strncpy(info->storage_type, "ext4", sizeof(info->storage_type)-1);
        else if (strstr(tmp, "f2fs")) strncpy(info->storage_type, "f2fs", sizeof(info->storage_type)-1);
    }

    tmp = adb_shell("adb shell dumpsys storaged 2>/dev/null | grep -iE 'emmc|ufs|flash|type' | head -3");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        if (strlen(info->storage_type) > 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%s | %s", info->storage_type, tmp);
            strncpy(info->storage_type, buf, sizeof(info->storage_type)-1);
        } else strncpy(info->storage_type, tmp, sizeof(info->storage_type)-1);
    }

    tmp = adb_shell("adb shell getprop ro.vendor.storage.type 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        if (strlen(info->storage_type) > 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%s (prop:%s)", info->storage_type, tmp);
            strncpy(info->storage_type, buf, sizeof(info->storage_type)-1);
        } else snprintf(info->storage_type, sizeof(info->storage_type), "UFS %s", tmp);
    }

    tmp = adb_shell("adb shell cat /proc/swaps 2>/dev/null | grep -v Filename | head -1 | awk '{print $1}'");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        tmp = adb_shell("adb shell cat /proc/swaps 2>/dev/null | grep -v Filename | awk '{sum+=$3} END {print sum}'");
        trim_newline(tmp);
        long sz = atol(tmp);
        if (sz > 0) snprintf(info->swap_total, sizeof(info->swap_total), "%ld KB swap", sz);
    }

    tmp = adb_shell("adb shell ls /sdcard 2>/dev/null | head -1");
    if (strlen(tmp) > 0) strncpy(info->sdcard, "Available (/sdcard)", sizeof(info->sdcard)-1);
    else strncpy(info->sdcard, "Not accessible", sizeof(info->sdcard)-1);

    tmp = adb_shell("adb shell sm list-disks 2>/dev/null | head -3");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%s | %s", info->sdcard, tmp);
        strncpy(info->sdcard, buf, sizeof(info->sdcard)-1);
    }
}

void print_mem_info(const DeviceInfo *info) {
    if (strlen(info->ram_type)) printf("  " BOLD "%-20s" RESET ": %s\n", "RAM Type", info->ram_type);
    if (strlen(info->storage_type)) printf("  " BOLD "%-20s" RESET ": %s\n", "Storage Type", info->storage_type);
    if (strlen(info->storage_total)) printf("  " BOLD "%-20s" RESET ": %s\n", "Storage Usage", info->storage_total);
    if (strlen(info->swap_total)) printf("  " BOLD "%-20s" RESET ": %s\n", "Swap", info->swap_total);
    if (strlen(info->sdcard)) printf("  " BOLD "%-20s" RESET ": %s\n", "SD Card", info->sdcard);
}
