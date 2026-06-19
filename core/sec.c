#include "sec.h"
#include "adb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void detect_sec(DeviceInfo *info) {
    char *tmp;

    tmp = adb_shell("adb shell getprop ro.boot.verifiedbootstate 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        strncpy(info->verified_boot, tmp, sizeof(info->verified_boot)-1);
        const char *color = "";
        if (strcmp(tmp, "green") == 0) color = " (locked, verified)";
        else if (strcmp(tmp, "orange") == 0) color = " (unlocked, warning)";
        else if (strcmp(tmp, "red") == 0) color = " (tampered!)";
        char buf[64];
        snprintf(buf, sizeof(buf), "%s%s", tmp, color);
        strncpy(info->verified_boot, buf, sizeof(info->verified_boot)-1);
    }

    tmp = adb_shell("adb shell getprop ro.boot.veritymode 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) snprintf(info->avb_mode, sizeof(info->avb_mode), "verity:%s", tmp);
    else {
        tmp = adb_shell("adb shell getprop ro.boot.vbmeta.avb_version 2>/dev/null");
        trim_newline(tmp);
        if (strlen(tmp) > 0) snprintf(info->avb_mode, sizeof(info->avb_mode), "AVB v%s", tmp);
    }

    tmp = adb_shell("adb shell dumpsys security.keystore 2>/dev/null | grep -iE 'keymaster|version|TEE' | head -3");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->keymaster_ver, tmp, sizeof(info->keymaster_ver)-1);
    else {
        tmp = adb_shell("adb shell getprop ro.vendor.keymaster.version 2>/dev/null");
        trim_newline(tmp);
        if (strlen(tmp) > 0) snprintf(info->keymaster_ver, sizeof(info->keymaster_ver), "Keymaster v%s", tmp);
    }

    tmp = adb_shell("adb shell dumpsys security.gatekeeper 2>/dev/null | grep -iE 'version|gatekeeper' | head -3");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->gatekeeper_ver, tmp, sizeof(info->gatekeeper_ver)-1);

    tmp = adb_shell("adb shell dumpsys security 2>/dev/null | grep -iE 'TEE|trustzone|trusted' | head -3");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->tee, tmp, sizeof(info->tee)-1);
    if (strlen(info->tee) == 0) strncpy(info->tee, "ARM TrustZone (TEE)", sizeof(info->tee)-1);

    tmp = adb_shell("adb shell cat /sys/kernel/security/dimsum/status 2>/dev/null | head -1");
    trim_newline(tmp);
    if (strlen(tmp) > 0) snprintf(info->dmverity_status, sizeof(info->dmverity_status), "%s", tmp);
    else {
        tmp = adb_shell("adb shell getprop ro.boot.dmverity 2>/dev/null");
        trim_newline(tmp);
        if (strlen(tmp) > 0) snprintf(info->dmverity_status, sizeof(info->dmverity_status), "dm-verity:%s", tmp);
    }

    tmp = adb_shell("adb shell getprop ro.boot.flash.locked 2>/dev/null");
    trim_newline(tmp);
    if (strcmp(tmp, "1") == 0) {
        if (strlen(info->bootloader_unlock) == 0) strncpy(info->bootloader_unlock, "Locked", sizeof(info->bootloader_unlock)-1);
    } else if (strcmp(tmp, "0") == 0) {
        strncpy(info->bootloader_unlock, "Unlocked", sizeof(info->bootloader_unlock)-1);
    }

    tmp = adb_shell("adb shell getprop ro.debuggable 2>/dev/null");
    trim_newline(tmp);
    if (strcmp(tmp, "1") == 0) append_str(info->proc_stats, sizeof(info->proc_stats), "Debuggable");
}

void print_sec_info(const DeviceInfo *info) {
    if (strlen(info->tee)) printf("  " BOLD "%-20s" RESET ": %s\n", "TEE", info->tee);
    if (strlen(info->keymaster_ver)) printf("  " BOLD "%-20s" RESET ": %s\n", "Keymaster", info->keymaster_ver);
    if (strlen(info->gatekeeper_ver)) printf("  " BOLD "%-20s" RESET ": %s\n", "Gatekeeper", info->gatekeeper_ver);
    if (strlen(info->verified_boot)) printf("  " BOLD "%-20s" RESET ": %s\n", "Verified Boot", info->verified_boot);
    if (strlen(info->avb_mode)) printf("  " BOLD "%-20s" RESET ": %s\n", "AVB/Verity", info->avb_mode);
    if (strlen(info->dmverity_status)) printf("  " BOLD "%-20s" RESET ": %s\n", "dm-verity", info->dmverity_status);
    if (strlen(info->bootloader_unlock)) printf("  " BOLD "%-20s" RESET ": %s\n", "Bootloader", info->bootloader_unlock);
    if (strlen(info->selinux_context)) printf("  " BOLD "%-20s" RESET ": %s\n", "SELinux", info->selinux_context);
    if (strlen(info->proc_stats)) printf("  " BOLD "%-20s" RESET ": %s\n", "Flags", info->proc_stats);
}
