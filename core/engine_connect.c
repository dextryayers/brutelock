#define _GNU_SOURCE
#include "engine_connect.h"
#include "adb_native.h"
#include "usb_raw.h"
#include "detect.h"
#include "usb_mode.h"
#include "deep_scan.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>

#define ANDROID_VENDORS "18d1 04e8 2717 12d1 22d9 2d95 1004 054c 22b8 0421 2a70 0e8d 05c6"

static int is_android_vid(int vid) {
    int vals[] = {0x18d1,0x04e8,0x2717,0x12d1,0x22d9,0x2d95,0x1004,0x054c,0x22b8,0x0421,0x2a70,0x0e8d,0x05c6,0};
    for (int i = 0; vals[i]; i++) if (vid == vals[i]) return 1;
    return 0;
}

void conn_init(ConnInfo *ci) {
    memset(ci, 0, sizeof(*ci));
    ci->state = CONN_DISCONNECTED;
    pthread_mutex_init(&ci->lock, NULL);
}

int conn_scan_usb(ConnInfo *ci) {
    struct dirent *entry;
    DIR *dp = opendir("/sys/bus/usb/devices");
    if (!dp) return 0;

    while ((entry = readdir(dp)) != NULL) {
        if (strchr(entry->d_name, '-') == NULL &&
            strchr(entry->d_name, ':') == NULL &&
            strcmp(entry->d_name, "usb") != 0) continue;

        char path[512];
        snprintf(path, sizeof(path), "/sys/bus/usb/devices/%s/idVendor", entry->d_name);
        FILE *f = fopen(path, "r");
        if (!f) continue;
        int vid = 0;
        if (fscanf(f, "%x", &vid) != 1) { fclose(f); continue; }
        fclose(f);

        if (!is_android_vid(vid)) continue;

        snprintf(path, sizeof(path), "/sys/bus/usb/devices/%s/idProduct", entry->d_name);
        f = fopen(path, "r"); int pid = 0;
        if (f) { fscanf(f, "%x", &pid); fclose(f); }

        snprintf(path, sizeof(path), "/sys/bus/usb/devices/%s/manufacturer", entry->d_name);
        f = fopen(path, "r");
        if (f) { fgets(ci->vendor, sizeof(ci->vendor), f); trim_newline(ci->vendor); fclose(f); }

        snprintf(path, sizeof(path), "/sys/bus/usb/devices/%s/product", entry->d_name);
        f = fopen(path, "r");
        if (f) { fgets(ci->model, sizeof(ci->model), f); trim_newline(ci->model); fclose(f); }

        snprintf(path, sizeof(path), "/sys/bus/usb/devices/%s/serial", entry->d_name);
        f = fopen(path, "r");
        if (f) { fgets(ci->serial, sizeof(ci->serial), f); trim_newline(ci->serial); fclose(f); }

        closedir(dp);

        pthread_mutex_lock(&ci->lock);
        ci->state = CONN_USB_DETECTED;
        pthread_mutex_unlock(&ci->lock);
        return 1;
    }
    closedir(dp);
    return 0;
}

int conn_wait_usb(ConnInfo *ci, int timeout_s) {
    int waited = 0;
    while (waited < timeout_s) {
        if (conn_scan_usb(ci)) return 1;
        sleep(2);
        waited += 2;
    }
    return 0;
}

int conn_init_adb(ConnInfo *ci) {
    int ret = adb_native_init();
    pthread_mutex_lock(&ci->lock);
    ci->adb_state = adb_native_state();
    if (ret && ci->adb_state >= 2) {
        ci->state = CONN_READY;
        /* Cache screen dimensions */
        char out[256];
        if (adb_native_shell("wm size", out, sizeof(out)) == 0) {
            char *val = strchr(out, ':');
            if (val) {
                val++;
                while (*val == ' ') val++;
                int w = atoi(val);
                char *x = strchr(val, 'x');
                if (x) {
                    x++;
                    int h = atoi(x);
                    if (w > 0 && w < 10000) ci->screen_width = w;
                    if (h > 0 && h < 10000) ci->screen_height = h;
                }
            }
        }
        if (ci->screen_width <= 0) ci->screen_width = 1080;
        if (ci->screen_height <= 0) ci->screen_height = 2400;
    } else if (ret && ci->adb_state == 1) {
        ci->state = CONN_UNAUTHORIZED;
    }
    pthread_mutex_unlock(&ci->lock);
    return ci->state == CONN_READY;
}

int conn_auto(ConnInfo *ci, int timeout_s) {
    conn_init(ci);
    if (!conn_wait_usb(ci, timeout_s)) return 0;
    if (!conn_init_adb(ci)) {
        /* Try USB mode switching */
        UsbRawDevice devs[USBRAW_MAX_DEVICES];
        int ndev = usb_raw_scan_all(devs, USBRAW_MAX_DEVICES);
        for (int i = 0; i < ndev; i++) {
            if (devs[i].idVendor == 0x18d1 || devs[i].idVendor == 0x04e8) {
                usb_mode_try_switch(&devs[i].dev);
                usb_raw_close(&devs[i]);
                break;
            }
            usb_raw_close(&devs[i]);
        }
        sleep(1);
        conn_init_adb(ci);
    }
    return ci->state == CONN_READY;
}

void conn_poll(ConnInfo *ci) {
    int st = adb_native_state();
    pthread_mutex_lock(&ci->lock);
    ci->adb_state = st;
    if (st < 2 && ci->state == CONN_READY) ci->state = CONN_LOST;
    if (st >= 2 && ci->state == CONN_LOST) ci->state = CONN_READY;
    pthread_mutex_unlock(&ci->lock);
}

void conn_status_str(ConnInfo *ci, char *out, int out_sz) {
    const char *states[] = {
        [CONN_DISCONNECTED] = RED "DISCONNECTED" RESET,
        [CONN_USB_DETECTED] = YELLOW "USB DETECTED" RESET,
        [CONN_ADB_INIT] = YELLOW "ADB INIT" RESET,
        [CONN_UNAUTHORIZED] = RED "UNAUTHORIZED" RESET,
        [CONN_AUTHORIZED] = GREEN "AUTHORIZED" RESET,
        [CONN_READY] = GREEN "READY" RESET,
        [CONN_LOST] = RED "LOST" RESET
    };
    pthread_mutex_lock(&ci->lock);
    snprintf(out, out_sz, "%s %s:%s [%dx%d] | %s",
             states[ci->state] ? states[ci->state] : "?",
             ci->vendor, ci->model,
             ci->screen_width, ci->screen_height,
             ci->locked ? "LOCKED" : "UNLOCKED");
    pthread_mutex_unlock(&ci->lock);
}

void conn_close(ConnInfo *ci) {
    adb_native_close();
    ci->state = CONN_DISCONNECTED;
    pthread_mutex_destroy(&ci->lock);
}
