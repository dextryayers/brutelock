#include "devices.h"
#include "adb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void detect_devices(DeviceInfo *info) {
    char *tmp;

    tmp = adb_shell("adb shell cat /proc/bus/input/devices 2>/dev/null | grep -E 'N:|H:' | head -20");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->input_devices, tmp, sizeof(info->input_devices)-1);

    tmp = adb_shell("adb shell getevent -p 2>/dev/null | grep -B1 -i 'touch' | head -10");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        char *p = strstr(tmp, "name:");
        if (p) {
            char *q = strchr(p, '"');
            if (q) {
                q++;
                char *r = strchr(q, '"');
                if (r) { *r = 0; strncpy(info->touchscreen, q, sizeof(info->touchscreen)-1); }
            }
        }
    }
    if (strlen(info->touchscreen) == 0) {
        tmp = adb_shell("adb shell dumpsys input 2>/dev/null | grep -i 'touch' | head -3");
        trim_newline(tmp);
        if (strlen(tmp) > 0) strncpy(info->touchscreen, tmp, sizeof(info->touchscreen)-1);
    }

    tmp = adb_shell("adb shell dumpsys nfc 2>/dev/null | grep -E 'mState|mScreenState|isEnabled' | head -5");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->nfc_state, tmp, sizeof(info->nfc_state)-1);

    tmp = adb_shell("adb shell dumpsys bluetooth_manager 2>/dev/null | grep -E 'name:|address:|state:|enabled' | head -5");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->bt_info, tmp, sizeof(info->bt_info)-1);

    tmp = adb_shell("adb shell dumpsys sensorservice 2>/dev/null | head -40");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->sensors_detail, tmp, sizeof(info->sensors_detail)-1);

    tmp = adb_shell("adb shell dumpsys media.radio 2>/dev/null | grep -E 'isAvailable|state|enabled' | head -5");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->fm_radio, tmp, sizeof(info->fm_radio)-1);
    if (strlen(info->fm_radio) == 0 && strlen(info->sensors_detail) > 0) {
        if (strstr(info->sensors_detail, "FM")) strncpy(info->fm_radio, "Available (via sensor)", sizeof(info->fm_radio)-1);
    }

    tmp = adb_shell("adb shell dumpsys ConsumerIr 2>/dev/null | grep -i 'hasIrEmitter' | head -1");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strstr(tmp, "true")) strncpy(info->ir_blaster, "Yes", sizeof(info->ir_blaster)-1);
    else if (strlen(tmp) > 0 && strstr(tmp, "false")) strncpy(info->ir_blaster, "No", sizeof(info->ir_blaster)-1);
    else {
        tmp = adb_shell("adb shell getprop ro.vendor.api.hardware.ir 2>/dev/null");
        trim_newline(tmp);
        if (strlen(tmp) > 0) strncpy(info->ir_blaster, "Yes", sizeof(info->ir_blaster)-1);
    }

    tmp = adb_shell("adb shell getevent -p 2>/dev/null | grep -i -E 'stylus|pen|wacom' | head -3");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->stylus, "Present", sizeof(info->stylus)-1);

    tmp = adb_shell("adb shell dumpsys battery 2>/dev/null | grep -E 'technology|temperature|voltage' | head -5");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        char *p = strstr(tmp, "technology:");
        if (p) { p += 11; while (*p == ' ') p++; char *q = strchr(p, '\n'); if (q) *q = 0; strncpy(info->battery_tech, p, sizeof(info->battery_tech)-1); }
        p = strstr(tmp, "temperature:");
        if (p) { p += 12; while (*p == ' ') p++; char *q = strchr(p, '\n'); if (q) *q = 0; int t = atoi(p); snprintf(info->battery_temp, sizeof(info->battery_temp), "%.1f C", t / 10.0); }
        p = strstr(tmp, "voltage:");
        if (p) { p += 8; while (*p == ' ') p++; char *q = strchr(p, '\n'); if (q) *q = 0; int v = atoi(p); snprintf(info->battery_voltage, sizeof(info->battery_voltage), "%d mV", v); }
        p = strstr(tmp, "health:");
        if (p) { p += 7; while (*p == ' ') p++; char *q = strchr(p, '\n'); if (q) *q = 0; strncpy(info->battery_health, p, sizeof(info->battery_health)-1); }
    }
}

void print_devices_info(const DeviceInfo *info) {
    if (strlen(info->touchscreen)) printf("  " BOLD "%-16s" RESET ": %s\n", "Touchscreen", info->touchscreen);
    if (strlen(info->nfc_state)) printf("  " BOLD "%-16s" RESET ": %s\n", "NFC", info->nfc_state);
    if (strlen(info->bt_info)) printf("  " BOLD "%-16s" RESET ": %s\n", "Bluetooth", info->bt_info);
    if (strlen(info->fm_radio)) printf("  " BOLD "%-16s" RESET ": %s\n", "FM Radio", info->fm_radio);
    if (strlen(info->ir_blaster)) printf("  " BOLD "%-16s" RESET ": %s\n", "IR Blaster", info->ir_blaster);
    if (strlen(info->stylus)) printf("  " BOLD "%-16s" RESET ": %s\n", "Stylus", info->stylus);
    if (strlen(info->battery_tech)) printf("  " BOLD "%-16s" RESET ": %s\n", "Battery Type", info->battery_tech);
    if (strlen(info->battery_temp)) printf("  " BOLD "%-16s" RESET ": %s\n", "Battery Temp", info->battery_temp);
    if (strlen(info->battery_voltage)) printf("  " BOLD "%-16s" RESET ": %s\n", "Battery Volt", info->battery_voltage);
    if (strlen(info->battery_health)) printf("  " BOLD "%-16s" RESET ": %s\n", "Battery Health", info->battery_health);
    if (strlen(info->input_devices)) {
        printf("  " BOLD "%-16s" RESET ":\n", "Input Devices");
        char copy[BIG_STR];
        strncpy(copy, info->input_devices, sizeof(copy)-1);
        char *line = strtok(copy, "\n");
        while (line) {
            if (strlen(line) > 0) printf("    %s\n", line);
            line = strtok(NULL, "\n");
        }
    }
}
