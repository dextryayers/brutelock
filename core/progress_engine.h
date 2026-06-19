#ifndef PROGRESS_ENGINE_H
#define PROGRESS_ENGINE_H

#include "util.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*  Progress tracking */
typedef struct {
    long    total;           /* Total PINs to try */
    long    attempted;       /* PINs attempted so far */
    long    found_at;        /* Attempt number where PIN found (0 = not found) */
    double  start_time;      /* Clock time when started */
    double  current_time;    /* Updated on each tick */
    double  elapsed;         /* Elapsed seconds */
    double  eta;             /* Estimated remaining seconds */
    double  rate;            /* Attempts per second */
    double  peak_rate;       /* Peak rate */
    int     lockout_count;   /* Number of lockouts */
    long    lockout_elapsed; /* Seconds spent in lockout */

    /* History for rate calculation */
    double  rate_history[60]; /* Last 60s of rate data */
    int     rate_idx;
} ProgressTracker;

/*  Initialize tracker */
void ptrack_init(ProgressTracker *pt, long total);

/*  Tick — call after each attempt */
void ptrack_tick(ProgressTracker *pt);

/*  Set as found */
void ptrack_found(ProgressTracker *pt, long at);

/*  Record lockout */
void ptrack_lockout(ProgressTracker *pt, int seconds);

/*  Build progress bar string (with ETA, rate, etc) */
void ptrack_bar_str(ProgressTracker *pt, const char *pin,
                    const char *extra, char *out, int out_sz);

/*  Print one-line status */
void ptrack_print_status(ProgressTracker *pt, const char *pin,
                         const char *extra);

/*  Print verbose multi-line status */
void ptrack_print_verbose(ProgressTracker *pt, const char *pin,
                          const char *screen_state, int digits);

/*  Get estimated completion percentage */
double ptrack_pct(ProgressTracker *pt);

/*  Save state to file */
void ptrack_save(ProgressTracker *pt, const char *state_file);

/*  Load state from file, return 1 if valid */
int ptrack_load(ProgressTracker *pt, const char *state_file);

/*  Print final summary */
void ptrack_print_summary(ProgressTracker *pt);

#ifdef __cplusplus
}
#endif
#endif
