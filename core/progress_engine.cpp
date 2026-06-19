#include "progress_engine.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

extern "C" {

void ptrack_init(ProgressTracker *pt, long total) {
    memset(pt, 0, sizeof(*pt));
    pt->total = total;
    pt->start_time = (double)clock() / CLOCKS_PER_SEC;
    pt->current_time = pt->start_time;
}

void ptrack_tick(ProgressTracker *pt) {
    pt->attempted++;
    pt->current_time = (double)clock() / CLOCKS_PER_SEC;
    pt->elapsed = pt->current_time - pt->start_time;

    /* Rate: smoothed over last 60s */
    if (pt->elapsed > 0) {
        double rate = pt->attempted / pt->elapsed;
        pt->rate_history[pt->rate_idx % 60] = rate;
        pt->rate_idx++;
        double sum = 0;
        int n = pt->rate_idx < 60 ? pt->rate_idx : 60;
        for (int i = 0; i < n; i++) sum += pt->rate_history[i];
        pt->rate = sum / n;
        if (pt->rate > pt->peak_rate) pt->peak_rate = pt->rate;
    }

    /* ETA */
    if (pt->rate > 0) {
        long remaining = pt->total - pt->attempted;
        pt->eta = remaining / pt->rate;
        /* Subtract lockout time from ETA */
        if (pt->lockout_elapsed > 0 && pt->eta > pt->lockout_elapsed)
            pt->eta -= pt->lockout_elapsed;
    }
}

void ptrack_found(ProgressTracker *pt, long at) {
    pt->found_at = at;
    pt->attempted = at;
    pt->current_time = (double)clock() / CLOCKS_PER_SEC;
    pt->elapsed = pt->current_time - pt->start_time;
}

void ptrack_lockout(ProgressTracker *pt, int seconds) {
    pt->lockout_count++;
    pt->lockout_elapsed += seconds;
}

void ptrack_bar_str(ProgressTracker *pt, const char *pin,
                    const char *extra, char *out, int out_sz) {
    double pct = pt->total > 0 ?
        (pt->attempted * 100.0 / pt->total) : 0;
    if (pct > 100) pct = 100;

    int bw = 20;
    int fill = (int)(bw * pct / 100.0);
    if (fill > bw) fill = bw;

    char bar[32];
    int pos = 0;
    bar[pos++] = '[';
    for (int i = 0; i < bw; i++)
        bar[pos++] = i < fill ? '=' : ' ';
    bar[pos++] = ']'; bar[pos] = 0;

    char found_str[32] = "";
    if (pt->found_at > 0)
        snprintf(found_str, sizeof(found_str), " " GREEN "FOUND" RESET);

    snprintf(out, out_sz, "%s %5.1f%% | %s | %4.0f/s | ETA %4.0fs | %s%s",
             bar, pct,
             pin ? pin : "",
             pt->rate, pt->eta,
             extra ? extra : "",
             found_str);
}

void ptrack_print_status(ProgressTracker *pt, const char *pin,
                         const char *extra) {
    char buf[256];
    ptrack_bar_str(pt, pin, extra, buf, sizeof(buf));
    printf("\r  %s  ", buf);
    fflush(stdout);
}

void ptrack_print_verbose(ProgressTracker *pt, const char *pin,
                          const char *screen_state, int digits) {
    double pct = pt->total > 0 ?
        (pt->attempted * 100.0 / pt->total) : 0;
    if (pct > 100) pct = 100;

    char bar[32];
    int bw = 25;
    int fill = (int)(bw * pct / 100.0);
    if (fill > bw) fill = bw;
    int pos = 0;
    bar[pos++] = '[';
    for (int i = 0; i < bw; i++)
        bar[pos++] = i < fill ? '=' : ' ';
    bar[pos++] = ']'; bar[pos] = 0;

    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char ts[32];
    strftime(ts, sizeof(ts), "%H:%M:%S", tm);

    printf("\r" CYAN "[%s]" RESET " #%ld/%ld | %s %s | %s | %4.0f/s"
           " | %.0fs | %d lockouts   ",
           ts, pt->attempted, pt->total,
           bar, pin ? pin : "",
           screen_state ? screen_state : "",
           pt->rate, pt->elapsed,
           pt->lockout_count);
    fflush(stdout);
}

double ptrack_pct(ProgressTracker *pt) {
    if (pt->total <= 0) return 0;
    return pt->attempted * 100.0 / pt->total;
}

void ptrack_save(ProgressTracker *pt, const char *state_file) {
    FILE *f = fopen(state_file, "w");
    if (!f) return;
    fprintf(f, "%ld %ld %d %.0f\n",
            pt->attempted, pt->total, pt->lockout_count, pt->elapsed);
    fclose(f);
}

int ptrack_load(ProgressTracker *pt, const char *state_file) {
    FILE *f = fopen(state_file, "r");
    if (!f) return 0;
    long attempted, total;
    int lockout_count;
    double elapsed;
    int r = fscanf(f, "%ld %ld %d %lf", &attempted, &total, &lockout_count, &elapsed);
    fclose(f);
    if (r != 4) return 0;
    if (attempted < 0 || attempted >= total) return 0;
    pt->attempted = attempted;
    pt->total = total;
    pt->lockout_count = lockout_count;
    pt->elapsed = elapsed;
    pt->start_time = (double)clock() / CLOCKS_PER_SEC - elapsed;
    return 1;
}

void ptrack_print_summary(ProgressTracker *pt) {
    printf("\n" BOLD "=== Brute Summary ===" RESET "\n");
    printf("  Attempted    : %ld\n", pt->attempted);
    printf("  Total        : %ld\n", pt->total);
    printf("  Elapsed      : %.0fs (%.1fh)\n", pt->elapsed, pt->elapsed / 3600.0);
    printf("  Avg Rate     : %.1f/s\n", pt->rate);
    printf("  Peak Rate    : %.1f/s\n", pt->peak_rate);
    printf("  Lockouts     : %d (%lds)\n", pt->lockout_count, pt->lockout_elapsed);
    if (pt->found_at > 0)
        printf("  " GREEN "Found at     : attempt #%ld%s\n" RESET, pt->found_at, "!");
    else
        printf("  " YELLOW "Result       : Not found%s\n" RESET, ".");
    printf("\n");
}

} // extern "C"
