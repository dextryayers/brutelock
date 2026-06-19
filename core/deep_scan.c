#define _GNU_SOURCE
#include "deep_scan.h"
#include "adb.h"
#include "adb_native.h"
#include "fastboot.h"
#include "usb_raw.h"
#include "detect.h"
#include "database.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* ── Known Android vendor IDs and their names ── */
typedef struct { int vid; const char *name; } VendorEntry;
static const VendorEntry vids[] = {
    {0x18d1, "Google"},    {0x04e8, "Samsung"},   {0x2717, "Xiaomi"},
    {0x12d1, "Huawei"},    {0x2a70, "OnePlus"},    {0x22d9, "OPPO"},
    {0x2d95, "Vivo"},      {0x1004, "LG"},          {0x054c, "Sony"},
    {0x22b8, "Motorola"},  {0x0421, "Nokia"},       {0x18d2, "Google"},
    {0x0b05, "ASUS"},      {0x17ef, "Lenovo"},      {0x0bb4, "HTC"},
    {0x0502, "Acer"},      {0x19d2, "ZTE"},         {0x2d02, "Meizu"},
    {0x19d1, "Nubia"},     {0x1bbb, "TCL"},         {0x091e, "Garmin-Asus"},
    {0x04e8, "Samsung"},   {0x04c5, "Fujitsu"},     {0x0471, "Philips"},
    {0x413c, "Dell"},      {0x0489, "Foxconn"},      {0x1c9e, "Pegatron"},
    {0x0c44, "Panasonic"}, {0x04dd, "Sharp"},        {0x0fce, "Pantech"},
    {0x1eb5, "Zenfone"},   {0x2970, "Xiaomi"},       {0, NULL}
};

const char* vendor_by_vid(int vid) {
    for (int i = 0; vids[i].vid; i++)
        if (vids[i].vid == vid) return vids[i].name;
    return NULL;
}

/* ── Known SoC families mapped from USB VID/PID ── */
typedef struct { int vid; int pid; const char *chipset; } SocEntry;
static const SocEntry soc_map[] = {
    /* Google / Pixel */
    {0x18d1, 0x4ee0, "Google Tensor G5"},  {0x18d1, 0x4ee1, "Google Tensor G4"},
    {0x18d1, 0x4ee2, "Google Tensor G3"},  {0x18d1, 0x4ee3, "Google Tensor G2"},
    {0x18d1, 0x4ee4, "Google Tensor"},     {0x18d1, 0x4ee5, "Google Tensor G5"},
    {0x18d1, 0x4ee6, "Google Tensor G4"},  {0x18d1, 0x4ee7, "Google Tensor G3"},
    /* Samsung */
    {0x04e8, 0x6860, "Samsung Exynos 2400"}, {0x04e8, 0x6861, "Samsung Exynos 2200"},
    {0x04e8, 0x6862, "Samsung Exynos 2100"}, {0x04e8, 0x6863, "Samsung Exynos 990"},
    {0x04e8, 0x6864, "Samsung Exynos 9820"}, {0x04e8, 0x6865, "Samsung Exynos 9810"},
    {0x04e8, 0x6866, "Samsung Exynos 9611"}, {0x04e8, 0x6867, "Samsung Exynos 9609"},
    {0x04e8, 0x6868, "Samsung Exynos 8895"}, {0x04e8, 0x6869, "Samsung Exynos 8890"},
    {0x04e8, 0x68C0, "Samsung Exynos 850"},  {0x04e8, 0x68C1, "Samsung Exynos 7885"},
    /* Qualcomm reference IDs */
    {0x05c6, 0x9008, "Qualcomm Snapdragon (EDL)"},
    {0x05c6, 0x9009, "Qualcomm Snapdragon (9009)"},
    {0x05c6, 0x900E, "Qualcomm Snapdragon (EDL)"},
    {0x05c6, 0x901B, "Qualcomm Snapdragon (901B)"},
    {0x05c6, 0x9025, "Qualcomm Snapdragon (9025)"},
    /* MediaTek */
    {0x0e8d, 0x2000, "MediaTek"}, {0x0e8d, 0x2001, "MediaTek Dimensity"},
    {0x0e8d, 0x2008, "MediaTek"}, {0x0e8d, 0x200C, "MediaTek"},
    /* Huawei HiSilicon */
    {0x12d1, 0x3608, "HiSilicon Kirin 9000"}, {0x12d1, 0x3609, "HiSilicon Kirin 990"},
    {0x12d1, 0x360A, "HiSilicon Kirin 990"},  {0x12d1, 0x360B, "HiSilicon Kirin 985"},
    {0x12d1, 0x360C, "HiSilicon Kirin 980"},  {0x12d1, 0x360D, "HiSilicon Kirin 970"},
    {0x12d1, 0x360E, "HiSilicon Kirin 960"},  {0x12d1, 0x360F, "HiSilicon Kirin 950"},
    {0x12d1, 0x3610, "HiSilicon Kirin 710"},  {0x12d1, 0x3611, "HiSilicon Kirin 659"},
    /* OnePlus / Realme (use Qualcomm) */
    {0x2a70, 0x4EE0, "OnePlus (Qualcomm SM8650)"},
    {0x2a70, 0x4EE1, "OnePlus (Qualcomm SM8550)"},
    {0x2a70, 0x4EE2, "OnePlus (Qualcomm SM8475)"},
    /* Xiaomi */
    {0x2717, 0x9080, "Xiaomi (Qualcomm SM8550)"},
    {0x2717, 0x9081, "Xiaomi (MediaTek Dimensity)"},
    {0x2717, 0x9082, "Xiaomi (Qualcomm SM8475)"},
    {0, 0, NULL}
};

static const char* soc_by_vid_pid(int vid, int pid) {
    for (int i = 0; soc_map[i].vid; i++)
        if (soc_map[i].vid == vid && soc_map[i].pid == pid)
            return soc_map[i].chipset;
    return NULL;
}

/* ── Fill DeviceInfo from raw USB descriptor ── */
void deep_scan_from_usb(DeviceInfo *info, const UsbRawDevice *raw) {
    if (!info || !raw || !raw->present) return;

    if (raw->manufacturer[0] && !info->vendor[0])
        strncpy(info->vendor, raw->manufacturer, sizeof(info->vendor)-1);
    else if (!info->vendor[0]) {
        const char *vn = vendor_by_vid(raw->idVendor);
        if (vn) strncpy(info->vendor, vn, sizeof(info->vendor)-1);
    }

    if (raw->product_str[0] && !info->model[0])
        strncpy(info->model, raw->product_str, sizeof(info->model)-1);

    if (raw->serial[0] && !info->serial[0])
        strncpy(info->serial, raw->serial, sizeof(info->serial)-1);

    /* Detect SoC from USB IDs */
    if (!info->chipset_vendor[0]) {
        const char *soc = soc_by_vid_pid(raw->idVendor, raw->idProduct);
        if (soc) {
            char buf[256];
            strncpy(buf, soc, sizeof(buf)-1);
            char *space = strchr(buf, ' ');
            if (space) {
                *space = 0;
                strncpy(info->chipset_vendor, buf, sizeof(info->chipset_vendor)-1);
                strncpy(info->chipset_name, space+1, sizeof(info->chipset_name)-1);
            } else {
                strncpy(info->chipset_name, soc, sizeof(info->chipset_name)-1);
            }
        } else {
            const char *vn = vendor_by_vid(raw->idVendor);
            if (vn) strncpy(info->chipset_vendor, vn, sizeof(info->chipset_vendor)-1);
        }
    }

    /* Mark as detected via USB */
    if (!info->model[0] && raw->idVendor)
        snprintf(info->model, sizeof(info->model), "Android (0x%04X:0x%04X)",
                 raw->idVendor, raw->idProduct);
}

/* ── Fill DeviceInfo from fastboot variables ── */
void deep_scan_from_fastboot(DeviceInfo *info, const FastbootInfo *fb) {
    if (!info || !fb || !fb->present) return;

    if (fb->product[0] && !info->model[0])
        strncpy(info->model, fb->product, sizeof(info->model)-1);

    if (fb->serial[0] && !info->serial[0])
        strncpy(info->serial, fb->serial, sizeof(info->serial)-1);

    if (fb->soc[0] && !info->chipset_name[0])
        strncpy(info->chipset_name, fb->soc, sizeof(info->chipset_name)-1);

    if (fb->fingerprint[0] && !info->build_fingerprint[0])
        strncpy(info->build_fingerprint, fb->fingerprint, sizeof(info->build_fingerprint)-1);

    if (fb->version[0] && !info->bootloader[0])
        strncpy(info->bootloader, fb->version, sizeof(info->bootloader)-1);

    if (fb->hw_revision[0] && !info->hardware[0])
        strncpy(info->hardware, fb->hw_revision, sizeof(info->hardware)-1);

    /* Fastboot always means locked bootloader */
    info->locked = 1;

    /* Fill more fastboot fields */
    if (fb->display_panel[0] && !info->screen_size[0])
        strncpy(info->screen_size, fb->display_panel, sizeof(info->screen_size)-1);

    if (fb->battery_voltage[0] && !info->battery_level[0])
        strncpy(info->battery_level, fb->battery_voltage, sizeof(info->battery_level)-1);

    strncpy(info->detected_via, "Fastboot", sizeof(info->detected_via)-1);
}

/* ── Deep scan without ADB ── */
int deep_scan_no_adb(DeviceInfo *info) {
    if (!info) return 0;
    memset(info, 0, sizeof(*info));

    /* Default: assume locked when no ADB access */
    info->locked = 1;
    info->pin_len = -1;

    /* Step 1: Try fastboot (most info without ADB) */
    FastbootInfo fb;
    if (fastboot_getvar_all(&fb) == 0 && fb.present) {
        deep_scan_from_fastboot(info, &fb);
        strncpy(info->detected_via, "Fastboot", sizeof(info->detected_via)-1);
        return 1;
    }

    /* Step 2: Try raw USB descriptor */
    UsbRawDevice devices[USBRAW_MAX_DEVICES];
    int ndev = usb_raw_scan_all(devices, USBRAW_MAX_DEVICES);
    if (ndev > 0) {
        for (int i = 0; i < ndev; i++) {
            if (devices[i].has_adb || devices[i].has_fastboot ||
                devices[i].has_mtp || devices[i].has_rndis) {
                deep_scan_from_usb(info, &devices[i]);
                /* Set default lock type for non-ADB */
                strncpy(info->pin_type, "PIN", sizeof(info->pin_type)-1);
                strncpy(info->lock_type, "PIN", sizeof(info->lock_type)-1);
                strncpy(info->detected_via, "USB", sizeof(info->detected_via)-1);
                usb_raw_close(&devices[i]);
                return 1;
            }
            usb_raw_close(&devices[i]);
        }
        for (int i = 0; i < ndev; i++) {
            if (vendor_by_vid(devices[i].idVendor)) {
                deep_scan_from_usb(info, &devices[i]);
                strncpy(info->pin_type, "PIN", sizeof(info->pin_type)-1);
                strncpy(info->lock_type, "PIN", sizeof(info->lock_type)-1);
                strncpy(info->detected_via, "USB", sizeof(info->detected_via)-1);
                usb_raw_close(&devices[i]);
                return 1;
            }
            usb_raw_close(&devices[i]);
        }
    }
    return 0;
}

/* ── Deep scan from ALL sources (does NOT wipe pre-existing data) ── */
int deep_scan_all(DeviceInfo *info) {
    if (!info) return 0;
    /* NOTE: Do NOT memset-zero. The caller may have data from USB/fastboot scan.
     * Only overwrite fields that we can fill from each source. */
    info->pin_len = -1;

    int found_any = 0;

    /* Step 1: Try ADB getprop first (most detailed) */
    if (adb_native_state() >= 2) {
        detect_device(info);
        if (info->vendor[0] || info->model[0]) {
            strncpy(info->detected_via, "ADB", sizeof(info->detected_via)-1);
            found_any = 1;
        }
        detect_database(info);
        if (adb_is_unlocked()) info->locked = 0;
    }

    /* Step 2: Supplement with fastboot info */
    FastbootInfo fb;
    if (fastboot_getvar_all(&fb) == 0 && fb.present) {
        deep_scan_from_fastboot(info, &fb);
        if (!found_any) {
            strncpy(info->detected_via, "Fastboot", sizeof(info->detected_via)-1);
            found_any = 1;
        }
    }

    /* Step 3: Supplement with raw USB descriptor info */
    UsbRawDevice devices[USBRAW_MAX_DEVICES];
    int ndev = usb_raw_scan_all(devices, USBRAW_MAX_DEVICES);
    if (ndev > 0) {
        for (int i = 0; i < ndev; i++) {
            if (devices[i].has_adb || devices[i].has_fastboot) {
                deep_scan_from_usb(info, &devices[i]);
                if (!found_any) {
                    strncpy(info->detected_via, "USB", sizeof(info->detected_via)-1);
                    found_any = 1;
                }
                usb_raw_close(&devices[i]);
                break;
            }
            usb_raw_close(&devices[i]);
        }
    }

    return found_any;
}
