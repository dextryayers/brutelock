#define _GNU_SOURCE
#include "screen_watch.h"
#include "adb_native.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>

void sw_init(ScreenState *ss) {
    memset(ss, 0, sizeof(*ss));
    ss->locked = 1;
    ss->poll_interval_ms = 2000;
    pthread_mutex_init(&ss->lock, NULL);
}

static void *sw_thread_func(void *arg) {
    ScreenState *ss = (ScreenState*)arg;
    while (ss->running) {
        sw_snapshot(ss);
        usleep(ss->poll_interval_ms * 1000);
    }
    return NULL;
}

void sw_start(ScreenState *ss) {
    if (ss->running) return;
    ss->running = 1;
    pthread_create(&ss->thread, NULL, sw_thread_func, ss);
    pthread_detach(ss->thread);
}

void sw_stop(ScreenState *ss) {
    ss->running = 0;
}

void sw_snapshot(ScreenState *ss) {
    char buf[16384];
    int locked = 1, screen_on = 1;
    char lock_type[32] = "PIN";
    char focus[256] = "?";
    int lockout = 0;
    int attempts = -1;

    /* Method 1: mKeyguardShowing */
    if (adb_native_shell("dumpsys window 2>/dev/null | grep -i 'mKeyguardShowing' | head -1",
                         buf, sizeof(buf)) == 0 && strlen(buf) > 0) {
        if (strstr(buf, "mKeyguardShowing=false")) locked = 0;
    }

    /* Method 2: mCurrentFocus */
    if (adb_native_shell("dumpsys window 2>/dev/null | grep -iE 'mCurrentFocus|mFocusedApp' | head -1",
                         buf, sizeof(buf)) == 0 && strlen(buf) > 0) {
        char *p = strstr(buf, "mCurrentFocus");
        if (p) {
            p = strchr(p, ':');
            if (p) { p++; while (*p == ' ') p++; }
            snprintf(focus, sizeof(focus), "%.200s", p ? p : "?");
            trim_newline(focus);
        }
        if (strstr(buf, "Keyguard") || strstr(buf, "LockScreen") || strstr(buf, "Bouncer"))
            locked = 1;
        if (strstr(buf, "Launcher") || strstr(buf, "Home") || strstr(buf, "SystemUI"))
            locked = 0;
    }

    /* Method 3: Screen on/off via mWakefulness */
    if (adb_native_shell("dumpsys power 2>/dev/null | grep 'mWakefulness' | head -1",
                         buf, sizeof(buf)) == 0) {
        screen_on = strstr(buf, "Awake") ? 1 : 0;
    }

    /* Method 4: Lock type from settings */
    if (adb_native_shell("settings get secure lockscreen.password_type 2>/dev/null",
                         buf, sizeof(buf)) == 0) {
        trim_newline(buf);
        if (strstr(buf, "65536")) snprintf(lock_type, sizeof(lock_type), "PIN");
        else if (strstr(buf, "131072")) snprintf(lock_type, sizeof(lock_type), "Password");
        else if (strstr(buf, "262144")) snprintf(lock_type, sizeof(lock_type), "Pattern");
        else if (strstr(buf, "327680")) snprintf(lock_type, sizeof(lock_type), "PIN(6)");
        else if (strlen(buf) > 0 && strcmp(buf, "0") != 0 && strcmp(buf, "null") != 0)
            snprintf(lock_type, sizeof(lock_type), "%s", buf);
    }

    /* Method 5: Lockout deadline (epoch timestamp) */
    if (adb_native_shell("settings get secure lockscreen.lockoutattemptdeadline 2>/dev/null",
                         buf, sizeof(buf)) == 0) {
        trim_newline(buf);
        long deadline = atol(buf);
        if (deadline > 0 && deadline < 9999999999L) {
            long now = (long)time(NULL);
            lockout = (int)(deadline - now);
            if (lockout < 0) lockout = 0;
        }
    }

    /* Method 6: Failed attempts from dumpsys lock_settings */
    if (adb_native_shell("dumpsys lock_settings 2>/dev/null | grep -i 'failed' | head -1",
                         buf, sizeof(buf)) == 0 && strlen(buf) > 0) {
        char *p = strstr(buf, "failed");
        if (p) {
            p = strchr(p, '=');
            if (p) { p++; while (*p == ' ') p++; }
            if (isdigit(*p)) attempts = atoi(p);
        }
    }

    /* Method 7: dumpsys activity top check */
    if (adb_native_shell("dumpsys activity top 2>/dev/null | head -5 | grep -iE 'Keyguard|LockScreen'",
                         buf, sizeof(buf)) == 0 && strlen(buf) > 0) {
        locked = 1;
    }

    /* Method 8: Check gatekeeper files */
    if (adb_native_shell("ls /data/system/gatekeeper.password.key 2>/dev/null && echo GKEXISTS",
                         buf, sizeof(buf)) == 0 && strlen(buf) > 0) {
        if (strstr(buf, "GKEXISTS") && lock_type[0] == 'P')
            snprintf(lock_type, sizeof(lock_type), "PIN");
    }

    /* Method 9: dumpsys window policy */
    if (locked == 0) {
        if (adb_native_shell("dumpsys window policy 2>/dev/null | grep -i 'mKeyguardShowing' | head -1",
                             buf, sizeof(buf)) == 0 && strlen(buf) > 0) {
            if (strstr(buf, "mKeyguardShowing=true")) locked = 1;
        }
    }

    /* Method 10: Check if screenshot works (only when unlocked) */
    if (locked == 0 && screen_on) {
        if (adb_native_shell("screencap -p /dev/null 2>&1 | grep -i 'secure\\|denied\\|Error' | head -1",
                             buf, sizeof(buf)) == 0 && strlen(buf) > 0) {
            locked = 1;
        }
    }

    pthread_mutex_lock(&ss->lock);
    ss->locked = locked;
    ss->screen_on = screen_on;
    strncpy(ss->lock_type, lock_type, sizeof(ss->lock_type)-1);
    strncpy(ss->current_focus, focus, sizeof(ss->current_focus)-1);
    ss->lockout_remaining = lockout;
    ss->failed_attempts = attempts;

    char attempts_str[32] = "";
    if (attempts >= 0) snprintf(attempts_str, sizeof(attempts_str), " | %d fails", attempts);

    char lockout_str[64] = "";
    if (lockout > 0) snprintf(lockout_str, sizeof(lockout_str), " | " RED "LOCKOUT %ds" RESET, lockout);

    snprintf(ss->info_str, sizeof(ss->info_str),
             "%s | %s | %s%s%s%s",
             screen_on ? GREEN "ON" RESET : RED "OFF" RESET,
             lock_type,
             locked ? RED "LOCKED" RESET : GREEN "UNLOCKED" RESET,
             focus,
             attempts_str,
             lockout_str);
    pthread_mutex_unlock(&ss->lock);
}

void sw_get_state(ScreenState *ss, ScreenState *out) {
    pthread_mutex_lock(&ss->lock);
    memcpy(out, ss, sizeof(*out));
    pthread_mutex_unlock(&ss->lock);
}

void sw_status_str(ScreenState *ss, char *out, int out_sz) {
    pthread_mutex_lock(&ss->lock);
    snprintf(out, out_sz, "%s", ss->info_str);
    pthread_mutex_unlock(&ss->lock);
}

int sw_check_unlocked(ScreenState *ss) {
    int locked;
    pthread_mutex_lock(&ss->lock);
    locked = ss->locked;
    pthread_mutex_unlock(&ss->lock);
    return locked ? 0 : 1;
}
