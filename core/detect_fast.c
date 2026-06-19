#define _GNU_SOURCE
#include "detect_fast.h"
#include "util.h"
#include "detect.h"
#include "hardware.h"
#include "database.h"
#include "allprop.h"
#include "chip.h"
#include <stdio.h>
#include <string.h>

int detect_fast(DeviceInfo *info) {
    memset(info, 0, sizeof(DeviceInfo));
    printf("[*] Fast detection: hw+getprop+db+allprop\n");
    detect_device(info);
    detect_database(info);
    detect_hardware(info);
    detect_allprop(info);
    if (strlen(info->chipset_vendor) == 0 && strlen(info->soc_exact) == 0) {
        detect_chip(info);
    }
    return info->pin_len > 0 ? 1 : 0;
}
