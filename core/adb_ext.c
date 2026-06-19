#define _GNU_SOURCE
#include "adb_ext.h"
#include "adb.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char* shell(const char *cmd) {
    static char result[4096];
    result[0] = 0;
    FILE *fp = popen(cmd, "r");
    if (!fp) return result;
    size_t pos = 0;
    while (fgets(result + pos, sizeof(result) - pos, fp)) {
        pos = strlen(result);
        if (pos >= sizeof(result) - 1) break;
    }
    pclose(fp);
    return result;
}

static void run_bg(const char *cmd) {
    char buf[8192];
    snprintf(buf, sizeof(buf), "%s 2>/dev/null &", cmd);
    FILE *fp = popen(buf, "r");
    if (fp) pclose(fp);
}

int adb_find_device(void) {
    char *devs = shell("adb devices -l 2>/dev/null | grep -v 'List of' | grep -v '^$'");
    trim_newline(devs);
    if (strlen(devs) > 0) {
        printf("[+] Devices:\n%s\n", devs);
        return 1;
    }
    devs = shell("fastboot devices 2>/dev/null | grep -v '^$' | head -3");
    trim_newline(devs);
    if (strlen(devs) > 0) {
        printf("[+] Fastboot devices:\n%s\n", devs);
        return 2;
    }
    return 0;
}

int adb_force_recovery(void) {
    printf("[*] Rebooting to recovery mode...\n");
    adb_shell_noret("adb reboot recovery");
    sleep(5);
    int waited = 0;
    while (waited < 30) {
        int st = adb_state();
        if (st >= 2) { printf("[+] Recovery ADB ready.\n"); return 1; }
        if (st == 1) { printf("[!] Recovery ADB unauthorized.\n"); return -1; }
        printf("."); fflush(stdout);
        sleep(2);
        waited += 2;
    }
    printf("\n");
    return 0;
}

int adb_force_fastboot(void) {
    printf("[*] Rebooting to bootloader...\n");
    adb_shell_noret("adb reboot bootloader");
    sleep(5);
    int waited = 0;
    while (waited < 30) {
        char *devs = shell("fastboot devices 2>/dev/null | grep -v '^$' | head -1");
        trim_newline(devs);
        if (strlen(devs) > 0) {
            printf("[+] Fastboot mode: %s\n", devs);
            return 1;
        }
        printf("."); fflush(stdout);
        sleep(2);
        waited += 2;
    }
    printf("\n[-] No fastboot device.\n");
    return 0;
}

int adb_is_fastboot(void) {
    char *devs = shell("fastboot devices 2>/dev/null | grep -v '^$' | head -1");
    trim_newline(devs);
    return strlen(devs) > 0 ? 1 : 0;
}

char* adb_fastboot_cmd(const char *cmd) {
    char buf[8192];
    snprintf(buf, sizeof(buf), "fastboot %s 2>/dev/null", cmd);
    return shell(buf);
}

int adb_authorize_loop(int timeout) {
    printf("[*] Trying to authorize ADB (kill-server loop)...\n");
    int waited = 0;
    while (waited < timeout) {
        run_bg("adb kill-server");
        usleep(200000);
        run_bg("adb start-server");
        usleep(500000);
        int st = adb_state();
        if (st >= 2) { printf("[+] ADB authorized.\n"); return 1; }
        printf("."); fflush(stdout);
        sleep(1);
        waited++;
    }
    printf("\n");
    return 0;
}

void adb_list_devices(void) {
    printf("[*] USB devices:\n");
    char *usb = shell("lsusb 2>/dev/null | grep -iE 'android|google|mtk|qualcomm|samsung|xiaomi|vivo|oppo|huawei|oneplus' | head -10");
    trim_newline(usb);
    if (strlen(usb) > 0) printf("%s\n", usb);
    printf("[*] ADB devices:\n");
    char *adb = shell("adb devices -l 2>/dev/null");
    trim_newline(adb);
    if (strlen(adb) > 0) printf("%s\n", adb);
    printf("[*] Fastboot devices:\n");
    char *fb = shell("fastboot devices 2>/dev/null");
    trim_newline(fb);
    if (strlen(fb) > 0) printf("%s\n", fb);
}
