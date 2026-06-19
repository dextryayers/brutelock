#include "sens.h"
#include "adb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void detect_sens(DeviceInfo *info) {
    char *tmp;

    tmp = adb_shell("adb shell dumpsys sensorservice 2>/dev/null | head -40 | grep -iE 'name:|vendor:|type:|maxRange:|power:|resolution:' | head -30");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->sensor_list, tmp, sizeof(info->sensor_list)-1);

    if (strlen(info->sensor_list) == 0) {
        tmp = adb_shell("adb shell dumpsys sensorservice 2>/dev/null | head -40 | grep -i 'Sensor:' | head -20");
        trim_newline(tmp);
        if (strlen(tmp) > 0) strncpy(info->sensor_list, tmp, sizeof(info->sensor_list)-1);
    }

    if (strlen(info->sensor_list) == 0) {
        tmp = adb_shell("adb shell dumpsys sensorservice 2>/dev/null | head -50");
        trim_newline(tmp);
        if (strlen(tmp) > 0) strncpy(info->sensor_list, tmp, sizeof(info->sensor_list)-1);
        else {
            tmp = adb_shell("adb shell getprop ro.hardware.sensors 2>/dev/null");
            trim_newline(tmp);
            if (strlen(tmp) > 0) snprintf(info->sensor_list, sizeof(info->sensor_list), "Sensors: %s", tmp);
        }
    }

    tmp = adb_shell("adb shell getprop ro.vendor.sensors 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->sensor_list) == 0)
        snprintf(info->sensor_list, sizeof(info->sensor_list), "Sensors: %s", tmp);

    if (strlen(info->sensor_list) > 0) {
        int count = 1;
        char *p = info->sensor_list;
        while (*p) { if (*p == '\n') count++; p++; }
        char nbuf[16];
        snprintf(nbuf, sizeof(nbuf), "%d", count);
        strncpy(info->sensors, nbuf, sizeof(info->sensors)-1);
    }

    tmp = adb_shell("adb shell dumpsys sensorservice 2>/dev/null | grep -i 'Fingerprint' | head -1");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->fingerprint) == 0)
        snprintf(info->fingerprint, sizeof(info->fingerprint), "FP sensor: %s", tmp);

    tmp = adb_shell("adb shell cat /proc/device-tree/fingerprint 2>/dev/null | head -1");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->fingerprint) == 0)
        strncpy(info->fingerprint, tmp, sizeof(info->fingerprint)-1);

    tmp = adb_shell("adb shell getprop ro.boot.fingerprint 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->fingerprint) == 0)
        snprintf(info->fingerprint, sizeof(info->fingerprint), "FP in DT: %s", tmp);

    tmp = adb_shell("adb shell ls /proc/device-tree/soc/fpc* 2>/dev/null | head -1");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->fingerprint) == 0)
        strncpy(info->fingerprint, "FPC (DT)", sizeof(info->fingerprint)-1);

    tmp = adb_shell("adb shell ls /proc/device-tree/soc/et* 2>/dev/null | head -1");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->fingerprint) == 0)
        strncpy(info->fingerprint, "EgisTec (DT)", sizeof(info->fingerprint)-1);

    tmp = adb_shell("adb shell ls /proc/device-tree/soc/goodix* 2>/dev/null | head -1");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->fingerprint) == 0)
        strncpy(info->fingerprint, "Goodix (DT)", sizeof(info->fingerprint)-1);

    if (strlen(info->fingerprint) == 0) {
        tmp = adb_shell("adb shell dumpsys fingerprint 2>/dev/null | grep -c 'Enrolled fingerprints' | head -1");
        trim_newline(tmp);
        int n = atoi(tmp);
        if (n > 0) snprintf(info->fingerprint, sizeof(info->fingerprint), "Enrolled: %d fp(s)", n);
    }

    if (strlen(info->face_unlock) == 0) {
        tmp = adb_shell("adb shell dumpsys face 2>/dev/null | grep -c 'Enrolled faces' | head -1");
        trim_newline(tmp);
        int n = atoi(tmp);
        if (n > 0) snprintf(info->face_unlock, sizeof(info->face_unlock), "Enrolled: %d face(s)", n);
    }

    tmp = adb_shell("adb shell dumpsys iris 2>/dev/null | grep -i 'enrolled' | head -1");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        if (strlen(info->biometrics) == 0) strncpy(info->biometrics, tmp, sizeof(info->biometrics)-1);
        else append_str(info->biometrics, sizeof(info->biometrics), " iris:%s", tmp);
    }

    if (strlen(info->sensor_list) > 0 && strlen(info->sensors) == 0)
        strncpy(info->sensors, "detected", sizeof(info->sensors)-1);
}

void print_sens_info(const DeviceInfo *info) {
    if (strlen(info->sensors)) printf("  " BOLD "%-17s" RESET ": %s\n", "Sensors Count", info->sensors);
    if (strlen(info->fingerprint)) printf("  " BOLD "%-17s" RESET ": %s\n", "Fingerprint", info->fingerprint);
    if (strlen(info->face_unlock)) printf("  " BOLD "%-17s" RESET ": %s\n", "Face Unlock", info->face_unlock);
    if (strlen(info->biometrics)) printf("  " BOLD "%-17s" RESET ": %s\n", "Biometrics", info->biometrics);
    if (strlen(info->sensor_list)) {
        printf("  " BOLD "%-17s" RESET ":\n", "Sensor Details");
        char copy[BIG_STR];
        strncpy(copy, info->sensor_list, sizeof(copy)-1);
        char *line = strtok(copy, "\n");
        while (line) {
            if (strlen(line) > 3) printf("    - %s\n", line);
            line = strtok(NULL, "\n");
        }
    }
}
