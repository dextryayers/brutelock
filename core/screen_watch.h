#ifndef SCREEN_WATCH_H
#define SCREEN_WATCH_H

#include "util.h"
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/*  Screen/lock state, refreshed by background monitor thread */
typedef struct {
    int locked;              /* 1 = locked */
    int screen_on;          /* 1 = screen on */
    char lock_type[32];     /* "PIN", "Password", "Pattern", "None" */
    char current_focus[256];/* Current focused window */
    int  lockout_remaining; /* seconds remaining, 0 = no lockout */
    int  failed_attempts;   /* reported failed attempts */
    char info_str[512];     /* Human-readable status line */

    pthread_mutex_t lock;
    int running;
    pthread_t thread;
    int poll_interval_ms;   /* How often to poll (ms) */
} ScreenState;

/*  Initialize screen watcher */
void sw_init(ScreenState *ss);

/*  Start background monitoring thread */
void sw_start(ScreenState *ss);

/*  Stop background monitoring thread */
void sw_stop(ScreenState *ss);

/*  Take a single snapshot of screen state (non-threaded use) */
void sw_snapshot(ScreenState *ss);

/*  Get current state (thread-safe copy) */
void sw_get_state(ScreenState *ss, ScreenState *out);

/*  Get formatted status line */
void sw_status_str(ScreenState *ss, char *out, int out_sz);

/*  Check if device is still locked, return 1 if unlocked */
int sw_check_unlocked(ScreenState *ss);

#ifdef __cplusplus
}
#endif
#endif
