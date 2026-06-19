#include "filesys.h"
#include "adb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void detect_filesys(DeviceInfo *info) {
    char *tmp, buf[BIG_STR];

    tmp = adb_shell("adb shell cat /sys/class/power_supply/battery/uevent 2>/dev/null | head -20");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        char *p;
        if (strlen(info->batt_cap) == 0) {
            p = strstr(tmp, "POWER_SUPPLY_CAPACITY=");
            if (p) { p += 22; char *q = strchr(p, '\n'); if (q) *q = 0;
                snprintf(info->batt_cap, sizeof(info->batt_cap), "%s mAh (sysfs)", p); }
        }
        if (strlen(info->battery_tech) == 0) {
            p = strstr(tmp, "POWER_SUPPLY_TECHNOLOGY=");
            if (p) { p += 24; char *q = strchr(p, '\n'); if (q) *q = 0;
                strncpy(info->battery_tech, p, sizeof(info->battery_tech)-1); }
        }
        if (strlen(info->battery_voltage) == 0) {
            p = strstr(tmp, "POWER_SUPPLY_VOLTAGE_NOW=");
            if (p) { p += 25; char *q = strchr(p, '\n'); if (q) *q = 0;
                int uv = atoi(p); snprintf(info->battery_voltage, sizeof(info->battery_voltage), "%d mV", uv/1000); }
        }
        if (strlen(info->battery_temp) == 0) {
            p = strstr(tmp, "POWER_SUPPLY_TEMP=");
            if (p) { p += 18; char *q = strchr(p, '\n'); if (q) *q = 0;
                int t = atoi(p); snprintf(info->battery_temp, sizeof(info->battery_temp), "%.1f C", t/10.0); }
        }
        if (strlen(info->battery_health) == 0) {
            p = strstr(tmp, "POWER_SUPPLY_HEALTH=");
            if (p) { p += 20; char *q = strchr(p, '\n'); if (q) *q = 0;
                strncpy(info->battery_health, p, sizeof(info->battery_health)-1); }
        }
        if (strlen(info->charge_type) == 0) {
            p = strstr(tmp, "POWER_SUPPLY_CHARGE_TYPE=");
            if (p) { p += 25; char *q = strchr(p, '\n'); if (q) *q = 0;
                strncpy(info->charge_type, p, sizeof(info->charge_type)-1); }
        }
    }

    tmp = adb_shell("adb shell cat /sys/class/power_supply/usb/uevent 2>/dev/null | head -10");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        char *p = strstr(tmp, "POWER_SUPPLY_VOLTAGE_NOW=");
        if (p) {
            p += 25; char *q = strchr(p, '\n'); if (q) *q = 0;
            int uv = atoi(p);
            if (strlen(info->charge_current) == 0)
                snprintf(info->charge_current, sizeof(info->charge_current), "USB: %d mV", uv/1000);
        }
    }

    tmp = adb_shell("adb shell cat /sys/class/kgsl/kgsl-3d0/gpu_model 2>/dev/null | head -1");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        if (strlen(info->gpu) == 0) strncpy(info->gpu, tmp, sizeof(info->gpu)-1);
        else {
            char full[128];
            snprintf(full, sizeof(full), "%s %s", info->gpu, tmp);
            strncpy(info->gpu, full, sizeof(info->gpu)-1);
        }
    }

    tmp = adb_shell("adb shell cat /sys/class/kgsl/kgsl-3d0/max_gpuclk 2>/dev/null | head -1");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->gpu_freq) == 0) {
        long hz = atol(tmp);
        snprintf(info->gpu_freq, sizeof(info->gpu_freq), "%ld MHz", hz/1000000);
    }

    tmp = adb_shell("adb shell cat /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        long freq = atol(tmp) / 1000;
        if (freq > 0) snprintf(info->cpu_maxfreq, sizeof(info->cpu_maxfreq), "%ld MHz", freq);
    }

    tmp = adb_shell("adb shell cat /sys/devices/system/cpu/possible 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->cpu_cores) == 0) {
        int n = atoi(tmp + 2) + 1;
        if (n > 0) snprintf(info->cpu_cores, sizeof(info->cpu_cores), "%d", n);
    }

    tmp = adb_shell("adb shell cat /proc/version 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->kernel) == 0) {
        char *p = strstr(tmp, "Linux version");
        if (p) { p += 14; char *q = strchr(p, ' ');
            if (q) { *q = 0; strncpy(info->kernel, p, sizeof(info->kernel)-1); } }
    }

    tmp = adb_shell("adb shell cat /sys/class/fingerprint/fp0/vendor 2>/dev/null | head -1");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->fingerprint) == 0) {
        snprintf(info->fingerprint, sizeof(info->fingerprint), "FP: %s", tmp);
    }

    tmp = adb_shell("adb shell ls /sys/class/fingerprint/ 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->fingerprint) == 0) {
        strncpy(info->fingerprint, "Present (sysfs)", sizeof(info->fingerprint)-1);
    }

    tmp = adb_shell("adb shell cat /sys/class/graphics/fb0/name 2>/dev/null | head -1");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->panel_manuf) == 0)
        strncpy(info->panel_manuf, tmp, sizeof(info->panel_manuf)-1);

    tmp = adb_shell("adb shell cat /sys/class/graphics/fb0/modes 2>/dev/null | head -1");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->display_feat) == 0) {
        snprintf(buf, sizeof(buf), "fb modes: %s", tmp);
        strncpy(info->display_feat, buf, sizeof(info->display_feat)-1);
    }

    tmp = adb_shell("adb shell cat /sys/class/drm/card0-DSI-1/status 2>/dev/null | head -1");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strcmp(tmp, "connected") == 0 && strlen(info->panel_type) == 0)
        strncpy(info->panel_type, "DSI (connected)", sizeof(info->panel_type)-1);

    tmp = adb_shell("adb shell for d in /sys/class/video4linux/video*; do cat $d/name 2>/dev/null; done 2>/dev/null | head -5");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->rear_cams) == 0) {
        snprintf(buf, sizeof(buf), "v4l: %s", tmp);
        strncpy(info->rear_cams, buf, sizeof(info->rear_cams)-1);
    }

    tmp = adb_shell("adb shell cat /proc/config.gz 2>/dev/null | head -c 100");
    if (strlen(tmp) == 0) {
        tmp = adb_shell("adb shell ls -la /proc/config.gz 2>/dev/null");
        trim_newline(tmp);
        if (strlen(tmp) > 0) {
            append_str(info->proc_stats, sizeof(info->proc_stats), " /proc/config.gz available");
        }
    }

    tmp = adb_shell("adb shell cat /proc/uptime 2>/dev/null | awk '{print int($1/86400) \"d \" int($1%86400/3600) \"h\"}'");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->power_info) == 0) {
        snprintf(buf, sizeof(buf), "Uptime: %s", tmp);
        strncpy(info->power_info, buf, sizeof(info->power_info)-1);
    }

    tmp = adb_shell("adb shell cat /proc/loadavg 2>/dev/null | head -1");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        char ld[64];
        snprintf(ld, sizeof(ld), " | Load: %s", tmp);
        append_str(info->power_info, sizeof(info->power_info), "%s", ld);
    }
}
