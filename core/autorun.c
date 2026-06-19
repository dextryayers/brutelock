#define _GNU_SOURCE
#include "autorun.h"
#include "util.h"
#include "adb.h"
#include "adb_native.h"
#include "fastboot.h"
#include "detect_fast.h"
#include "detect.h"
#include "deep_scan.h"
#include "safe.h"
#include "force.h"
#include "brute.h"
#include "menu.h"
#include "usb.h"
#include "usb_raw.h"
#include "usb_mode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ── Show comprehensive non-ADB device info ── */
static void show_nonadb_info(void) {
    DeviceInfo info;
    if (deep_scan_no_adb(&info)) {
        printf("\n" BOLD "=== Device Info (from USB + Fastboot) ===" RESET "\n");
        printf("  Vendor : %s\n", info.vendor[0] ? info.vendor : "(from USB)");
        printf("  Model  : %s\n", info.model[0] ? info.model : "(from USB)");
        printf("  Serial : %s\n", info.serial[0] ? info.serial : "(unknown)");
        if (info.chipset_vendor[0])
            printf("  Chipset: %s %s\n", info.chipset_vendor, info.chipset_name);
        if (info.detected_via[0])
            printf("  Source : %s\n", info.detected_via);
    } else {
        printf("  No USB device information available.\n");
    }
    printf("\n");
}

/* ── Fastboot interactive menu ── */
static void fastboot_menu(void) {
    printf("\n" BOLD "=== Fastboot Mode Device ===" RESET "\n");

    if (!fastboot_init()) {
        printf("  Could not initialize fastboot connection.\n");
        return;
    }

    char buf[2048];
    fastboot_get_info(buf, sizeof(buf));
    printf("%s\n", buf);

    while (1) {
        printf("\n" BOLD "Fastboot actions:" RESET "\n");
        printf("  " GREEN "1" RESET " - Continue booting\n");
        printf("  " GREEN "2" RESET " - Reboot to bootloader\n");
        printf("  " GREEN "3" RESET " - Reboot device\n");
        printf("  " GREEN "4" RESET " - Attempt bootloader unlock\n");
        printf("  " GREEN "5" RESET " - Full device info\n");
        printf("  " GREEN "q" RESET " - Exit fastboot\n");
        printf("> ");
        fflush(stdout);

        char c = getch(); printf("%c\n", c);
        if (c == '1') { fastboot_continue(); printf("[*] Continuing...\n"); break; }
        else if (c == '2') { fastboot_reboot_bootloader(); printf("[*] Sent.\n"); break; }
        else if (c == '3') { fastboot_reboot(); printf("[*] Rebooting...\n"); break; }
        else if (c == '4') {
            if (fastboot_oem_unlock() == 0) printf(GREEN "[+] Unlock sent\n" RESET);
            else printf(RED "[-] Unlock failed\n" RESET);
        }
        else if (c == '5') { fastboot_get_info(buf, sizeof(buf)); printf("%s\n", buf); }
        else if (c == 'q' || c == '0') break;
    }
    fastboot_close();
}

/* ── USB raw scan display ── */
static void raw_usb_scan(void) {
    printf("\n" BOLD "=== USB Device Scan ===" RESET "\n");
    UsbRawDevice devices[USBRAW_MAX_DEVICES];
    int ndev = usb_raw_scan_all(devices, USBRAW_MAX_DEVICES);
    if (ndev == 0) {
        printf("  No USB devices found.\n");
        return;
    }
    for (int i = 0; i < ndev; i++) {
        if (devices[i].has_adb || devices[i].has_fastboot || devices[i].has_mtp ||
            devices[i].has_rndis || devices[i].has_sideload) {
            printf("\n" BOLD "Android Device %d:" RESET "\n", i+1);
            usb_raw_print_info(&devices[i]);
        } else if (devices[i].idVendor == 0x18d1 || devices[i].idVendor == 0x04e8 ||
                   devices[i].idVendor == 0x2717 || devices[i].idVendor == 0x12d1) {
            printf("\n" BOLD "Android Device %d (charging mode):" RESET "\n", i+1);
            usb_raw_print_info(&devices[i]);

            /* Try USB mode switching */
            printf("\n" YELLOW "[!] Device in CHARGING mode only" RESET "\n");
            printf("[*] Attempting USB mode switch...\n");
            if (usb_mode_try_switch(&devices[i].dev)) {
                printf(GREEN "[+] USB mode switched!\n" RESET);
                /* Rescan */
                usb_raw_close(&devices[i]);
                usb_raw_scan_all(devices, USBRAW_MAX_DEVICES);
                if (i < ndev) usb_raw_print_info(&devices[i]);
            } else {
                printf(YELLOW "[!] USB mode switching not supported by this device.\n" RESET);
            }
        }
        usb_raw_close(&devices[i]);
    }
}

/* ── Main autorun - totally ADB-independent ── */
int autorun_main(int delay, long start, long end, int resume) {
    printf("\n" BOLD "=== BrutePin v1.1 - Auto Mode ===" RESET "\n");
    printf("[*] Scanning USB for Android devices...\n");

    /* Phase 1: USB raw scan - works on ANY device, no ADB needed */
    raw_usb_scan();

    /* Phase 2: Check fastboot */
    ModeInfo fm;
    int fastboot_found = detect_fastboot_mode(&fm);
    if (fastboot_found) {
        printf("\n" GREEN "[!] Device detected in FASTBOOT mode\n" RESET);
        fastboot_menu();
        /* After fastboot actions, recheck ADB */
        printf("[*] Rechecking ADB...\n");
        adb_native_init();
    }

    /* Phase 3: Try ADB (if available) */
    int has_adb = (adb_native_state() >= 2);
    if (!has_adb) {
        /* Try to initialize ADB */
        has_adb = adb_native_init();
    }

    if (has_adb) {
        printf(GREEN "[+] ADB device available!\n" RESET);

        if (adb_native_state() == 1) {
            printf(YELLOW "[!] Device UNAUTHORIZED - needs RSA key acceptance\n" RESET);
            printf("[*] Attempting recovery mode...\n");
            force_try_recovery();
        }

        if (adb_native_state() >= 2) {
            /* ADB available - do full scan + brute */
            printf("\n" BOLD "=== Full ADB Device Scan ===" RESET "\n");
            DeviceInfo info;
            safe_detect_all(&info);
            print_full_info(&info);

            if (!info.locked) {
                printf(GREEN "[+] Device is not locked!\n" RESET);
                return 0;
            }
            printf("\n" RED "[!] Device is LOCKED." RESET "\n");

            int plen = menu_choose_pin_length();
            if (plen == 99) {
                plen = (info.pin_len == 4 || info.pin_len == 6 || info.pin_len == 8)
                        ? info.pin_len : 6;
                printf("\n[*] Smart mode: %d-digit\n", plen);
                return brute_smart(&info, plen, delay) ? 0 : 1;
            }
            printf("\n[*] Launching %d-digit brute force...\n", plen);
            return brute_pin(plen, delay, start, end, resume, 1) ? 0 : 1;
        }
    }

    /* Phase 4: No ADB available - show non-ADB info and guidance */
    printf("\n" YELLOW "========================================" RESET "\n");
    printf(YELLOW "  ADB is NOT available on this device" RESET "\n");
    printf(YELLOW "========================================" RESET "\n");
    printf("\n");
    show_nonadb_info();

    printf("\n" BOLD "What you can do:" RESET "\n");
    printf("  " GREEN "1" RESET " - Reboot to bootloader (fastboot mode)\n");
    printf("  " GREEN "2" RESET " - Reboot to recovery mode\n");
    printf("  " GREEN "3" RESET " - Retry ADB detection\n");
    printf("  " GREEN "4" RESET " - Show USB raw info\n");
    printf("  " GREEN "q" RESET " - Quit\n");

    while (1) {
        printf("\n> ");
        fflush(stdout);
        char c = getch(); printf("%c", c);

        if (c == '1') {
            printf("\n");
            if (fastboot_found) {
                fastboot_menu();
            } else {
                printf("[*] Attempting fastboot reboot...\n");
                if (adb_native_state() >= 1) {
                    force_try_fastboot();
                } else {
                    printf("[-] Cannot send reboot command without ADB.\n");
                    printf("[*] Manually: Power off, then Power+Vol Down\n");
                }
            }
        } else if (c == '2') {
            printf("\n");
            if (adb_native_state() >= 1) {
                force_try_recovery();
            } else {
                printf("[-] Cannot send reboot command without ADB.\n");
                printf("[*] Manually: Power off, then Power+Vol Up\n");
            }
        } else if (c == '3') {
            printf("\n[*] Retrying ADB...\n");
            has_adb = adb_native_init();
            if (has_adb) {
                printf(GREEN "[+] ADB found!\n" RESET);
                DeviceInfo info;
                safe_detect_all(&info);
                print_full_info(&info);
                return 0;
            }
            printf("[-] Still no ADB.\n");
        } else if (c == '4') {
            printf("\n");
            raw_usb_scan();
        } else if (c == 'q' || c == '0') {
            printf("\n");
            break;
        }
    }

    printf("\n" BOLD "Summary:" RESET "\n");
    printf("  Device connected: YES\n");
    printf("  ADB mode       : NO\n");
    printf("  Fastboot mode  : %s\n", fastboot_found ? "YES" : "NO");
    printf("  USB mode       : CHARGING only\n");
    printf("\n" YELLOW "PIN brute force requires either:" RESET "\n");
    printf("  1. Enable ADB (accept RSA key on phone)\n");
    printf("  2. Boot to recovery with ADB\n");
    printf("  3. Use fastboot to unlock bootloader + flash custom recovery\n");
    printf("\n");

    return 0;
}
