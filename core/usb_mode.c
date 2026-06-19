#define _GNU_SOURCE
#include "usb_mode.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#if OS_POSIX && !OS_MAC
#include <linux/usbdevice_fs.h>
#include <sys/ioctl.h>
#endif

/* ── USB mode switching ──
 *
 * Android devices typically expose multiple USB configurations:
 *   Config 0: Charging only (no data interfaces)
 *   Config 1: MTP (0xFF/0xFF/0xFF) + ADB (0xFF/0x42/0x01) if enabled
 *   Config 2: PTP (0x06/0x01/0x01)
 *   Config 3: RNDIS (0x02/0x02/0x01) + ADB
 *
 * The active config can be switched via USB SET_CONFIGURATION request.
 * Some devices also support vendor-specific requests.
 */

int usb_mode_get_configs(UsbDevice *dev, UsbConfigInfo *info) {
    if (!dev || !info) return -1;
    memset(info, 0, sizeof(*info));

#if OS_POSIX && !OS_MAC
    if (dev->fd < 0) return -1;

    /* Read config descriptor(s) to enumerate all configurations */
    unsigned char buf[4096];
    struct usbdevfs_ctrltransfer ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.bRequestType = 0x80 | 0x00 | 0x00;
    ctrl.bRequest = 6;
    ctrl.wValue  = (2 << 8) | 0;  /* CONFIGURATION descriptor */
    ctrl.wIndex  = 0;
    ctrl.wLength = sizeof(buf);
    ctrl.data    = buf;
    ctrl.timeout = 5000;
    int r = ioctl(dev->fd, USBDEVFS_CONTROL, &ctrl);
    if (r < 9) return -1;

    int ilen = buf[0];

    /* There might be multiple configurations. Try reading each. */
    /* For now, parse the first config and extract available alternates */
    unsigned char *p = buf + 9;
    int left = ilen - 9;

    int has_mtp = 0, has_adb = 0, has_fastboot = 0, has_rndis = 0, has_ptp = 0;

    while (left >= 3) {
        int len = p[0];
        if (len < 3 || len > left) break;

        if (p[1] == 4 && len >= 9) {
            /* INTERFACE descriptor */
            if (p[3] == 0xFF && p[4] == 0xFF && p[5] == 0xFF) has_mtp = 1;
            if (p[3] == 0xFF && p[4] == 0x42 && p[5] == 0x01) has_adb = 1;
            if (p[3] == 0xFF && p[4] == 0x42 && p[5] == 0x03) has_fastboot = 1;
            if (p[3] == 0x02 && p[4] == 0x02 && p[5] == 0x01) has_rndis = 1;
            if (p[3] == 0x02 && p[4] == 0x02 && p[5] == 0x02) has_rndis = 1;
            if (p[3] == 0x06 && p[4] == 0x01 && p[5] == 0x01) has_ptp = 1;
            if (p[3] == 0xEF && p[4] == 0x04 && p[5] == 0x01) has_rndis = 1;
        }
        left -= len;
        p   += len;
    }

    info->has_mtp = has_mtp;
    info->has_adb = has_adb;
    info->has_fastboot = has_fastboot;
    info->has_rndis = has_rndis;
    info->has_ptp = has_ptp;

    int n = 0;
    if (has_mtp)      snprintf(info->config_names[n++], 64, "MTP");
    if (has_adb)      snprintf(info->config_names[n++], 64, "ADB");
    if (has_fastboot) snprintf(info->config_names[n++], 64, "Fastboot");
    if (has_rndis)    snprintf(info->config_names[n++], 64, "RNDIS");
    if (has_ptp)      snprintf(info->config_names[n++], 64, "PTP");
    if (n == 0)       snprintf(info->config_names[n++], 64, "Charging");
    info->configs_count = n;

    return n;

#else
    (void)dev;
    return -1;
#endif
}

int usb_mode_set_config(UsbDevice *dev, int config_index) {
#if OS_POSIX && !OS_MAC
    if (dev->fd < 0) return -1;
    struct usbdevfs_ctrltransfer ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.bRequestType = 0x00;
    ctrl.bRequest = 9;           /* SET_CONFIGURATION */
    ctrl.wValue  = config_index;
    ctrl.wIndex  = 0;
    ctrl.wLength = 0;
    ctrl.data    = NULL;
    ctrl.timeout = 5000;
    return ioctl(dev->fd, USBDEVFS_CONTROL, &ctrl);
#else
    (void)dev; (void)config_index;
    return -1;
#endif
}

int usb_mode_set_mtp(UsbDevice *dev) {
    /* Try configuration 1 (MTP is typically config 1) */
    return usb_mode_set_config(dev, 1);
}

int usb_mode_set_adb(UsbDevice *dev) {
    /* Try to enable ADB by looking for known adb enable control request */
    /* Some devices use vendor-specific request to enable ADB */
    /* Standard approach: switch to config that includes ADB interface */
    /* Usually config 1 (MTP+ADB) or config 2 */
    int r = usb_mode_set_config(dev, 1);
    if (r < 0) r = usb_mode_set_config(dev, 2);
    return r;
}

int usb_mode_set_rndis(UsbDevice *dev) {
    /* RNDIS is usually config 2 or 3 */
    int r = usb_mode_set_config(dev, 2);
    if (r < 0) r = usb_mode_set_config(dev, 3);
    return r;
}

/* ── Android Accessory Mode (AOAv2) ──
 *
 * This protocol lets the Android device act as USB host and the computer
 * as a USB accessory. Once in accessory mode, the computer can send HID
 * keyboard reports directly to the lock screen.
 *
 * Steps:
 *   1. Send 0x51 (GET_PROTOCOL) control request
 *   2. If protocol >= 2, send accessory identification strings
 *   3. Send 0x53 (START) control request
 *   4. Device disconnects and reconnects as host, computer as accessory
 *   5. Send HID keyboard reports via interrupt OUT endpoint
 */

#define AOA_GET_PROTOCOL     0x51
#define AOA_SEND_STRING      0x52
#define AOA_START            0x53
#define AOA_REGISTER_HID     0x54
#define AOA_UNREGISTER_HID   0x55
#define AOA_SET_HID_REPORT   0x56
#define AOA_SEND_HID_EVENT   0x57
#define AOA_SET_AUDIO        0x58

#define AOA_STRING_MANUFACTURER 0
#define AOA_STRING_MODEL        1
#define AOA_STRING_DESCRIPTION  2
#define AOA_STRING_VERSION      3
#define AOA_STRING_URI          4
#define AOA_STRING_SERIAL       5

#define AOA_IFACE_CLASS   0xFF
#define AOA_IFACE_SUBCLASS 0xFF
#define AOA_IFACE_PROTOCOL 0

int usb_mode_try_accessory(UsbDevice *dev) {
#if OS_POSIX && !OS_MAC
    if (dev->fd < 0) return -1;

    struct usbdevfs_ctrltransfer ctrl;

    /* Step 1: Get protocol version */
    unsigned char proto = 0;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.bRequestType = 0xC0 | 0x40;  /* DEVICE to HOST, VENDOR */
    ctrl.bRequest = AOA_GET_PROTOCOL;
    ctrl.wValue  = 0;
    ctrl.wIndex  = 0;
    ctrl.wLength = 2;
    ctrl.data    = &proto;
    ctrl.timeout = 5000;
    int r = ioctl(dev->fd, USBDEVFS_CONTROL, &ctrl);
    if (r < 2 || proto < 2) return -1;

    printf("[AOA] Protocol v%d detected\n", proto);

    /* Step 2: Send identification strings */
    const char *strings[] = {
        "BrutePin",           /* manufacturer */
        "Android PIN Tool",   /* model */
        "PIN Brute Force",    /* description */
        "1.0",                /* version */
        "https://github.com/brutepin", /* URI */
        "00000001"            /* serial */
    };

    for (int i = 0; i < 6; i++) {
        memset(&ctrl, 0, sizeof(ctrl));
        ctrl.bRequestType = 0x40;
        ctrl.bRequest = AOA_SEND_STRING;
        ctrl.wValue  = i;
        ctrl.wIndex  = 0;
        ctrl.wLength = strlen(strings[i]) + 1;
        ctrl.data    = (void*)strings[i];
        ctrl.timeout = 5000;
        ioctl(dev->fd, USBDEVFS_CONTROL, &ctrl);
    }

    /* Step 3: Start accessory mode */
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.bRequestType = 0x40;
    ctrl.bRequest = AOA_START;
    ctrl.wValue  = 0;
    ctrl.wIndex  = 0;
    ctrl.wLength = 0;
    ctrl.data    = NULL;
    ctrl.timeout = 5000;
    r = ioctl(dev->fd, USBDEVFS_CONTROL, &ctrl);
    if (r < 0) return -1;

    printf("[AOA] Accessory mode started - device will reconnect\n");
    return 1;
#else
    (void)dev;
    return -1;
#endif
}

/* ── Try all methods to get useful access ── */
int usb_mode_try_switch(UsbDevice *dev) {
    if (!dev) return 0;

    /* Method 1: Try to switch to MTP config (index 1) */
    printf("[*] Trying MTP mode... ");
    fflush(stdout);
    if (usb_mode_set_mtp(dev) == 0) {
        usleep(200000);
        /* Re-read config to check */
        UsbConfigInfo cfg;
        usb_mode_get_configs(dev, &cfg);
        if (cfg.has_mtp) { printf("MTP active\n"); return 1; }
        printf("no change\n");
    } else {
        printf("not supported\n");
    }

    /* Method 2: Try AOAv2 accessory mode */
    printf("[*] Trying Accessory mode... ");
    fflush(stdout);
    int r = usb_mode_try_accessory(dev);
    if (r > 0) { printf("switched\n"); return 1; }
    printf("not supported\n");

    return 0;
}

/* ── Find best available mode ── */
int usb_mode_find_best(UsbDevice *dev) {
    if (!dev) return 0;

    UsbConfigInfo cfg;
    if (usb_mode_get_configs(dev, &cfg) <= 0) return 0;

    if (cfg.has_adb)      return 2;  /* ADB - best for our tool */
    if (cfg.has_fastboot) return 3;  /* Fastboot */
    if (cfg.has_rndis)    return 4;  /* RNDIS networking */
    if (cfg.has_mtp)      return 1;  /* MTP file access */
    if (cfg.has_ptp)      return 5;  /* PTP */
    return 0;  /* Charging only */
}
