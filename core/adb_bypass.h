#ifndef ADB_BYPASS_H
#define ADB_BYPASS_H

#include "util.h"

#define AB_MAX_METHODS 16

typedef enum {
    AB_NONE = 0,
    AB_RECOVERY_ADB,      /* ADB via recovery mode */
    AB_RSA_KEY_INJECT,    /* Inject known RSA public keys */
    AB_PROTO_DOWNGRADE,   /* Force ADB protocol v1 (no auth) */
    AB_USB_MODE_SWITCH,   /* Switch USB mode to force ADB */
    AB_ROOT_SHELL,        /* Root shell via exploit */
    AB_WIRELESS_ADB,      /* ADB over TCP (port 5555) */
    AB_PAIRING_CRACK,     /* Brute-force ADB pairing code */
    AB_FASTBOOT_BOOT,     /* Boot modified boot.img via fastboot */
    AB_EDL_MODE,          /* Qualcomm EDL mode access */
    AB_KNOWN_KEY         /* Try known ADB private keys */
} AdbBypassMethod;

typedef struct {
    AdbBypassMethod methods[AB_MAX_METHODS];
    int nmethods;
    int success_method;
    char adb_key_path[256];
    char last_error[256];
    char log[4096];
    int log_len;
    int adb_state_before;
    int adb_state_after;
    int attempts;
    int max_attempts;
} AdbBypass;

void ab_init(AdbBypass *ab);
void ab_log(AdbBypass *ab, const char *fmt, ...);
int ab_run_all(AdbBypass *ab);

/* Individual bypass methods */
int ab_bypass_recovery_adb(AdbBypass *ab);
int ab_bypass_rsa_key_inject(AdbBypass *ab);
int ab_bypass_proto_downgrade(AdbBypass *ab);
int ab_bypass_usb_mode_switch(AdbBypass *ab);
int ab_bypass_root_shell(AdbBypass *ab);
int ab_bypass_wireless_adb(AdbBypass *ab);
int ab_bypass_fastboot_boot(AdbBypass *ab);
int ab_bypass_edl_mode(AdbBypass *ab);

/* RSA key management */
int ab_generate_rsa_key(const char *path);
int ab_push_rsa_key(AdbBypass *ab, const char *key_path);

/* ADB state helpers */
int ab_is_authorized(void);
int ab_wait_for_auth(int timeout_sec);
void ab_print_status(AdbBypass *ab);

#endif
