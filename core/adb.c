#define _GNU_SOURCE
#include "adb.h"
#include "adb_native.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>

/* Native-only ADB layer - no external "adb" binary needed */

int adb_init(void) {
    int ret = adb_native_init();
    return ret;
}

int adb_check(void) {
    return adb_native_check();
}

int adb_state(void) {
    return adb_native_state();
}

int adb_wait(int timeout) {
    int waited = 0;
    while (timeout == 0 || waited < timeout) {
        int st = adb_native_state();
        if (st >= 2) { printf("\n[+] ADB device ready.\n"); return 1; }
        if (st == 1) {
            printf("\n[!] Device found but UNAUTHORIZED.\n");
            printf("[*] Accept RSA key on phone or use recovery mode.\n");
            return -1;
        }
        if (waited == 0) printf("[*] Waiting for ADB device");
        printf("."); fflush(stdout);
        sleep(2);
        waited += 2;
    }
    printf("\n");
    return 0;
}

int adb_wait_any(int timeout) {
    return adb_wait(timeout);
}

void adb_recover(void) {
    adb_native_close();
    sleep(1);
    adb_native_init();
}

char* adb_shell(const char *fmt, ...) {
    static char result[16384];
    result[0] = 0;
    if (adb_state() < 2) return result;

    static char cmd[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, ap);
    va_end(ap);

    /* Strip "adb shell " prefix if present */
    char *p = strstr(cmd, "adb shell ");
    if (p) {
        char *actual = p + 10;
        while (*actual == ' ') actual++;
        adb_native_shell(actual, result, sizeof(result));
        return result;
    }

    /* Strip "timeout N adb shell " prefix */
    p = strstr(cmd, "timeout");
    if (p) {
        p = strstr(p, "adb shell ");
        if (p) {
            char *actual = p + 10;
            while (*actual == ' ') actual++;
            adb_native_shell(actual, result, sizeof(result));
            return result;
        }
    }

    /* Plain command - try as direct shell command */
    adb_native_shell(cmd, result, sizeof(result));
    return result;
}

void adb_shell_noret(const char *fmt, ...) {
    static char cmd[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, ap);
    va_end(ap);

    /* Strip prefixes */
    char *p = strstr(cmd, "adb shell ");
    if (p) {
        char *actual = p + 10;
        while (*actual == ' ') actual++;
        adb_native_shell_noret(actual);
        return;
    }
    p = strstr(cmd, "timeout");
    if (p) {
        p = strstr(p, "adb shell ");
        if (p) {
            char *actual = p + 10;
            while (*actual == ' ') actual++;
            adb_native_shell_noret(actual);
            return;
        }
    }
    adb_native_shell_noret(cmd);
}

char* adb_getprop(const char *prop) {
    static char result[4096];
    if (adb_state() < 2) {
        result[0] = 0;
        return result;
    }
    adb_native_getprop(prop, result, sizeof(result));
    return result;
}

int adb_shell_works(void) {
    char out[256];
    return adb_native_shell("echo OK", out, sizeof(out)) == 0 &&
           strcmp(out, "OK\n") == 0;
}

/* ── wake / screen ── */
void adb_wakeup(void) {
    for (int i = 0; i < 3; i++) {
        adb_shell_noret("input keyevent 26");
        usleep(200000);
    }
    usleep(200000);
    adb_shell_noret("input keyevent 224");
    usleep(200000);
}

void adb_screen_on(void) {
    adb_shell_noret("input keyevent 26");
    usleep(400000);
}

void adb_swipe(void) {
    adb_shell_noret("input swipe 300 1000 300 300");
    usleep(400000);
}

void adb_swipe_smart(void) {
    int h = adb_get_screen_h();
    int y_start = h * 75 / 100;
    int y_end   = h * 15 / 100;
    int x_center = adb_get_screen_w() / 2;
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "input swipe %d %d %d %d",
             x_center, y_start, x_center, y_end);
    adb_shell_noret("%s", cmd);
    usleep(300000);
}

/* ── screen size ── */
int adb_get_screen_w(void) {
    static int cached_w = 0;
    if (cached_w > 0) return cached_w;
    char out[1024];
    if (adb_native_shell("wm size", out, sizeof(out)) == 0) {
        char *val = strchr(out, ':');
        if (val) {
            val++; while (*val == ' ') val++;
            int w = atoi(val);
            if (w > 0 && w < 10000) { cached_w = w; return w; }
        }
    }
    if (adb_native_shell("getprop ro.sf.lcd_width", out, sizeof(out)) == 0) {
        trim_newline(out);
        if (strlen(out) > 0) { int w = atoi(out); if (w > 0 && w < 10000) { cached_w = w; return w; } }
    }
    return 1080;
}

int adb_get_screen_h(void) {
    static int cached_h = 0;
    if (cached_h > 0) return cached_h;
    char out[1024];
    if (adb_native_shell("wm size", out, sizeof(out)) == 0) {
        char *val = strchr(out, ':');
        if (val) {
            val++; while (*val == ' ') val++;
            char *x = strchr(val, 'x');
            if (x) { x++; int h = atoi(x); if (h > 0 && h < 10000) { cached_h = h; return h; } }
        }
    }
    if (adb_native_shell("getprop ro.sf.lcd_height", out, sizeof(out)) == 0) {
        trim_newline(out);
        if (strlen(out) > 0) { int h = atoi(out); if (h > 0 && h < 10000) { cached_h = h; return h; } }
    }
    return 2400;
}

/* ── digit coordinates ── */
typedef enum { GRID_AOSP, GRID_SAMSUNG, GRID_XIAOMI, GRID_OPPO, GRID_VIVO } GridPreset;

static void digit_coords_grid(char digit, int preset, int *x, int *y) {
    int w = adb_get_screen_w();
    int h = adb_get_screen_h();
    int col = 1, row = 0;
    switch (digit) {
        case '1': col=0; row=0; break; case '2': col=1; row=0; break; case '3': col=2; row=0; break;
        case '4': col=0; row=1; break; case '5': col=1; row=1; break; case '6': col=2; row=1; break;
        case '7': col=0; row=2; break; case '8': col=1; row=2; break; case '9': col=2; row=2; break;
        case '0': col=1; row=3; break; default:  col=1; row=3; break;
    }
    int key_w, key_h, start_x, start_y;
    switch (preset) {
        case GRID_SAMSUNG:
            key_w = w / 3; key_h = h / 7;
            start_x = 0; start_y = h * 40 / 100;
            break;
        case GRID_XIAOMI:
            key_w = w / 4; key_h = h / 8;
            start_x = w / 8; start_y = h * 42 / 100;
            break;
        case GRID_OPPO:
            key_w = w / 3; key_h = h / 9;
            start_x = w / 8; start_y = h * 44 / 100;
            break;
        case GRID_VIVO:
            key_w = w / 4; key_h = h / 7;
            start_x = w / 8; start_y = h * 45 / 100;
            break;
        default:
            key_w = w / 4; key_h = h / 8;
            start_x = (w - key_w * 3) / 2; start_y = h * 45 / 100;
            break;
    }
    *x = start_x + col * key_w + key_w / 2;
    *y = start_y + row * key_h + key_h / 2;
}

void adb_digit_coords(char digit, int *x, int *y) {
    digit_coords_grid(digit, GRID_AOSP, x, y);
}

/* ── SAKTI PIN entry ── */
int adb_enter_pin_sakti(const char *pin) {
    int len = strlen(pin);
    if (len < 1) return 0;

    for (int preset = 0; preset < 5; preset++) {
        for (int attempt = 0; attempt < 2; attempt++) {
            if (adb_is_unlocked()) return 1;

            adb_screen_on();
            usleep(150000);
            adb_swipe_smart();
            usleep(80000);

            /* Build shell script: tap + keyevent + text + confirm */
            char script[4096];
            int pos = 0;

            /* Method 1: tap coordinates */
            for (int i = 0; i < len; i++) {
                int x, y;
                digit_coords_grid(pin[i], preset, &x, &y);
                pos += snprintf(script + pos, sizeof(script) - pos,
                                "input tap %d %d; ", x, y);
            }
            /* Method 2: keyevent fallback */
            for (int i = 0; i < len; i++) {
                if (pin[i] >= '0' && pin[i] <= '9')
                    pos += snprintf(script + pos, sizeof(script) - pos,
                                    "input keyevent %d; ", 7 + (pin[i] - '0'));
            }
            /* Method 3: text fallback */
            pos += snprintf(script + pos, sizeof(script) - pos,
                            "input text '%s'; ", pin);
            /* confirm */
            pos += snprintf(script + pos, sizeof(script) - pos,
                            "input keyevent 66");
            adb_native_shell_noret(script);
            usleep(500000);

            if (adb_is_unlocked()) return 1;

            /* alternate confirm keys */
            adb_native_shell_noret("input keyevent 23");
            usleep(100000);
            adb_native_shell_noret("input keyevent 66");
            usleep(300000);

            if (adb_is_unlocked()) return 1;
        }
    }
    return 0;
}

int adb_enter_pin(const char *pin) {
    if (adb_enter_pin_sakti(pin)) return 1;
    int len = strlen(pin);
    if (len < 1) return 0;

    for (int pass = 0; pass < 2; pass++) {
        adb_screen_on();
        usleep(200000);
        adb_swipe_smart();
        usleep(200000);

        char script[4096];
        int pos = 0;
        for (int i = 0; i < len; i++) {
            char d = pin[i];
            if (d >= '0' && d <= '9') {
                int x, y;
                adb_digit_coords(d, &x, &y);
                pos += snprintf(script + pos, sizeof(script) - pos,
                                "input tap %d %d; input keyevent %d; ",
                                x, y, 7 + (d - '0'));
            }
        }
        pos += snprintf(script + pos, sizeof(script) - pos,
                        "input keyevent 66");
        adb_native_shell_noret(script);
        usleep(500000);

        if (adb_is_unlocked()) return 1;
    }
    return 0;
}

/* ── Ultra-reliable unlock detection (10 methods) ── */
int adb_is_unlocked(void) {
    char buf[16384];

    /* Method 1: mCurrentFocus / mFocusedApp */
    if (adb_native_shell("dumpsys window 2>/dev/null | grep -iE 'mCurrentFocus|mFocusedApp' | head -1",
                         buf, sizeof(buf)) == 0 && strlen(buf) > 0) {
        if (strstr(buf, "Keyguard") || strstr(buf, "LockScreen") || strstr(buf, "Bouncer"))
            return 0;
        if (strstr(buf, "Launcher") || strstr(buf, "Home") || strstr(buf, "SystemUI")
            || strstr(buf, "StatusBar"))
            return 1;
    }

    /* Method 2: mKeyguardShowing */
    if (adb_native_shell("dumpsys window 2>/dev/null | grep -i 'mKeyguardShowing' | head -1",
                         buf, sizeof(buf)) == 0 && strlen(buf) > 0) {
        if (strstr(buf, "mKeyguardShowing=false")) return 1;
        if (strstr(buf, "mKeyguardShowing=true")) return 0;
    }

    /* Method 3: lockscreen.password_type via settings */
    if (adb_native_shell("settings get secure lockscreen.password_type 2>/dev/null",
                         buf, sizeof(buf)) == 0 && strlen(buf) > 0) {
        trim_newline(buf);
        if (strcmp(buf, "0") == 0 || strlen(buf) == 0) return 1; /* No password */
        if (strcmp(buf, "65536") == 0 || strcmp(buf, "131072") == 0 ||
            strcmp(buf, "262144") == 0 || strcmp(buf, "327680") == 0 ||
            strcmp(buf, "393216") == 0 || strcmp(buf, "524288") == 0) return 0; /* Has lock */
    }

    /* Method 4: dumpsys lock_settings password type */
    if (adb_native_shell("dumpsys lock_settings 2>/dev/null | grep -i 'password.type\\|password_type\\|lockscreen.password_type' | head -1",
                         buf, sizeof(buf)) == 0 && strlen(buf) > 0) {
        if (strstr(buf, "=0") || strstr(buf, "= 0")) {
            return 1; /* No password type set */
        }
        return 0; /* Has password type */
    }

    /* Method 5: dumpsys activity top */
    if (adb_native_shell("dumpsys activity top 2>/dev/null | head -5",
                         buf, sizeof(buf)) == 0 && strlen(buf) > 0) {
        if (strstr(buf, "Keyguard") || strstr(buf, "LockScreen") || strstr(buf, "Bouncer"))
            return 0;
    }

    /* Method 6: Check for gatekeeper files (has lock = locked) */
    if (adb_native_shell("ls /data/system/gatekeeper.password.key 2>/dev/null && echo EXISTS || echo NOPE",
                         buf, sizeof(buf)) == 0 && strlen(buf) > 0) {
        if (strstr(buf, "EXISTS")) return 0;
    }

    /* Method 7: Check dumpsys power for dream state */
    if (adb_native_shell("dumpsys power 2>/dev/null | grep 'mWakefulness' | head -1",
                         buf, sizeof(buf)) == 0 && strlen(buf) > 0) {
        if (strstr(buf, "Asleep")) return 0; /* Screen off = locked */
    }

    /* Method 8: lockscreen.lockoutattempteddeadline > 0 = locked */
    if (adb_native_shell("settings get secure lockscreen.lockoutattemptdeadline 2>/dev/null",
                         buf, sizeof(buf)) == 0 && strlen(buf) > 0) {
        trim_newline(buf);
        long deadline = atol(buf);
        if (deadline > 0 && deadline < 9999999999L) return 0; /* Lockout active = locked */
    }

    /* Method 9: Check if we can take a screenshot (requires unlocked) */
    if (adb_native_shell("screencap -p /dev/null 2>&1 | head -1",
                         buf, sizeof(buf)) == 0 && strlen(buf) > 0) {
        if (strstr(buf, "secure") || strstr(buf, "denied") || strstr(buf, "Error"))
            return 0; /* Can't screenshot = locked */
    }

    /* Method 10: Check keyguard state via window policy */
    if (adb_native_shell("dumpsys window policy 2>/dev/null | grep -i 'keyguard' | head -3",
                         buf, sizeof(buf)) == 0 && strlen(buf) > 0) {
        if (strstr(buf, "mKeyguardOccluded=false") && strstr(buf, "mKeyguardShowing=true"))
            return 0;
        if (strstr(buf, "mKeyguardGoingAway"))
            return 1; /* Transitioning away from lock */
    }

    /* If shell is dead (bootloop), assume locked */
    if (!adb_shell_works()) return 0;

    /* Default: assume locked */
    return 0;
}

/* ── Screen state info for non-blind brute force ── */
/* Returns human-readable lock screen state string */
void adb_lock_screen_info(char *out, int out_sz) {
    char buf[4096];
    out[0] = 0;

    /* Get current focus window */
    if (adb_native_shell("dumpsys window 2>/dev/null | grep -iE 'mCurrentFocus|mFocusedApp' | head -2",
                         buf, sizeof(buf)) == 0 && strlen(buf) > 0) {
        trim_newline(buf);
        char *focus = strstr(buf, "mCurrentFocus");
        if (focus) {
            char *val = strchr(focus, ':');
            if (val) {
                val++; while (*val == ' ') val++;
                snprintf(out, out_sz, "Focus: %.120s", val);
            }
        }
    }

    /* Get keyguard state */
    if (adb_native_shell("dumpsys window 2>/dev/null | grep -i 'mKeyguardShowing' | head -1",
                         buf, sizeof(buf)) == 0 && strlen(buf) > 0) {
        trim_newline(buf);
        int cur = strlen(out);
        if (cur > 0) cur += snprintf(out+cur, out_sz-cur, " | ");
        snprintf(out+strlen(out), out_sz-strlen(out), "%.80s", buf);
    }

    /* Get isStatusBarKeyguard */
    if (adb_native_shell("dumpsys window 2>/dev/null | grep -i 'isStatusBarKeyguard' | head -1",
                         buf, sizeof(buf)) == 0 && strlen(buf) > 0) {
        trim_newline(buf);
        int cur = strlen(out);
        if (cur > 0) cur += snprintf(out+cur, out_sz-cur, " | ");
        snprintf(out+strlen(out), out_sz-strlen(out), "%.60s", buf);
    }
}

/* ── Boot animation / screen on detection ── */
int adb_screen_is_on(void) {
    char buf[256];
    if (adb_native_shell("dumpsys power 2>/dev/null | grep -i 'mWakefulness=' | head -1",
                         buf, sizeof(buf)) == 0) {
        if (strstr(buf, "Awake")) return 1;
    }
    /* Alternative: check display state */
    if (adb_native_shell("dumpsys display 2>/dev/null | grep -i 'mScreenState=' | head -1",
                         buf, sizeof(buf)) == 0) {
        if (strstr(buf, "ON")) return 1;
    }
    return 0;
}

/* ── Lock type detection (PIN/Pattern/Password/None) ── */
void adb_lock_type_detect(char *out, int out_sz) {
    char buf[4096];
    out[0] = 0;

    /* Try locksettings database */
    if (adb_native_shell("dumpsys lock_settings 2>/dev/null | grep -iE 'lockscreen.passwordtype|password.type' | head -1",
                         buf, sizeof(buf)) == 0 && strlen(buf) > 0) {
        trim_newline(buf);
        snprintf(out, out_sz, "%.200s", buf);
        return;
    }

    /* Try keyguard from settings db */
    if (adb_native_shell("settings get secure lockscreen.password_type 2>/dev/null",
                         buf, sizeof(buf)) == 0 && strlen(buf) > 0) {
        trim_newline(buf);
        long pt = atol(buf);
        const char *type = "Unknown";
        if (pt == 0)      type = "None";
        else if (pt == 262144) type = "PIN";
        else if (pt == 327680) type = "Password";
        else if (pt == 65536)  type = "Pattern";
        else if (pt == 131072) type = "Pattern (legacy)";
        snprintf(out, out_sz, "password_type=%ld (%s)", pt, type);
        return;
    }

    snprintf(out, out_sz, "unknown (shell may not work)");
}

void adb_reboot(void)          { adb_native_shell_noret("reboot"); }
void adb_reboot_recovery(void) { adb_native_shell_noret("reboot recovery"); }
void adb_reboot_bootloader(void){ adb_native_shell_noret("reboot bootloader"); }
