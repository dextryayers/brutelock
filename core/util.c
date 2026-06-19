#define _GNU_SOURCE
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdarg.h>
#include <unistd.h>

void banner(void) {
    printf(RED "\n"
    "  ██████  ██████  ██    ██ ████████ ███████  ██████  ██ ███    ██ \n"
    "  ██   ██ ██   ██ ██    ██    ██    ██      ██       ██ ████   ██ \n"
    "  ██████  ██████  ██    ██    ██    █████   ██   ███ ██ ██ ██  ██ \n"
    "  ██   ██ ██   ██ ██    ██    ██    ██      ██    ██ ██ ██  ██ ██ \n"
    "  ██████  ██   ██  ██████     ██    ███████  ██████  ██ ██   ████ \n" RESET
    "  =================================================================\n"
  "  " GREEN "BrutePin v1.1" RESET " - Interactive PIN BruteForce - By: Aniipid\n"
  "  Authorized Security Testing Only\n"
    "  =================================================================\n\n");
}

void save_state(long cur, long tot) {
    FILE *f = fopen(STATE_FILE, "w");
    if (f) { fprintf(f, "%ld %ld\n", cur, tot); fclose(f); }
}

long load_state(long *tot) {
    FILE *f = fopen(STATE_FILE, "r");
    if (!f) { *tot = 0; return -1; }
    long cur; fscanf(f, "%ld %ld", &cur, tot); fclose(f);
    return cur;
}

void print_progress(long cur, long total, const char *pin, double elapsed) {
    if (elapsed <= 0) elapsed = 0.1;
    double pct = (double)cur / total * 100.0;
    double rate = cur / elapsed;
    double eta = (rate > 0) ? (total - cur) / rate : 0;
    char eta_str[32];
    if (eta >= 3600) snprintf(eta_str, sizeof(eta_str), "%.1fh", eta/3600);
    else if (eta >= 60) snprintf(eta_str, sizeof(eta_str), "%.1fm", eta/60);
    else snprintf(eta_str, sizeof(eta_str), "%.0fs", eta);
    int bw = 20, fill = (int)round(bw * pct / 100.0);
    if (fill > bw) fill = bw;
    printf("\r[");
    for (int i = 0; i < bw; i++) putchar(i < fill ? '=' : ' ');
    printf("] %5.1f%% | %s | %4.0f/s | ETA %s | %ld/%ld  ", pct, pin, rate, eta_str, cur, total);
    fflush(stdout);
}

char* trim_newline(char *s) {
    size_t l = strlen(s);
    while (l > 0 && (s[l-1] == '\n' || s[l-1] == '\r')) s[--l] = 0;
    return s;
}

void normalize_str(char *dst, const char *src) {
    while (*src) {
        if (*src == '\n' || *src == '\r') { src++; continue; }
        if ((unsigned char)*src < 32) { src++; continue; }
        *dst++ = *src++;
    }
    *dst = 0;
}

void append_str(char *dst, size_t size, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char tmp[2048];
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    size_t cur = strlen(dst);
    if (cur + strlen(tmp) < size - 1) strcat(dst, tmp);
}

int yes_no(const char *prompt) {
    printf("%s", prompt);
    char buf[16] = {0};
    if (!fgets(buf, sizeof(buf), stdin)) return 0;
    return buf[0] == 'y' || buf[0] == 'Y';
}

void loading_spinner(const char *msg, int duration_s) {
    const char spin[] = {'|', '/', '-', '\\'};
    printf("%s ", msg);
    fflush(stdout);
    for (int i = 0; i < duration_s * 4; i++) {
        printf("\r%c %s ", spin[i % 4], msg);
        fflush(stdout);
        usleep(250000);
    }
    printf("\r");
}

void loading_dots(const char *msg, int duration_s) {
    printf("%s", msg);
    fflush(stdout);
    for (int i = 0; i < duration_s * 2; i++) {
        printf(".");
        fflush(stdout);
        sleep(1);
    }
    printf("\n");
}
