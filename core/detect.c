#include "detect.h"
#include "adb.h"
#include "fastboot.h"
#include "usb_raw.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* ── ADB-based detection helpers ── */

static void getprop(const char *prop, char *dst, size_t dst_size) {
    char *val = adb_getprop(prop);
    if (val) {
        trim_newline(val);
        char tmp[MAX_STR];
        normalize_str(tmp, val);
        strncpy(dst, tmp, dst_size - 1);
        dst[dst_size - 1] = 0;
    } else {
        dst[0] = 0;
    }
}

/* ── Chipset detection ── */

ChipsetFamily detect_chipset(DeviceInfo *info) {
    char raw[512] = {0};
    const char *keys[] = {
        "ro.soc.manufacturer", "ro.chipset.name", "ro.chipname",
        "ro.board.platform", "ro.hardware", "ro.product.board",
        "ro.mediatek.platform", NULL
    };
    for (int i = 0; keys[i]; i++) {
        char *v = adb_getprop(keys[i]);
        if (v) { trim_newline(v); normalize_str(raw, v); }
        if (strlen(raw) > 0 && strcmp(raw, "unknown") && strcmp(raw, "(unknown)")) break;
    }
    if (strlen(raw) == 0) {
        /* Fallback: parse /proc/cpuinfo */
        char *cpuinfo = adb_shell("cat /proc/cpuinfo 2>/dev/null | grep -i Hardware | head -1");
        trim_newline(cpuinfo);
        if (strlen(cpuinfo) > 0) {
            char *p = strchr(cpuinfo, ':');
            if (p) {
                p++; while (*p == ' ') p++;
                normalize_str(raw, p);
            }
        }
    }
    if (strlen(raw) == 0) strcpy(raw, "generic");
    snprintf(info->chipset_name, sizeof(info->chipset_name), "%s", raw);

    char low[512];
    for (int i = 0; raw[i]; i++) low[i] = tolower((unsigned char)raw[i]);
    low[strlen(raw)] = 0;

    ChipsetFamily fam = CHIP_UNKNOWN;
    if (strstr(low, "mt") || strstr(low, "mediatek")) fam = CHIP_MEDIATEK;
    else if (strstr(low, "qcom") || strstr(low, "msm") || strstr(low, "sdm") || strstr(low, "sm") || strstr(low, "snapdragon")) fam = CHIP_QUALCOMM;
    else if (strstr(low, "exynos")) fam = CHIP_EXYNOS;
    else if (strstr(low, "sc") || strstr(low, "spreadtrum") || strstr(low, "unisoc")) fam = CHIP_SPREADTRUM;
    else if (strstr(low, "gs") || strstr(low, "tensor")) fam = CHIP_TENSOR;
    else if (strstr(low, "kirin") || strstr(low, "hi")) fam = CHIP_KIRIN;
    else if (strstr(low, "bcm") || strstr(low, "broadcom")) fam = CHIP_BROADCOM;
    else if (strstr(low, "rk") || strstr(low, "rockchip")) fam = CHIP_ROCKCHIP;
    else if (strstr(low, "sun") || strstr(low, "allwinner")) fam = CHIP_ALLWINNER;
    else if (strstr(low, "intel") || strstr(low, "atom")) fam = CHIP_INTEL;
    else if (strstr(low, "nvidia") || strstr(low, "tegra")) fam = CHIP_NVIDIA;

    const char *names[] = {
        [CHIP_UNKNOWN] = "Generic",
        [CHIP_MEDIATEK] = "MediaTek",
        [CHIP_QUALCOMM] = "Qualcomm",
        [CHIP_EXYNOS] = "Exynos",
        [CHIP_SPREADTRUM] = "Spreadtrum",
        [CHIP_TENSOR] = "Tensor",
        [CHIP_KIRIN] = "Kirin",
        [CHIP_BROADCOM] = "Broadcom",
        [CHIP_ROCKCHIP] = "Rockchip",
        [CHIP_ALLWINNER] = "Allwinner",
        [CHIP_INTEL] = "Intel",
        [CHIP_NVIDIA] = "NVIDIA"
    };
    strncpy(info->chipset_vendor, names[fam], sizeof(info->chipset_vendor)-1);
    return fam;
}

/* ── Standard ADB-based device detection (does NOT wipe pre-existing data) ── */

void detect_device(DeviceInfo *info) {
    /* NOTE: Do NOT memset-zero. Previous deep_scan_no_adb() may have filled
     * vendor/model/serial via USB descriptors. Only overwrite when ADB succeeds. */
    if (adb_state() < 2) return;

    getprop("ro.product.manufacturer", info->vendor, sizeof(info->vendor));
    getprop("ro.product.model", info->model, sizeof(info->model));
    getprop("ro.product.name", info->product_name, sizeof(info->product_name));
    getprop("ro.product.device", info->device_name, sizeof(info->device_name));
    getprop("ro.hardware", info->hardware, sizeof(info->hardware));
    getprop("ro.build.version.release", info->android, sizeof(info->android));
    getprop("ro.build.version.sdk", info->sdk, sizeof(info->sdk));
    getprop("ro.build.display.id", info->display_id, sizeof(info->display_id));
    getprop("ro.build.fingerprint", info->build_fingerprint, sizeof(info->build_fingerprint));
    getprop("ro.build.version.security_patch", info->security_patch, sizeof(info->security_patch));
    getprop("ro.serialno", info->serial, sizeof(info->serial));
    getprop("ro.boot.serialno", info->serial, sizeof(info->serial));
    getprop("ro.bootloader", info->bootloader, sizeof(info->bootloader));
    getprop("ro.build.version.baseband", info->baseband, sizeof(info->baseband));
    getprop("ro.product.cpu.abi", info->cpu_abi, sizeof(info->cpu_abi));

    detect_chipset(info);

    if (strlen(info->android) == 0) {
        char *tmp = adb_shell("cat /system/build.prop 2>/dev/null | grep ro.build.version.release | head -1");
        trim_newline(tmp);
        if (strlen(tmp) > 0) {
            char *p = strchr(tmp, '=');
            if (p) { p++; normalize_str(info->android, p); }
        }
        tmp = adb_shell("cat /proc/version 2>/dev/null | head -1");
        trim_newline(tmp);
        if (strlen(tmp) > 0) strncpy(info->kernel, tmp, sizeof(info->kernel)-1);
    }

    if (strlen(info->model) == 0) {
        strncpy(info->model, "Unknown (bootloop)", sizeof(info->model)-1);
        strncpy(info->vendor, "Unknown", sizeof(info->vendor)-1);
    }
}

int verify_device(const DeviceInfo *info) {
    if (strlen(info->serial) == 0 && strlen(info->model) == 0) {
        fprintf(stderr, "[-] Cannot verify device. ADB shell may not work.\n");
        return 0;
    }
    return 1;
}

int choose_pin_len(const DeviceInfo *info) {
    if (info->pin_len == 4 || info->pin_len == 6 || info->pin_len == 8)
        return info->pin_len;
    return 6;
}

/* ── Device mode string ── */

const char* device_mode_str(DeviceMode m) {
    switch (m) {
        case MODE_ADB:       return "ADB";
        case MODE_FASTBOOT:  return "FASTBOOT";
        case MODE_RECOVERY:  return "RECOVERY";
        case MODE_SIDELOAD:  return "SIDELOAD";
        case MODE_MTP:       return "MTP";
        case MODE_RNDIS:     return "RNDIS";
        case MODE_CHARGING:  return "CHARGING";
        case MODE_HID:       return "HID";
        default:             return "UNKNOWN";
    }
}

/* ── Multi-mode detection via usb_raw ── */

int detect_all_modes(ModeInfo *modes, int max_modes) {
    if (!modes || max_modes < 1) return 0;

    UsbRawDevice devices[USBRAW_MAX_DEVICES];
    int ndev = usb_raw_scan_all(devices, USBRAW_MAX_DEVICES);
    if (ndev < 1) return 0;

    int count = 0;
    for (int i = 0; i < ndev && count < max_modes; i++) {
        UsbRawDevice *d = &devices[i];

        /* Skip non-Android devices (no known interfaces) */
        if (!d->has_adb && !d->has_fastboot && !d->has_sideload && !d->has_mtp && !d->has_rndis)
            continue;

        ModeInfo *m = &modes[count];
        memset(m, 0, sizeof(*m));
        m->vid = d->idVendor;
        m->pid = d->idProduct;
        m->bus = d->bus;
        m->dev_num = d->dev_num;
        if (d->serial[0]) strncpy(m->serial, d->serial, sizeof(m->serial)-1);
        if (d->manufacturer[0]) strncpy(m->vendor_name, d->manufacturer, sizeof(m->vendor_name)-1);
        m->adb_available = d->has_adb;
        m->fastboot_available = d->has_fastboot;
        m->recovery_available = d->has_sideload;

        /* Determine primary mode */
        if (d->has_fastboot)               m->mode = MODE_FASTBOOT;
        else if (d->has_sideload && d->has_adb) m->mode = MODE_RECOVERY;
        else if (d->has_sideload)           m->mode = MODE_SIDELOAD;
        else if (d->has_adb)                m->mode = MODE_ADB;
        else if (d->has_mtp)                m->mode = MODE_MTP;
        else if (d->has_rndis)              m->mode = MODE_RNDIS;
        else if (d->has_hid)                m->mode = MODE_HID;
        else                                m->mode = MODE_CHARGING;

        strncpy(m->mode_str, device_mode_str(m->mode), sizeof(m->mode_str)-1);
        count++;

        usb_raw_close(d);
    }

    return count;
}

/* ── Fastboot-specific detect ── */

int detect_fastboot_mode(ModeInfo *info) {
    if (!info) return 0;
    memset(info, 0, sizeof(*info));

    FastbootInfo fb;
    if (fastboot_detect(&fb) <= 0) return 0;

    info->mode = MODE_FASTBOOT;
    strncpy(info->mode_str, "FASTBOOT", sizeof(info->mode_str)-1);
    if (fb.serial[0]) strncpy(info->serial, fb.serial, sizeof(info->serial)-1);
    info->fastboot_available = 1;
    return 1;
}

/* ── Any device available? ── */

int detect_any_device(void) {
    /* Try ADB first */
    if (adb_check() || adb_init()) return 1;

    /* Try fastboot */
    if (fastboot_detect(NULL) > 0) return 1;

    /* Scan USB for any Android device */
    ModeInfo modes[4];
    return detect_all_modes(modes, 4) > 0;
}

/* ── Print mode info ── */

void print_mode_info(const ModeInfo *info) {
    if (!info || info->mode == MODE_NONE) {
        printf("  [No device detected]\n");
        return;
    }

    printf("  Mode    : %s\n", info->mode_str);
    if (info->serial[0])        printf("  Serial  : %s\n", info->serial);
    if (info->vendor_name[0])   printf("  Vendor  : %s\n", info->vendor_name);
    printf("  VID:PID : 0x%04X:0x%04X\n", info->vid, info->pid);
    printf("  Bus     : %03d:%03d\n", info->bus, info->dev_num);

    printf("  Capabilities:");
    if (info->adb_available)         printf(" ADB");
    if (info->fastboot_available)    printf(" FASTBOOT");
    if (info->recovery_available)    printf(" RECOVERY");
    printf("\n");
}
