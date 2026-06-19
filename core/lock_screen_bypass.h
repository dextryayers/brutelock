#ifndef LOCK_SCREEN_BYPASS_H
#define LOCK_SCREEN_BYPASS_H

#include "util.h"

#define LS_MAX_METHODS 32
#define LS_MAX_OUTPUT 8192

typedef enum {
    BYPASS_NONE = 0,
    BYPASS_EMERGENCY_CALL,
    BYPASS_CAMERA,
    BYPASS_ACCESSIBILITY,
    BYPASS_SYSTEM_UI_TUNER,
    BYPASS_POWER_MENU,
    BYPASS_NOTIFICATIONS,
    BYPASS_GOOGLE_NOW,
    BYPASS_VOICE_ASSISTANT,
    BYPASS_BIOMETRICS_FAILOVER,
    BYPASS_SAMSUNG_EMERGENCY,
    BYPASS_XIAOMI_VULN,
    BYPASS_OPPO_ENGINEERING,
    BYPASS_VIVO_VULN,
    BYPASS_HUAWEI_VULN,
    BYPASS_CVE_EXPLOIT,
    BYPASS_SMUDGE_ATTACK,
    BYPASS_RECOVERY_DATA,
    BYPASS_ADB_VULN,
    BYPASS_ACTIVITY_INJECTION,
    BYPASS_OVERLAY_ATTACK
} BypassMethod;

typedef struct {
    BypassMethod method;
    char method_name[48];
    char description[128];
    int works_on_locked;
    int requires_adb;
    int requires_physical_access;
    int success;
    char result[512];
    int min_sdk;
    int max_sdk;
} BypassEntry;

typedef struct {
    char vendor[64];
    char model[128];
    char android_version[32];
    int sdk_version;
    char security_patch[32];
    char build_fingerprint[256];
    char lock_type[32];          /* "PIN", "Password", "Pattern", "None" */
    int locked;
    char device_serial[64];
    char ro_debuggable[16];

    BypassEntry methods[LS_MAX_METHODS];
    int nmethods;
    int any_success;
    char last_success_method[64];
    char bypass_result[LS_MAX_OUTPUT];
    char exploit_log[4096];
    int log_len;
} LockScreenBypass;

void ls_init(LockScreenBypass *ls);
void ls_log(LockScreenBypass *ls, const char *fmt, ...);

/* Detect lock screen type */
int ls_detect_lock_type(LockScreenBypass *ls);

/* Check if device is vulnerable to specific bypass methods */
int ls_check_vulnerabilities(LockScreenBypass *ls);

/* Method 1: Emergency call -> activity injection (Android 4.x-6.x) */
int ls_emergency_call_bypass(LockScreenBypass *ls);

/* Method 2: Camera app from lock screen (Android 5.x-7.x) */
int ls_camera_bypass(LockScreenBypass *ls);

/* Method 3: Accessibility service exploit */
int ls_accessibility_bypass(LockScreenBypass *ls);

/* Method 4: System UI Tuner (Android 6.x-7.x) */
int ls_system_ui_tuner_bypass(LockScreenBypass *ls);

/* Method 5: Power menu + emergency (Android 4.x-8.x) */
int ls_power_menu_bypass(LockScreenBypass *ls);

/* Method 6: Notification panel bypass */
int ls_notification_bypass(LockScreenBypass *ls);

/* Method 7: Google Now/Assistant bypass */
int ls_google_now_bypass(LockScreenBypass *ls);

/* Method 8: Voice Assistant bypass */
int ls_voice_assistant_bypass(LockScreenBypass *ls);

/* Method 9: Samsung-specific exploits */
int ls_samsung_emergency_bypass(LockScreenBypass *ls);
int ls_samsung_accessibility_bypass(LockScreenBypass *ls);

/* Method 10: Xiaomi MIUI-specific exploits */
int ls_xiaomi_vuln_bypass(LockScreenBypass *ls);
int ls_xiaomi_engineering_bypass(LockScreenBypass *ls);

/* Method 11: OPPO/Realme engineering mode */
int ls_oppo_engineering_bypass(LockScreenBypass *ls);

/* Method 12: Vivo vuln bypass */
int ls_vivo_vuln_bypass(LockScreenBypass *ls);

/* Method 13: Huawei vuln bypass */
int ls_huawei_vuln_bypass(LockScreenBypass *ls);

/* Method 14: Known CVE-based bypasses */
int ls_cve_2020_exploit(LockScreenBypass *ls);
int ls_cve_2021_exploit(LockScreenBypass *ls);
int ls_cve_2022_exploit(LockScreenBypass *ls);

/* Method 15: Smudge attack (pattern unlock visualization) */
int ls_smudge_attack(LockScreenBypass *ls);

/* Method 16: Activity injection via ADB (if available) */
int ls_activity_injection(LockScreenBypass *ls);

/* Method 17: Overlay attack */
int ls_overlay_attack(LockScreenBypass *ls);

/* Auto-run all methods */
int ls_full_bypass(LockScreenBypass *ls);

/* Get lock screen info for brute force */
int ls_get_brute_info(LockScreenBypass *ls, char *out, int out_sz);
int ls_get_password_hash(LockScreenBypass *ls, char *hash_out, int out_sz);

void ls_print_summary(LockScreenBypass *ls);
void ls_print_methods(LockScreenBypass *ls);

#endif
