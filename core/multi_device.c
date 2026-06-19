#define _GNU_SOURCE
#include "multi_device.h"
#include "adb.h"
#include "util.h"
#include <stdio.h>
#include <string.h>

#define MAX_DEV 16

static char devs[MAX_DEV][256];
static int ndev = 0;

int mdev_scan(void) {
    ndev = 0;
    char *out = adb_shell("adb devices -l 2>/dev/null | grep -v 'List of' | grep -v '^$'");
    trim_newline(out);
    if (strlen(out) == 0) return 0;
    char *line = out;
    while (line && *line && ndev < MAX_DEV) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = 0;
        char *serial = line;
        char *sp = strchr(line, ' ');
        if (sp) *sp = 0;
        strncpy(devs[ndev], serial, sizeof(devs[0])-1);
        ndev++;
        line = nl ? nl + 1 : NULL;
    }
    return ndev;
}

int mdev_count(void) { return ndev; }

const char* mdev_serial(int idx) {
    if (idx < 0 || idx >= ndev) return NULL;
    return devs[idx];
}

int mdev_select(int idx) {
    if (idx < 0 || idx >= ndev) return 0;
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "adb -s %s get-state 2>/dev/null", devs[idx]);
    char *out = adb_shell("%s", cmd);
    trim_newline(out);
    return strlen(out) > 0;
}

int mdev_try_all(int (*fn)(const char *serial)) {
    int ret = 0;
    for (int i = 0; i < ndev; i++) {
        printf("[*] Trying device %d/%d: %s\n", i+1, ndev, devs[i]);
        if (fn(devs[i])) ret = 1;
    }
    return ret;
}
