#define _GNU_SOURCE
#include "connection_mgr.h"
#include "adb_native.h"
#include "adb.h"
#include "usb_raw.h"
#include "fastboot.h"
#include "usb_mode.h"
#include "sysfs.h"
#include "detect.h"
#include "deep_scan.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <errno.h>

void mgr_init(ConnectionMgr *cm) {
    memset(cm, 0, sizeof(*cm));
    cm->state = MGR_DISCONNECTED;
    strcpy(cm->state_name, "DISCONNECTED");
    cm->bus = -1;
    cm->dev_num = -1;
}

void mgr_log(ConnectionMgr *cm, const char *fmt, ...) {
    if (!cm) return;
    va_list ap;
    va_start(ap, fmt);
    int space = (int)sizeof(cm->connection_log) - cm->log_len;
    if (space > 0) {
        cm->log_len += vsnprintf(cm->connection_log + cm->log_len, space, fmt, ap);
        if (cm->log_len > (int)sizeof(cm->connection_log) - 1)
            cm->log_len = (int)sizeof(cm->connection_log) - 1;
    }
    va_end(ap);
}

int mgr_scan_usb_devices(ConnectionMgr *cm) {
    UsbRawDevice devs[CONN_MAX_DEVICES];
    int n = usb_raw_scan_all(devs, CONN_MAX_DEVICES);
    if (n <= 0) {
        mgr_log(cm, "[-] No USB devices found.\n");
        return 0;
    }
    for (int i = 0; i < n; i++) {
        if (vendor_by_vid(devs[i].idVendor)) {
            cm->bus = devs[i].bus;
            cm->dev_num = devs[i].dev_num;
            cm->vid = devs[i].idVendor;
            cm->pid = devs[i].idProduct;
            snprintf(cm->state_name, sizeof(cm->state_name), "USB_DETECTED");
            cm->state = MGR_USB_DETECTED;

            /* Read USB descriptors via sysfs */
            char path[256];
            snprintf(path, sizeof(path), "/sys/bus/usb/devices/usb%03d/%03d-%03d",
                     cm->bus, cm->bus, cm->dev_num);
            char alt_path[256];
            snprintf(alt_path, sizeof(alt_path), "/sys/bus/usb/devices/%d-%d",
                     cm->bus, cm->dev_num);

            /* Try to read manufacturer, product, serial */
            const char *paths[] = {path, alt_path};
            for (int p = 0; p < 2; p++) {
                char full[512];
                snprintf(full, sizeof(full), "%s/manufacturer", paths[p]);
                FILE *f = fopen(full, "r");
                if (f) {
                    if (fgets(cm->manufacturer, sizeof(cm->manufacturer), f))
                        cm->manufacturer[strcspn(cm->manufacturer, "\n")] = 0;
                    fclose(f);
                }
                snprintf(full, sizeof(full), "%s/product", paths[p]);
                f = fopen(full, "r");
                if (f) {
                    if (fgets(cm->product, sizeof(cm->product), f))
                        cm->product[strcspn(cm->product, "\n")] = 0;
                    fclose(f);
                }
                snprintf(full, sizeof(full), "%s/serial", paths[p]);
                f = fopen(full, "r");
                if (f) {
                    if (fgets(cm->serial, sizeof(cm->serial), f))
                        cm->serial[strcspn(cm->serial, "\n")] = 0;
                    fclose(f);
                }
            }

            mgr_log(cm, "[+] USB device: %04x:%04x %s %s\n",
                     cm->vid, cm->pid, cm->manufacturer, cm->product);
            return 1;
        }
    }
    mgr_log(cm, "[-] No Android device found on USB.\n");
    return 0;
}

int mgr_try_adb(ConnectionMgr *cm) {
    mgr_log(cm, "[*] Trying ADB...\n");
    cm->tries_adb++;

    if (!adb_init()) {
        mgr_log(cm, "[-] ADB init failed.\n");
        cm->state = MGR_FAILED;
        strcpy(cm->state_name, "ADB_INIT_FAIL");
        return 0;
    }

    int state = adb_state();
    if (state >= 2) {
        cm->state = MGR_ADB_AUTHORIZED;
        strcpy(cm->state_name, "ADB_AUTHORIZED");
        mgr_log(cm, "[+] ADB authorized!\n");

        /* Gather basic info */
        char buf[4096];
        if (adb_native_shell("getprop ro.product.manufacturer", buf, sizeof(buf)) == 0)
            strncpy(cm->vendor, buf, sizeof(cm->vendor)-1);
        cm->vendor[strcspn(cm->vendor, "\n")] = 0;
        if (adb_native_shell("getprop ro.product.model", buf, sizeof(buf)) == 0)
            strncpy(cm->model, buf, sizeof(cm)-1);
        cm->model[strcspn(cm->model, "\n")] = 0;
        if (adb_native_shell("getprop ro.build.version.release", buf, sizeof(buf)) == 0)
            strncpy(cm->android_version, buf, sizeof(cm->android_version)-1);
        cm->android_version[strcspn(cm->android_version, "\n")] = 0;
        if (adb_native_shell("getprop ro.build.version.sdk", buf, sizeof(buf)) == 0)
            strncpy(cm->sdk_version, buf, sizeof(cm->sdk_version)-1);
        cm->sdk_version[strcspn(cm->sdk_version, "\n")] = 0;
        if (adb_native_shell("getprop ro.build.fingerprint", buf, sizeof(buf)) == 0)
            strncpy(cm->build_fingerprint, buf, sizeof(cm->build_fingerprint)-1);
        cm->build_fingerprint[strcspn(cm->build_fingerprint, "\n")] = 0;

        /* Get screen size */
        if (adb_native_shell("wm size 2>/dev/null | head -1", buf, sizeof(buf)) == 0)
            strncpy(cm->screen_size, buf, sizeof(cm->screen_size)-1);
        cm->screen_size[strcspn(cm->screen_size, "\n")] = 0;

        mgr_log(cm, "[+] %s %s (Android %s)\n", cm->vendor, cm->model, cm->android_version);
        return 1;
    }

    if (state == 1) {
        cm->state = MGR_ADB_UNAUTHORIZED;
        strcpy(cm->state_name, "ADB_UNAUTHORIZED");
        mgr_log(cm, "[-] ADB device unauthorized (accept RSA key or use exploit).\n");
        return 0;
    }

    mgr_log(cm, "[-] ADB device not found.\n");
    cm->state = MGR_FAILED;
    strcpy(cm->state_name, "ADB_NOT_FOUND");
    return 0;
}

int mgr_try_fastboot(ConnectionMgr *cm) {
    mgr_log(cm, "[*] Trying Fastboot...\n");
    cm->tries_fastboot++;

    ModeInfo mi;
    if (detect_fastboot_mode(&mi)) {
        cm->has_fastboot = 1;
        cm->state = MGR_FASTBOOT;
        strcpy(cm->state_name, "FASTBOOT");
        mgr_log(cm, "[+] Fastboot device available!\n");

        /* Gather fastboot info */
        mgr_gather_fastboot_info(cm);
        return 1;
    }

    /* Try to reboot to bootloader */
    mgr_log(cm, "[-] Fastboot not available.\n");
    return 0;
}

int mgr_try_recovery_adb(ConnectionMgr *cm) {
    mgr_log(cm, "[*] Trying Recovery ADB...\n");
    cm->tries_recovery++;

    /* First, check if we can reboot to recovery */
    if (!adb_init()) {
        mgr_log(cm, "[-] Cannot init ADB to reboot to recovery.\n");
        return 0;
    }

    /* Try rebooting to recovery */
    mgr_log(cm, "[*] Rebooting to recovery...\n");
    char buf[256];
    adb_native_shell("reboot recovery 2>/dev/null", buf, sizeof(buf));

    /* Wait for device to reappear in recovery mode */
    mgr_log(cm, "[*] Waiting for device in recovery...\n");
    sleep(5);

    /* Try ADB again */
    if (adb_init()) {
        for (int i = 0; i < 15; i++) {
            int st = adb_state();
            if (st >= 2) {
                cm->has_recovery_adb = 1;
                cm->state = MGR_RECOVERY_ADB;
                strcpy(cm->state_name, "RECOVERY_ADB");
                mgr_log(cm, "[+] Recovery ADB active!\n");
                return 1;
            }
            if (st == 1) {
                /* Unauthorized in recovery too */
                mgr_log(cm, "[-] Recovery ADB also unauthorized.\n");
                break;
            }
            sleep(1);
        }
    }

    mgr_log(cm, "[-] Recovery ADB not available.\n");
    return 0;
}

int mgr_gather_usb_info(ConnectionMgr *cm) {
    if (cm->state < MGR_USB_DETECTED) return 0;

    /* Read sysfs for device info */
    char path[256];
    snprintf(path, sizeof(path), "/sys/bus/usb/devices/%d-%d", cm->bus, cm->dev_num);

    char full[512];
    char buf[256];

    /* bcdDevice - often encodes Android version */
    snprintf(full, sizeof(full), "%s/bcdDevice", path);
    FILE *f = fopen(full, "r");
    if (f) {
        if (fgets(buf, sizeof(buf), f)) {
            buf[strcspn(buf, "\n")] = 0;
            mgr_log(cm, "   bcdDevice: %s\n", buf);
        }
        fclose(f);
    }

    return 1;
}

int mgr_gather_fastboot_info(ConnectionMgr *cm) {
    char buf[4096];

    /* Get product info */
    FILE *fp = popen("fastboot getvar product 2>&1", "r");
    if (fp) {
        if (fgets(buf, sizeof(buf), fp))
            strncpy(cm->model, buf, sizeof(cm->model)-1);
        cm->model[strcspn(cm->model, "\n")] = 0;
        pclose(fp);
    }

    fp = popen("fastboot getvar version-bootloader 2>&1", "r");
    if (fp) {
        if (fgets(buf, sizeof(buf), fp))
            strncpy(cm->bootloader, buf, sizeof(cm->bootloader)-1);
        cm->bootloader[strcspn(cm->bootloader, "\n")] = 0;
        pclose(fp);
    }

    fp = popen("fastboot getvar version-baseband 2>&1", "r");
    if (fp) {
        if (fgets(buf, sizeof(buf), fp))
            strncpy(cm->baseband, buf, sizeof(cm->baseband)-1);
        cm->baseband[strcspn(cm->baseband, "\n")] = 0;
        pclose(fp);
    }

    fp = popen("fastboot getvar serialno 2>&1", "r");
    if (fp) {
        if (fgets(buf, sizeof(buf), fp))
            strncpy(cm->serial, buf, sizeof(cm->serial)-1);
        cm->serial[strcspn(cm->serial, "\n")] = 0;
        pclose(fp);
    }

    mgr_log(cm, "[+] Fastboot: %s | BL: %s\n", cm->model, cm->bootloader);
    return 1;
}

int mgr_try_usb_hid(ConnectionMgr *cm) {
    cm->tries_usb_hid++;
    mgr_log(cm, "[*] Trying USB HID mode...\n");

    /* Switch USB mode to something that might accept HID */
    UsbRawDevice raw;
    if (usb_raw_open_device(cm->bus, cm->dev_num, &raw) > 0) {
        /* Try switching to accessory mode */
        if (raw.has_adb == 0) {
            if (usb_mode_try_switch(&raw.dev)) {
                mgr_log(cm, "[+] USB mode switched!\n");
                usb_raw_close(&raw);
                /* Recheck ADB */
                if (adb_init()) {
                    for (int i = 0; i < 10; i++) {
                        if (adb_state() >= 2) {
                            cm->state = MGR_ADB_AUTHORIZED;
                            strcpy(cm->state_name, "ADB_AFTER_USB_SWITCH");
                            mgr_log(cm, "[+] ADB available after USB switch!\n");
                            return 1;
                        }
                        sleep(1);
                    }
                }
                return 1;
            }
        }
        usb_raw_close(&raw);
    }

    mgr_log(cm, "[-] USB HID mode not available.\n");
    return 0;
}

int mgr_force_recovery(ConnectionMgr *cm) {
    mgr_log(cm, "[*] Attempting force recovery boot...\n");

    /* Method 1: ADB reboot recovery */
    if (adb_init()) {
        adb_native_shell_noret("reboot recovery");
        mgr_log(cm, "[*] ADB reboot recovery sent.\n");
        return 1;
    }

    /* Method 2: fastboot boot recovery */
    if (cm->has_fastboot) {
        mgr_log(cm, "[*] Trying fastboot boot recovery.img...\n");
        FILE *fp = popen("fastboot boot recovery.img 2>&1", "r");
        if (fp) {
            char buf[256];
            while (fgets(buf, sizeof(buf), fp))
                mgr_log(cm, "   fastboot: %s", buf);
            pclose(fp);
        }
        /* Check for ADB after boot */
        sleep(10);
        if (adb_init()) {
            for (int i = 0; i < 20; i++) {
                if (adb_state() >= 2) return 1;
                sleep(1);
            }
        }
    }

    /* Method 3: key combo instructions */
    mgr_log(cm, "[!] Manual: Press Power+VolUp to enter recovery.\n");
    return 0;
}

int mgr_auto_connect(ConnectionMgr *cm, int timeout_sec) {
    mgr_log(cm, "[*] Auto-connecting to device...\n");

    /* 1. Scan USB for Android devices */
    if (!mgr_scan_usb_devices(cm)) {
        mgr_log(cm, "[-] No Android device on USB.\n");
        cm->state = MGR_FAILED;
        return 0;
    }

    /* 2. Try ADB (normal) */
    if (mgr_try_adb(cm)) return 1;

    /* 3. Try USB mode switch + ADB recheck */
    if (mgr_try_usb_hid(cm)) return 1;

    /* 4. Try Fastboot */
    if (mgr_try_fastboot(cm)) {
        mgr_log(cm, "[*] Recovery exploit available via fastboot.\n");
        return 1;
    }

    /* 5. Try Recovery ADB */
    if (mgr_try_recovery_adb(cm)) return 1;

    /* 6. Report USB info as fallback */
    mgr_gather_usb_info(cm);
    cm->state = MGR_USB_DETECTED;
    strcpy(cm->state_name, "USB_ONLY");
    mgr_log(cm, "[!] Advanced ADB unavailable. USB info only.\n");
    return 1;
}

void mgr_print_status(ConnectionMgr *cm) {
    printf("\n" BOLD "=== Connection Status ===" RESET "\n");
    printf("  State    : %s%s%s\n",
           cm->state >= MGR_ADB_AUTHORIZED ? GREEN :
           cm->state >= MGR_USB_DETECTED ? YELLOW : RED,
           cm->state_name, RESET);
    printf("  Device   : ");
    if (cm->manufacturer[0]) printf("%s ", cm->manufacturer);
    if (cm->product[0]) printf("%s", cm->product);
    printf("\n");
    if (cm->serial[0]) printf("  Serial   : %s\n", cm->serial);
    if (cm->vendor[0]) printf("  Vendor   : %s\n", cm->vendor);
    if (cm->model[0]) printf("  Model    : %s\n", cm->model);
    if (cm->android_version[0]) printf("  Android  : %s\n", cm->android_version);
    if (cm->screen_size[0]) printf("  Screen   : %s\n", cm->screen_size);
    if (cm->bootloader[0]) printf("  BL       : %s\n", cm->bootloader);

    printf("\n" BOLD "  Attempts:" RESET "\n");
    printf("    ADB     : %s\n", cm->tries_adb ? (cm->state == MGR_ADB_AUTHORIZED ? GREEN"OK" : RED"FAIL") : " -");
    printf("    Fastboot: %s\n", cm->tries_fastboot ? (cm->has_fastboot ? GREEN"OK" : RED"FAIL") : " -");
    printf("    Recovery: %s\n", cm->tries_recovery ? (cm->has_recovery_adb ? GREEN"OK" : RED"FAIL") : " -");
    printf("    USB HID : %s\n", cm->tries_usb_hid ? RED"FAIL" : " -");

    if (cm->connection_log[0])
        printf("\n%s\n", cm->connection_log);
}
