#include "pwr.h"
#include "adb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void detect_pwr(DeviceInfo *info) {
    char *tmp;

    tmp = adb_shell("adb shell dumpsys battery 2>/dev/null | head -20");
    trim_newline(tmp);
    if (strlen(tmp) == 0) tmp = adb_shell("adb shell cat /sys/class/power_supply/battery/uevent 2>/dev/null | head -20");

    if (strlen(tmp) > 0) {
        char *p;
        if (strlen(info->batt_cap) == 0) {
            p = strstr(tmp, "capacity");
            if (!p) p = strstr(tmp, "CAPACITY");
            if (p) {
                int cap = 0; sscanf(p, "%*[^=]=%d", &cap);
                snprintf(info->batt_cap, sizeof(info->batt_cap), "%d%%", cap);
                if (strlen(info->battery_level) == 0)
                    snprintf(info->battery_level, sizeof(info->battery_level)-1, "%d", cap);
            }
        }

        if (strlen(info->battery_status) == 0) {
            p = strstr(tmp, "status");
            if (!p) p = strstr(tmp, "STATUS");
            if (p) {
                int st = 0; sscanf(p, "%*[^=]=%d", &st);
                char *stl[] = {"Unknown", "Charging", "Discharging", "Not charging", "Full"};
                if (st >= 1 && st <= 4) strncpy(info->battery_status, stl[st], sizeof(info->battery_status)-1);
                else if (st > 0) snprintf(info->battery_status, sizeof(info->battery_status), "code=%d", st);
            }
        }

        if (strlen(info->battery_tech) == 0) {
            p = strstr(tmp, "technology");
            if (!p) p = strstr(tmp, "TECHNOLOGY");
            if (p) {
                char *q = strchr(p, '='); if (q) q++; while (*q == ' ') q++;
                char *r = strchr(q, '\n'); if (r) *r = 0;
                strncpy(info->battery_tech, q, sizeof(info->battery_tech)-1);
            }
        }

        if (strlen(info->charge_type) == 0) {
            p = strstr(tmp, "USB");
            if (p && strlen(info->charge_type) == 0) {
                int usb = 0; sscanf(p, "%*[^=]=%d", &usb);
                snprintf(info->charge_type, sizeof(info->charge_type), "USB=%d", usb);
            }
        }

        if (strlen(info->charge_type) == 0) {
            p = strstr(tmp, "AC");
            if (p) {
                int ac = 0; sscanf(p, "%*[^=]=%d", &ac);
                if (strlen(info->charge_type) == 0)
                    snprintf(info->charge_type, sizeof(info->charge_type), "AC=%d", ac);
            }
        }

        if (strlen(info->charge_type) == 0) {
            p = strstr(tmp, "power_supply");
            if (!p) p = strstr(tmp, "POWER_SUPPLY");
            if (strlen(info->charge_type) == 0 && p) {
                char *q = strstr(tmp, "CHARGE_TYPE");
                if (q) {
                    char *v = strchr(q, '='); if (v) v++;
                    char *r = strchr(v, '\n'); if (r) *r = 0;
                    strncpy(info->charge_type, v, sizeof(info->charge_type)-1);
                }
            }
        }

        if (strlen(info->battery_health) == 0) {
            p = strstr(tmp, "health");
            if (!p) p = strstr(tmp, "HEALTH");
            if (p) {
                int h = 0; sscanf(p, "%*[^=]=%d", &h);
                char *hl[] = {"Unknown", "Good", "Overheat", "Dead", "OverVoltage", "Unspecified", "Cold"};
                if (h >= 1 && h <= 6) strncpy(info->battery_health, hl[h], sizeof(info->battery_health)-1);
                else if (h > 0) snprintf(info->battery_health, sizeof(info->battery_health), "code=%d", h);
            }
        }

        if (strlen(info->battery_temp) == 0) {
            p = strstr(tmp, "temperature");
            if (!p) p = strstr(tmp, "TEMP");
            if (p) {
                int t = 0; sscanf(p, "%*[^=]=%d", &t);
                snprintf(info->battery_temp, sizeof(info->battery_temp), "%.1f C", t/10.0);
            }
        }
    }

    if (strlen(info->charge_type) == 0) {
        tmp = adb_shell("adb shell cat /sys/class/power_supply/battery/charge_type 2>/dev/null");
        trim_newline(tmp);
        if (strlen(tmp) > 0) strncpy(info->charge_type, tmp, sizeof(info->charge_type)-1);
    }

    tmp = adb_shell("adb shell cat /sys/class/power_supply/battery/charge_counter 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->batt_cap) == 0)
        snprintf(info->batt_cap, sizeof(info->batt_cap), "%s mAh", tmp);

    tmp = adb_shell("adb shell cat /sys/class/power_supply/battery/current_now 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->charge_current) == 0) {
        int ua = atoi(tmp);
        snprintf(info->charge_current, sizeof(info->charge_current), "%d mA", ua/1000);
    }

    tmp = adb_shell("adb shell cat /sys/class/power_supply/battery/voltage_now 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->battery_voltage) == 0) {
        int uv = atoi(tmp);
        snprintf(info->battery_voltage, sizeof(info->battery_voltage), "%d mV", uv/1000);
    }

    tmp = adb_shell("adb shell ls /sys/class/power_supply/wireless/ 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strcmp(tmp, "No such file or directory") != 0)
        append_str(info->charge_type, sizeof(info->charge_type), " wireless");

    tmp = adb_shell("adb shell getprop ro.product.charger 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) append_str(info->charge_type, sizeof(info->charge_type), " [%s]", tmp);

    tmp = adb_shell("adb shell cat /sys/class/power_supply/battery/status 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->power_info) == 0)
        strncpy(info->power_info, tmp, sizeof(info->power_info)-1);

    if (strlen(info->batt_cap) > 0 && strlen(info->battery_tech) == 0)
        strncpy(info->battery_tech, "Li-ion (by default)", sizeof(info->battery_tech)-1);

    if (strlen(info->wireless_charge) == 0) {
        tmp = adb_shell("adb shell ls /sys/class/power_supply/wireless/ 2>/dev/null");
        trim_newline(tmp);
        if (strlen(tmp) > 0 && strcmp(tmp, "No such file or directory") != 0)
            strncpy(info->wireless_charge, "Supported", sizeof(info->wireless_charge)-1);
        else {
            tmp = adb_shell("adb shell dumpsys battery 2>/dev/null | grep 'Wireless' | head -1");
            trim_newline(tmp);
            if (strstr(tmp, "1")) strncpy(info->wireless_charge, "Supported", sizeof(info->wireless_charge)-1);
        }
    }
}

void print_pwr_info(const DeviceInfo *info) {
    if (strlen(info->batt_cap)) printf("  " BOLD "%-17s" RESET ": %s\n", "Battery", info->batt_cap);
    if (strlen(info->battery_health)) printf("  " BOLD "%-17s" RESET ": %s\n", "Health", info->battery_health);
    if (strlen(info->battery_temp)) printf("  " BOLD "%-17s" RESET ": %s\n", "Temp", info->battery_temp);
    if (strlen(info->battery_voltage)) printf("  " BOLD "%-17s" RESET ": %s\n", "Voltage", info->battery_voltage);
    if (strlen(info->battery_tech)) printf("  " BOLD "%-17s" RESET ": %s\n", "Technology", info->battery_tech);
    if (strlen(info->charge_type)) printf("  " BOLD "%-17s" RESET ": %s\n", "Charge", info->charge_type);
    if (strlen(info->charge_current)) printf("  " BOLD "%-17s" RESET ": %s\n", "Current", info->charge_current);
    if (strlen(info->power_info)) printf("  " BOLD "%-17s" RESET ": %s\n", "Power State", info->power_info);
    if (strlen(info->wireless_charge)) printf("  " BOLD "%-17s" RESET ": %s\n", "Wireless Charging", info->wireless_charge);
}
