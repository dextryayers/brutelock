#ifndef ADB_AUTH_BYPASS_H
#define ADB_AUTH_BYPASS_H

#include "util.h"

#define MAX_ADB_KEYS 64
#define ADB_KEY_PATH_HOST "~/.android/adbkey.pub"
#define ADB_KEY_PATH_DEVICE "/data/misc/adb/adb_keys"
#define ADB_KEY_PATH_SYSTEM "/data/misc/adb/adb_keys"

typedef enum {
    AUTH_NONE = 0,
    AUTH_RSA_ACCEPTED,
    AUTH_RSA_BRUTEFORCE,
    AUTH_RESTORE_ATTACK,
    AUTH_DOWNGRADE,
    AUTH_RECOVERY_ADB,
    AUTH_FASTBOOT_ADB,
    AUTH_CVE_EXPLOIT,
    AUTH_SHELL_INJECTION,
    AUTH_KEY_INJECTION,
    AUTH_ENGINEERING_MODE,
    AUTH_VULN_TEST
} AuthMethod;

typedef struct {
    AuthMethod method;
    char method_name[64];
    int success;
    char result[512];
    double time_taken;
} AuthAttempt;

typedef struct {
    int available;
    char device_serial[64];
    char android_version[32];
    int sdk_version;
    char build_fingerprint[256];
    char ro_debuggable[16];
    char ro_secure[16];
    char ro_adb_secure[16];
    char adb_keys_protected[16];

    AuthAttempt attempts[32];
    int nattempts;
    int adb_unlocked;

    /* RSA keys */
    char local_pubkey[4096];
    char local_privkey_path[512];
    char injected_key[4096];
} AdbAuthBypass;

void auth_init(AdbAuthBypass *ab);
void auth_load_local_keys(AdbAuthBypass *ab);

/* Method 1: Brute-force RSA key acceptance (send many keys) */
int auth_rsa_bruteforce(AdbAuthBypass *ab);

/* Method 2: ADB restore attack — inject ADB keys via backup restore */
int auth_restore_attack(AdbAuthBypass *ab);

/* Method 3: Downgrade attack — force ADB to non-secure mode */
int auth_downgrade_attack(AdbAuthBypass *ab);

/* Method 4: Recovery ADB (no auth needed) */
int auth_recovery_adb(AdbAuthBypass *ab);
int auth_boot_recovery_with_adb(AdbAuthBypass *ab);

/* Method 5: Fastboot boot modified boot image with ADB */
int auth_fastboot_adb(AdbAuthBypass *ab);

/* Method 6: Known CVE exploits per Android version */
int auth_cve_exploit(AdbAuthBypass *ab);

/* Method 7: Shell injection via USB control */
int auth_shell_injection(AdbAuthBypass *ab);

/* Method 8: Key injection via /data/misc/adb */
int auth_key_injection(AdbAuthBypass *ab);

/* Method 9: Engineering mode ADB (MTK/Samsung/Qualcomm) */
int auth_engineering_mode(AdbAuthBypass *ab);

/* Full auto-attack chain */
int auth_full_bypass(AdbAuthBypass *ab);

/* Check if ADB is now available */
int auth_check_adb_available(AdbAuthBypass *ab);

void auth_print_status(AdbAuthBypass *ab);
void auth_print_report(AdbAuthBypass *ab);

#endif
