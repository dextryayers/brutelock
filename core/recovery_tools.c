#define _GNU_SOURCE
#include "recovery_tools.h"
#include "adb.h"
#include "adb_native.h"
#include "adb_sync.h"
#include "util.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int recovery_check(void) {
    /* Check if device has recovery ADB (sideload interface present) */
    if (adb_native_state() >= 2) {
        /* Try to access /data/system/ to confirm recovery has root */
        if (adb_sync_exists("/data/system/"))
            return 3; /* full recovery root access */
        return 2;     /* ADB works but no root = normal mode */
    }
    return 0;
}

int recovery_pull_db(const char *dest) {
    if (!dest) dest = ".";
    printf("[*] Pulling locksettings.db via native SYNC...\n");
    int found = adb_sync_pull_locksettings(dest);
    if (found > 0) {
        printf(GREEN "[+] Successfully pulled %d locksettings file(s)\n" RESET, found);
    } else {
        printf(RED "[-] Could not find locksettings.db on device\n" RESET);
        printf("[*] Listing /data/system/ contents:\n");
        recovery_list_data();
    }
    return found;
}

int recovery_list_data(void) {
    printf("[*] Listing /data/system/ from recovery...\n");
    char out[4096];
    int n = adb_sync_list("/data/system/", out, sizeof(out));
    if (n > 0) {
        printf("  %s", out);
        return 1;
    }

    /* Try /data/ */
    printf("[*] Listing /data/...\n");
    n = adb_sync_list("/data/", out, sizeof(out));
    if (n > 0) {
        printf("  %s", out);
        return 1;
    }

    return 0;
}

char* recovery_getprop(const char *prop) {
    return adb_shell("getprop %s 2>/dev/null", prop);
}

int recovery_parse_db(const char *db_path, DeviceInfo *info) {
    (void)info;
    if (!db_path) db_path = "locksettings.db";
    FILE *f = fopen(db_path, "rb");
    if (!f) { printf("[-] Cannot open %s\n", db_path); return 0; }

    printf("[*] Parsing %s for password info...\n", db_path);
    char buf[65536];
    size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);

    char *p = buf;
    while ((p = strstr(p, "sp_"))) {
        char *end = strchr(p, ' ');
        if (!end) end = buf + n;
        if (end - p < 100) {
            printf("  %.*s\n", (int)(end - p), p);
        }
        p = end + 1;
    }
    p = buf;
    while ((p = strstr(p, "password"))) {
        printf("  %s\n", p);
        p += 8;
    }
    p = buf;
    while ((p = strstr(p, "pin"))) {
        printf("  %s\n", p);
        p += 3;
    }
    return 0;
}

int recovery_extract_hash(const char *db_path, char *hash, int hash_sz) {
    if (!db_path) db_path = "locksettings.db";
    FILE *f = fopen(db_path, "rb");
    if (!f) return 0;
    char buf[65536];
    size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);

    hash[0] = 0;
    char *p = buf;
    while (p < buf + n - 4) {
        if (memcmp(p, "sp_", 3) == 0 || memcmp(p, "SALT", 4) == 0) {
            char *nl = strchr(p, '\n');
            if (!nl) nl = buf + n;
            int len = (nl - p > hash_sz - 1) ? hash_sz - 1 : (int)(nl - p);
            strncpy(hash, p, len);
            hash[len] = 0;
            return 1;
        }
        p++;
    }
    return 0;
}
