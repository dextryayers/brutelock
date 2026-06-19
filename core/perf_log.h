#ifndef PERF_LOG_H
#define PERF_LOG_H

#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

/*  Attempt log entry */
typedef struct {
    long    attempt_num;
    char    pin[16];
    int     result;         /* 0=fail, 1=success */
    double  elapsed_ms;     /* Time taken for this attempt */
    double  total_elapsed;  /* Total elapsed time */
    int     screen_was_on;
    int     was_locked;
    int     lockout_sec;
    char    lock_type[32];
    char    timestamp[64];
} AttemptLog;

/*  Performance statistics */
typedef struct {
    long    total_attempts;
    long    successful_attempts;
    double  start_time;
    double  last_attempt_time;
    double  avg_attempt_time_ms;
    double  min_attempt_time_ms;
    double  max_attempt_time_ms;
    double  current_rate;     /* attempts/sec */
    double  peak_rate;        /* peak attempts/sec */
    double  avg_rate;
    int     lockout_count;
    long    total_lockout_sec;
    int     consecutive_failures;
} PerfStats;

/*  Log buffer (circular, holds last N entries) */
#define PERF_LOG_SIZE 256

typedef struct {
    AttemptLog entries[PERF_LOG_SIZE];
    int head;
    int count;
    PerfStats stats;
    long attempt_counter;
} PerfLog;

/*  Initialize performance log */
void perf_init(PerfLog *pl);

/*  Log an attempt result */
void perf_log_attempt(PerfLog *pl, const char *pin, int result,
                      int screen_on, int locked, int lockout_sec,
                      const char *lock_type);

/*  Get current stats (thread-safe copy) */
void perf_get_stats(PerfLog *pl, PerfStats *out);

/*  Get recent attempts (up to N entries) */
int perf_recent(PerfLog *pl, AttemptLog *out, int max);

/*  Print current performance summary */
void perf_print_summary(PerfLog *pl);

/*  Build progress bar string */
void perf_progress_bar(char *out, int out_sz, double pct, int width);

/*  Print real-time status line (non-blind verbose) */
void perf_print_status(PerfLog *pl, long current, long total,
                       const char *pin, const char *screen_status);

/*  Print verbose attempt log entry */
void perf_print_attempt(AttemptLog *entry);

#ifdef __cplusplus
}
#endif
#endif
