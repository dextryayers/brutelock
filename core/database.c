#include "database.h"
#include "adb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h>

static int bootloop_detected = 0;

static void parse_lock_settings(DeviceInfo *info, const char *data) {
    if (!data || strlen(data) == 0) return;

    if (strstr(data, "password")) {
        strncpy(info->pin_type, "Password", sizeof(info->pin_type)-1);
        strncpy(info->lock_type, "Password", sizeof(info->lock_type)-1);
    } else if (strstr(data, "pin") || strstr(data, "PIN")) {
        strncpy(info->pin_type, "PIN", sizeof(info->pin_type)-1);
        strncpy(info->lock_type, "PIN", sizeof(info->lock_type)-1);
    } else if (strstr(data, "pattern")) {
        strncpy(info->pin_type, "Pattern", sizeof(info->pin_type)-1);
        strncpy(info->lock_type, "Pattern", sizeof(info->lock_type)-1);
    } else {
        strncpy(info->pin_type, "None", sizeof(info->pin_type)-1);
        strncpy(info->lock_type, "None", sizeof(info->lock_type)-1);
    }

    if (strstr(data, "lockscreen.password_type=")) {
        char *p = strstr(data, "lockscreen.password_type=");
        int ptype = 0; sscanf(p, "lockscreen.password_type=%d", &ptype);
        if (ptype > 0) info->locked = 1;
    }
    if (strlen(info->pin_hash) > 0) info->locked = 1;
    if (strstr(data, "activePassword") || strstr(data, "hasPassword")) info->locked = 1;

    const char *p = strstr(data, "salt");
    if (p) {
        char *q = strchr(p, '=');
        if (q) {
            q++;
            while (*q && isspace(*q)) q++;
            char salt[64] = {0};
            int si = 0;
            while (*q && !isspace(*q) && si < 63) salt[si++] = *q++;
            salt[si] = 0;
            strncpy(info->pin_salt, salt, sizeof(info->pin_salt)-1);
        }
    }

    p = strstr(data, "hash");
    if (!p) p = strstr(data, "passwordHash");
    if (p) {
        char *q = strchr(p, '=');
        if (q) {
            q++;
            while (*q && isspace(*q)) q++;
            char hash[256] = {0};
            int hi = 0;
            while (*q && !isspace(*q) && hi < 255) hash[hi++] = *q++;
            hash[hi] = 0;
            strncpy(info->pin_hash, hash, sizeof(info->pin_hash)-1);
        }
    }

    p = strstr(data, "length");
    if (p) {
        char *q = strchr(p, '=');
        if (q) {
            q++;
            while (*q && isspace(*q)) q++;
            if (isdigit(*q)) info->pin_len = atoi(q);
        }
    }

    p = strstr(data, "backupPin");
    if (p) {
        char *q = strchr(p, '=');
        if (q) {
            q++;
            while (*q && isspace(*q)) q++;
            char bp[16] = {0};
            int bi = 0;
            while (*q && !isspace(*q) && bi < 15) bp[bi++] = *q++;
            bp[bi] = 0;
            strncpy(info->backup_pin, bp, sizeof(info->backup_pin)-1);
        }
    }

    p = strstr(data, "failedAttempts");
    if (p) {
        char *q = strchr(p, '=');
        if (q) {
            q++;
            while (*q && isspace(*q)) q++;
            if (isdigit(*q)) {
                int fa = atoi(q);
                snprintf(info->locksettings_info, sizeof(info->locksettings_info),
                         "Failed attempts: %d", fa);
            }
        }
    }

    strncpy(info->locksettings_info, data, sizeof(info->locksettings_info)-1);
}

static void try_sqlite_extract(DeviceInfo *info) {
    char *tmp;

    tmp = adb_shell("adb shell which sqlite3 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) == 0) return;

    tmp = adb_shell("adb shell sqlite3 /data/system/locksettings.db "
                    "\"SELECT name,value FROM locksettings WHERE name LIKE '%%hash%%' "
                    "OR name LIKE '%%salt%%' OR name LIKE '%%pin%%' "
                    "OR name LIKE '%%password%%' OR name LIKE '%%pattern%%' "
                    "OR name LIKE '%%lockout%%' OR name LIKE '%%failed%%'\" 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && !strstr(tmp, "not found") && !strstr(tmp, "no such")) {
        strncpy(info->pin_hash, "SQLite:", sizeof(info->pin_hash)-1);
        append_str(info->pin_hash, sizeof(info->pin_hash), "%s", tmp);

        char *p = strstr(tmp, "|");
        if (p && info->pin_len <= 0) {
            char *q = strstr(tmp, "length");
            if (q) {
                char *r = strchr(q, '|');
                if (r) {
                    r++;
                    while (*r && isspace(*r)) r++;
                    if (isdigit(*r)) info->pin_len = atoi(r);
                }
            }
        }
    }
}

void detect_database(DeviceInfo *info) {
    char *tmp;
    int found_lock = 0;

    /* Method 1: Try dumpsys lock_settings first */
    tmp = adb_shell("adb shell dumpsys lock_settings 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) == 0) {
        tmp = adb_shell("adb shell dumpsys locksettings 2>/dev/null");
        trim_newline(tmp);
    }
    if (strlen(tmp) > 0) {
        parse_lock_settings(info, tmp);
        if (info->locked) found_lock = 1;
    }

    /* Method 2: Try direct settings get for password type */
    if (!found_lock) {
        tmp = adb_shell("adb shell settings get secure lockscreen.password_type 2>/dev/null");
        trim_newline(tmp);
        if (strlen(tmp) > 0 && strcmp(tmp, "0") != 0 && strcmp(tmp, "null") != 0) {
            int ptype = atoi(tmp);
            if (ptype > 0) {
                info->locked = 1; found_lock = 1;
                if (ptype == 65536) strncpy(info->pin_type, "PIN", sizeof(info->pin_type)-1);
                else if (ptype == 131072) strncpy(info->pin_type, "Password", sizeof(info->pin_type)-1);
                else if (ptype == 262144) strncpy(info->pin_type, "Pattern", sizeof(info->pin_type)-1);
                else if (ptype == 327680) strncpy(info->pin_type, "PIN (6-digit)", sizeof(info->pin_type)-1);
                else snprintf(info->pin_type, sizeof(info->pin_type), "Type-%d", ptype);
                strncpy(info->lock_type, info->pin_type, sizeof(info->lock_type)-1);
            }
        }
    }

    /* Method 3: Check gatekeeper files existence */
    if (!found_lock) {
        tmp = adb_shell("adb shell ls /data/system/gatekeeper.password.key 2>/dev/null && echo EXISTS || echo NOKEY");
        trim_newline(tmp);
        if (strstr(tmp, "EXISTS")) {
            info->locked = 1; found_lock = 1;
            strncpy(info->pin_type, "PIN", sizeof(info->pin_type)-1);
            strncpy(info->lock_type, "PIN", sizeof(info->lock_type)-1);
        }
        tmp = adb_shell("adb shell ls /data/system/gatekeeper.pattern.key 2>/dev/null && echo EXISTS || echo NOKEY");
        trim_newline(tmp);
        if (strstr(tmp, "EXISTS") && !info->locked) {
            info->locked = 1; found_lock = 1;
            strncpy(info->pin_type, "Pattern", sizeof(info->pin_type)-1);
            strncpy(info->lock_type, "Pattern", sizeof(info->lock_type)-1);
        }
    }

    /* Method 4: Check gesture.key (legacy) */
    if (!found_lock) {
        tmp = adb_shell("adb shell ls /data/system/gesture.key 2>/dev/null && echo EXISTS || echo NOKEY");
        trim_newline(tmp);
        if (strstr(tmp, "EXISTS")) {
            info->locked = 1; found_lock = 1;
            strncpy(info->pin_type, "Pattern", sizeof(info->pin_type)-1);
            strncpy(info->lock_type, "Pattern", sizeof(info->lock_type)-1);
        }
    }

    try_sqlite_extract(info);

    /* If dumpsys returned nothing AND getprop returns nothing -> bootloop */
    if (strlen(info->pin_type) == 0) {
        char *test_getprop = adb_shell("adb shell getprop ro.product.model 2>/dev/null");
        trim_newline(test_getprop);
        if (strlen(test_getprop) == 0) {
            bootloop_detected = 1;
            info->locked = 1;
            strncpy(info->pin_type, "Unknown", sizeof(info->pin_type)-1);
            strncpy(info->lock_type, "Locked (bootloop)", sizeof(info->lock_type)-1);
        }
    }

    tmp = adb_shell("adb shell dumpsys lock_settings 2>/dev/null | grep -iE 'failed|lockout|attempt'");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        append_str(info->locksettings_info, sizeof(info->locksettings_info),
                   " | %s", tmp);
    }
}

void print_database_info(const DeviceInfo *info) {
    printf("  " BOLD "%-16s" RESET ": %s\n", "Lock Type", info->pin_type);
    if (info->pin_len > 0) printf("  " BOLD "%-16s" RESET ": %d-digit\n", "PIN Length", info->pin_len);
    if (strlen(info->pin_hash)) printf("  " BOLD "%-16s" RESET ": %s\n", "PIN Hash", info->pin_hash);
    if (strlen(info->pin_salt)) printf("  " BOLD "%-16s" RESET ": %s\n", "PIN Salt", info->pin_salt);
    if (strlen(info->backup_pin)) printf("  " BOLD "%-16s" RESET ": %s\n", "Backup PIN", info->backup_pin);
    if (strlen(info->locksettings_info)) printf("  " BOLD "%-16s" RESET ": %s\n", "Lock Settings", info->locksettings_info);
    if (bootloop_detected) printf("  " BOLD "%-16s" RESET ": %s\n", "Bootloop", "Yes - shell commands failing");
}
