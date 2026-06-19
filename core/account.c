#include "account.h"
#include "adb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void detect_accounts(DeviceInfo *info) {
    char *tmp;

    tmp = adb_shell("adb shell dumpsys account 2>/dev/null | grep -E 'Account|name=|email|type=|user=' | head -20");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->google_accounts, tmp, sizeof(info->google_accounts)-1);

    if (strlen(info->google_accounts) == 0) {
        tmp = adb_shell("adb shell content query --uri content://settings/secure --projection name:value --where \"name LIKE 'account%%'\" 2>/dev/null | head -10");
        trim_newline(tmp);
        if (strlen(tmp) > 0 && !strstr(tmp, "Error")) strncpy(info->google_accounts, tmp, sizeof(info->google_accounts)-1);
    }

    if (strlen(info->google_accounts) == 0) {
        tmp = adb_shell("adb shell settings list secure 2>/dev/null | grep -i account | head -10");
        trim_newline(tmp);
        if (strlen(tmp) > 0) strncpy(info->google_accounts, tmp, sizeof(info->google_accounts)-1);
    }

    if (strlen(info->google_accounts) == 0) {
        tmp = adb_shell("adb shell dumpsys user 2>/dev/null | grep -iE 'UserInfo|name:|account' | head -10");
        trim_newline(tmp);
        if (strlen(tmp) > 0) strncpy(info->google_accounts, tmp, sizeof(info->google_accounts)-1);
    }

    tmp = adb_shell("adb shell dumpsys account 2>/dev/null | grep -i samsung | head -5");
    trim_newline(tmp);
    if (strlen(tmp) > 0)
        append_str(info->google_accounts, sizeof(info->google_accounts), "\n  [Samsung] %s", tmp);

    tmp = adb_shell("adb shell dumpsys account 2>/dev/null | grep -i -E 'xiaomi|mi account' | head -5");
    trim_newline(tmp);
    if (strlen(tmp) > 0)
        append_str(info->google_accounts, sizeof(info->google_accounts), "\n  [Xiaomi] %s", tmp);

    tmp = adb_shell("adb shell dumpsys account 2>/dev/null | grep -i -E 'huawei|huawei id' | head -5");
    trim_newline(tmp);
    if (strlen(tmp) > 0)
        append_str(info->google_accounts, sizeof(info->google_accounts), "\n  [Huawei] %s", tmp);

    tmp = adb_shell("adb shell pm list users 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && (strstr(tmp, "140") || strstr(tmp, "Work")))
        append_str(info->google_accounts, sizeof(info->google_accounts), "\n  [Work Profile] detected");

    if (strlen(info->google_accounts) > 0) {
        tmp = adb_shell("adb shell dumpsys account 2>/dev/null | grep -c 'Account {'");
        trim_newline(tmp);
        int acct_count = atoi(tmp);
        if (acct_count > 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "\n  Total: %d account(s)", acct_count);
            append_str(info->google_accounts, sizeof(info->google_accounts), "%s", buf);
        }
    }

    if (strlen(info->google_accounts) == 0) {
        tmp = adb_shell("adb shell getprop persist.sys.account 2>/dev/null");
        trim_newline(tmp);
        if (strlen(tmp) > 0) snprintf(info->google_accounts, sizeof(info->google_accounts), "Acct: %s", tmp);
    }

    tmp = adb_shell("adb shell dumpsys device_policy 2>/dev/null | grep -i 'frp' | head -5");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->frp_status, tmp, sizeof(info->frp_status)-1);
    else {
        tmp = adb_shell("adb shell settings get global device_provisioned 2>/dev/null");
        trim_newline(tmp);
        if (strcmp(tmp, "1") == 0) strncpy(info->frp_status, "Provisioned (FRP inactive)", sizeof(info->frp_status)-1);
        else if (strcmp(tmp, "0") == 0) strncpy(info->frp_status, "NOT provisioned (FRP active)", sizeof(info->frp_status)-1);
        else {
            tmp = adb_shell("adb shell getprop ro.vendor.frp 2>/dev/null");
            trim_newline(tmp);
            if (strlen(tmp) > 0) snprintf(info->frp_status, sizeof(info->frp_status), "FRP: %s", tmp);
        }
    }

    tmp = adb_shell("adb shell dumpsys device_policy 2>/dev/null | grep -i 'owner' | head -5");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->device_owner, tmp, sizeof(info->device_owner)-1);

    tmp = adb_shell("adb shell settings get secure backup_pin 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->backup_account, tmp, sizeof(info->backup_account)-1);
}

void print_account_info(const DeviceInfo *info) {
    if (strlen(info->google_accounts)) {
        printf("  " BOLD "%-17s" RESET ":\n", "Accounts");
        char copy[BIG_STR];
        strncpy(copy, info->google_accounts, sizeof(copy)-1);
        char *line = strtok(copy, "\n");
        while (line) { printf("    %s\n", line); line = strtok(NULL, "\n"); }
    } else {
        printf("  " BOLD "%-17s" RESET ": None detected\n", "Accounts");
    }
    if (strlen(info->frp_status)) printf("  " BOLD "%-17s" RESET ": %s\n", "FRP", info->frp_status);
    if (strlen(info->device_owner)) printf("  " BOLD "%-17s" RESET ": %s\n", "Device Owner", info->device_owner);
}
