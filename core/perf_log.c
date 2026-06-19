#define _GNU_SOURCE
#include "perf_log.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>

void perf_init(PerfLog *pl) {
    memset(pl, 0, sizeof(*pl));
    pl->stats.start_time = (double)clock() / CLOCKS_PER_SEC;
    pl->stats.min_attempt_time_ms = 999999.0;
    pl->stats.max_attempt_time_ms = 0.0;
}

void perf_log_attempt(PerfLog *pl, const char *pin, int result,
                      int screen_on, int locked, int lockout_sec,
                      const char *lock_type) {
    int idx = (pl->head + pl->count) % PERF_LOG_SIZE;
    if (pl->count == PERF_LOG_SIZE) {
        pl->head = (pl->head + 1) % PERF_LOG_SIZE;
        idx = (pl->head + pl->count - 1) % PERF_LOG_SIZE;
    } else {
        pl->count++;
    }

    AttemptLog *e = &pl->entries[idx];
    pl->attempt_counter++;
    e->attempt_num = pl->attempt_counter;
    strncpy(e->pin, pin, sizeof(e->pin)-1);
    e->result = result;
    e->total_elapsed = ((double)clock() / CLOCKS_PER_SEC) - pl->stats.start_time;
    e->screen_was_on = screen_on;
    e->was_locked = locked;
    e->lockout_sec = lockout_sec;
    strncpy(e->lock_type, lock_type ? lock_type : "?", sizeof(e->lock_type)-1);

    /* Elapsed since last attempt */
    double now = (double)clock() / CLOCKS_PER_SEC;
    if (pl->stats.last_attempt_time > 0) {
        e->elapsed_ms = (now - pl->stats.last_attempt_time) * 1000.0;
    } else {
        e->elapsed_ms = 0;
    }
    pl->stats.last_attempt_time = now;

    /* Timestamp */
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(e->timestamp, sizeof(e->timestamp), "%H:%M:%S", tm);

    /* Update stats */
    pl->stats.total_attempts = pl->attempt_counter;
    if (result) pl->stats.successful_attempts++;
    if (lockout_sec > 0) {
        pl->stats.lockout_count++;
        pl->stats.total_lockout_sec += lockout_sec;
        pl->stats.consecutive_failures = 0;
    } else {
        if (!result) pl->stats.consecutive_failures++;
        else pl->stats.consecutive_failures = 0;
    }

    double elapsed = e->total_elapsed;
    if (elapsed > 0) {
        double rate = pl->stats.total_attempts / elapsed;
        pl->stats.current_rate = rate;
        if (rate > pl->stats.peak_rate) pl->stats.peak_rate = rate;
        pl->stats.avg_rate = rate;
    }

    if (e->elapsed_ms > 0) {
        if (e->elapsed_ms < pl->stats.min_attempt_time_ms)
            pl->stats.min_attempt_time_ms = e->elapsed_ms;
        if (e->elapsed_ms > pl->stats.max_attempt_time_ms)
            pl->stats.max_attempt_time_ms = e->elapsed_ms;
        if (pl->stats.total_attempts > 1) {
            pl->stats.avg_attempt_time_ms =
                (pl->stats.avg_attempt_time_ms * (pl->stats.total_attempts - 2) + e->elapsed_ms)
                / (pl->stats.total_attempts - 1);
        } else {
            pl->stats.avg_attempt_time_ms = e->elapsed_ms;
        }
    }
}

void perf_get_stats(PerfLog *pl, PerfStats *out) {
    memcpy(out, &pl->stats, sizeof(*out));
}

int perf_recent(PerfLog *pl, AttemptLog *out, int max) {
    int n = pl->count < max ? pl->count : max;
    for (int i = 0; i < n; i++) {
        int idx = (pl->head + i) % PERF_LOG_SIZE;
        memcpy(&out[i], &pl->entries[idx], sizeof(AttemptLog));
    }
    return n;
}

void perf_print_summary(PerfLog *pl) {
    double elapsed = ((double)clock() / CLOCKS_PER_SEC) - pl->stats.start_time;
    printf("\n" BOLD "=== Performance Summary ===" RESET "\n");
    printf("  Total attempts : %ld\n", pl->stats.total_attempts);
    printf("  Successes      : %ld\n", pl->stats.successful_attempts);
    printf("  Elapsed        : %.0fs\n", elapsed);
    printf("  Avg rate       : %.1f/s\n", pl->stats.avg_rate);
    printf("  Peak rate      : %.1f/s\n", pl->stats.peak_rate);
    printf("  Avg attempt    : %.1fms\n", pl->stats.avg_attempt_time_ms);
    printf("  Min attempt    : %.1fms\n", pl->stats.min_attempt_time_ms);
    printf("  Max attempt    : %.1fms\n", pl->stats.max_attempt_time_ms);
    printf("  Lockouts       : %d (%lds total)\n",
           pl->stats.lockout_count, pl->stats.total_lockout_sec);
    printf("\n");
}

void perf_progress_bar(char *out, int out_sz, double pct, int width) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    int fill = (int)(width * pct / 100.0);
    int pos = 0;
    pos += snprintf(out + pos, out_sz - pos, "[");
    for (int i = 0; i < width; i++)
        pos += snprintf(out + pos, out_sz - pos, "%c", i < fill ? '=' : ' ');
    pos += snprintf(out + pos, out_sz - pos, "]");
}

void perf_print_status(PerfLog *pl, long current, long total,
                       const char *pin, const char *screen_status) {
    double elapsed = ((double)clock() / CLOCKS_PER_SEC) - pl->stats.start_time;
    double pct = total > 0 ? (current * 100.0 / total) : 0;
    double rate = elapsed > 0 ? current / elapsed : 0;
    double eta = rate > 0 ? (total - current) / rate : 0;

    char bar[32];
    perf_progress_bar(bar, sizeof(bar), pct, 20);

    printf("\r  %s %5.1f%% | %s | %4.0f/s | ETA %4.0fs | %ld/%ld | %s  ",
           bar, pct, pin, rate, eta, current, total,
           screen_status ? screen_status : "");
    fflush(stdout);
}

void perf_print_attempt(AttemptLog *entry) {
    if (!entry) return;
    printf("\r  [%s] #%05ld | %s | %s | %5.0fms | %s%s\n",
           entry->timestamp,
           entry->attempt_num,
           entry->pin,
           entry->result ? GREEN "OK" RESET : RED "FAIL" RESET,
           entry->elapsed_ms,
           entry->screen_was_on ? "ON" : "OFF",
           entry->lockout_sec > 0 ? " | LOCKOUT" : "");
}
