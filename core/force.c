#define _GNU_SOURCE
#include "force.h"
#include "adb.h"
#include "adb_native.h"
#include "fastboot.h"
#include "util.h"
#include "detect.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int force_connect(int offer_recovery) {
    int st = adb_state();
    if (st >= 2) return 1;

    if (st == 1) {
        printf("[!] Device is UNAUTHORIZED.\n");
        if (offer_recovery) {
            printf("[*] Trying recovery mode...\n");
            return force_try_recovery();
        }
        return -1;
    }

    if (st == 0) {
        printf("[-] No ADB device.\n");

        /* Check fastboot */
        FastbootInfo fb;
        if (fastboot_detect(&fb) > 0) {
            printf("[!] Device in FASTBOOT mode.\n");
            if (offer_recovery) {
                printf("[*] Continuing boot to reach Android...\n");
                if (fastboot_init()) {
                    fastboot_continue();
                    fastboot_close();
                    sleep(5);
                    /* Recheck ADB */
                    adb_init();
                    st = adb_state();
                    if (st >= 2) return 1;
                }
            }
            return -1;
        }

        if (offer_recovery) {
            printf("[*] Trying recovery mode...\n");
            return force_try_recovery();
        }
    }
    return 0;
}

int force_try_recovery(void) {
    printf("[*] Rebooting to recovery...\n");

    if (adb_native_shell_noret("reboot recovery") < 0) {
        /* Try fastboot boot to recovery if ADB not working */
        printf("[*] ADB shell failed. Checking fastboot...\n");
        if (fastboot_init()) {
            fastboot_reboot();
            fastboot_close();
        }
    }

    sleep(5);
    int waited = 0;
    while (waited < 30) {
        adb_native_close();
        sleep(1);
        int st = adb_native_init();
        if (st) {
            st = adb_native_state();
            if (st >= 2) { printf("[+] Recovery ADB ready.\n"); return 1; }
            if (st == 1) { printf("[!] Recovery ADB unauthorized.\n"); return -1; }
        }
        printf("."); fflush(stdout);
        sleep(2);
        waited += 2;
    }
    printf("\n[-] Recovery ADB not available.\n");
    return 0;
}

int force_try_fastboot(void) {
    printf("[*] Rebooting to bootloader...\n");

    if (adb_native_shell_noret("reboot bootloader") < 0) {
        /* Direct USB reset to trigger bootloader */
        printf("[*] ADB shell failed. Direct USB not possible.\n");
        return 0;
    }

    sleep(5);
    int waited = 0;
    while (waited < 30) {
        ModeInfo mi;
        if (detect_fastboot_mode(&mi)) {
            printf("[+] Fastboot device: %s\n", mi.serial[0] ? mi.serial : "unknown");
            printf("[*] Use: bootloader unlock via --fastboot command\n");
            return 1;
        }
        printf("."); fflush(stdout);
        sleep(2);
        waited += 2;
    }
    printf("\n[-] No fastboot device.\n");
    return 0;
}

int force_is_recovery(void) {
    /* Check if current device has sideload interface = recovery */
    ModeInfo modes[4];
    int n = detect_all_modes(modes, 4);
    for (int i = 0; i < n; i++) {
        if (modes[i].recovery_available && modes[i].mode == MODE_RECOVERY)
            return 1;
    }
    return 0;
}

int force_is_fastboot(void) {
    return fastboot_detect(NULL) > 0;
}
