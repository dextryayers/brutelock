#include "extra.h"
#include "adb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void detect_extra(DeviceInfo *info) {
    char *tmp;

    tmp = adb_shell("adb shell getprop ro.bootmode 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->boot_mode, tmp, sizeof(info->boot_mode)-1);

    tmp = adb_shell("adb shell getprop ro.build.type 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->build_type, tmp, sizeof(info->build_type)-1);

    tmp = adb_shell("adb shell settings get global development_settings_enabled 2>/dev/null");
    trim_newline(tmp);
    if (strcmp(tmp, "1") == 0) strncpy(info->dev_options, "Enabled", sizeof(info->dev_options)-1);
    else if (strcmp(tmp, "0") == 0) strncpy(info->dev_options, "Disabled", sizeof(info->dev_options)-1);

    tmp = adb_shell("adb shell getprop ro.oem_unlock_supported 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->oem_unlock, tmp, sizeof(info->oem_unlock)-1);

    tmp = adb_shell("adb shell getprop ro.crypto.state 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        strncpy(info->crypto_state, tmp, sizeof(info->crypto_state)-1);
        tmp = adb_shell("adb shell getprop ro.crypto.type 2>/dev/null");
        trim_newline(tmp);
        if (strlen(tmp) > 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%s (%s)", info->crypto_state, tmp);
            strncpy(info->crypto_state, buf, sizeof(info->crypto_state)-1);
        }
    }

    tmp = adb_shell("adb shell getprop ro.boot.verifiedbootstate 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->avb_state, tmp, sizeof(info->avb_state)-1);
    else {
        tmp = adb_shell("adb shell getprop ro.boot.vbmeta.device_state 2>/dev/null");
        trim_newline(tmp);
        if (strlen(tmp) > 0) strncpy(info->avb_state, tmp, sizeof(info->avb_state)-1);
    }

    tmp = adb_shell("adb shell getprop persist.sys.usb.config 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->usb_config, tmp, sizeof(info->usb_config)-1);

    tmp = adb_shell("adb shell getprop ro.adb.secure 2>/dev/null");
    trim_newline(tmp);
    if (strcmp(tmp, "1") == 0) strncpy(info->adb_secure, "Yes (authorized)", sizeof(info->adb_secure)-1);
    else if (strcmp(tmp, "0") == 0) strncpy(info->adb_secure, "No (insecure)", sizeof(info->adb_secure)-1);

    tmp = adb_shell("adb shell cat /proc/self/attr/current 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->selinux_context, tmp, sizeof(info->selinux_context)-1);

    tmp = adb_shell("adb shell getprop persist.sys.locale 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) == 0) {
        tmp = adb_shell("adb shell getprop ro.product.locale 2>/dev/null");
        trim_newline(tmp);
    }
    if (strlen(tmp) > 0) strncpy(info->locale, tmp, sizeof(info->locale)-1);

    tmp = adb_shell("adb shell getprop ro.product.cpu.abilist 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->cpu_abilist, tmp, sizeof(info->cpu_abilist)-1);

    tmp = adb_shell("adb shell settings get system screen_brightness 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        int b = atoi(tmp);
        int pct = b * 100 / 255;
        snprintf(info->brightness, sizeof(info->brightness), "%d%% (%d/255)", pct, b);
    }

    tmp = adb_shell("adb shell settings get global window_animation_scale 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        char buf[128];
        char *ts = tmp;
        char *trans = adb_shell("adb shell settings get global transition_animation_scale 2>/dev/null");
        trim_newline(trans);
        char *anim = adb_shell("adb shell settings get global animator_duration_scale 2>/dev/null");
        trim_newline(anim);
        snprintf(buf, sizeof(buf), "W:%s T:%s A:%s", ts, trans, anim);
        strncpy(info->animation_scale, buf, sizeof(info->animation_scale)-1);
    }
}

void print_extra_info(const DeviceInfo *info) {
    if (strlen(info->boot_mode)) printf("  " BOLD "%-16s" RESET ": %s\n", "Boot Mode", info->boot_mode);
    if (strlen(info->build_type)) printf("  " BOLD "%-16s" RESET ": %s\n", "Build Type", info->build_type);
    if (strlen(info->dev_options)) printf("  " BOLD "%-16s" RESET ": %s\n", "Dev Options", info->dev_options);
    if (strlen(info->oem_unlock)) printf("  " BOLD "%-16s" RESET ": %s\n", "OEM Unlock", info->oem_unlock);
    if (strlen(info->avb_state)) printf("  " BOLD "%-16s" RESET ": %s\n", "AVB State", info->avb_state);
    if (strlen(info->crypto_state)) printf("  " BOLD "%-16s" RESET ": %s\n", "Encryption", info->crypto_state);
    if (strlen(info->usb_config)) printf("  " BOLD "%-16s" RESET ": %s\n", "USB Config", info->usb_config);
    if (strlen(info->adb_secure)) printf("  " BOLD "%-16s" RESET ": %s\n", "ADB Secure", info->adb_secure);
    if (strlen(info->locale)) printf("  " BOLD "%-16s" RESET ": %s\n", "Locale", info->locale);
    if (strlen(info->cpu_abilist)) printf("  " BOLD "%-16s" RESET ": %s\n", "CPU ABIs", info->cpu_abilist);
    if (strlen(info->brightness)) printf("  " BOLD "%-16s" RESET ": %s\n", "Brightness", info->brightness);
    if (strlen(info->animation_scale)) printf("  " BOLD "%-16s" RESET ": %s\n", "Anim Scale", info->animation_scale);
    if (strlen(info->selinux_context)) printf("  " BOLD "%-16s" RESET ": %s\n", "SELinux Ctx", info->selinux_context);
}
