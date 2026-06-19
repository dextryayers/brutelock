#ifndef ADAPTIVE_TIMER_H
#define ADAPTIVE_TIMER_H

#ifdef __cplusplus
extern "C" {
#endif

/*  Timing profile for one device */
typedef struct {
    double  response_latency_ms;   /* Measured phone response time */
    double  current_delay_ms;      /* Current delay between attempts */
    int     lockout_threshold;     /* Attempts before lockout detected */
    int     consecutive_attempts;  /* Current streak without lockout */
    int     max_consecutive;       /* Max allowed before cooling */
    double  cooling_period_ms;     /* Extra delay during cooling */
    int     adapt_count;           /* How many times we adapted */
    double  min_delay;             /* Minimum allowed delay */
    double  max_delay;             /* Maximum allowed delay */
    double  target_rate;           /* Target attempts/sec */
} AdaptiveTimer;

/*  Initialize timer with defaults */
void atimer_init(AdaptiveTimer *at, double initial_delay_ms);

/*  Record an attempt result. Call after each PIN entry.
    delay_ms: how long we waited before this attempt
    locked_out: 1 if device was in lockout
    response_time_ms: how long phone took to process (approx) */
void atimer_record(AdaptiveTimer *at, double delay_ms,
                   int locked_out, double response_time_ms);

/*  Get recommended delay for next attempt */
double atimer_get_delay(AdaptiveTimer *at);

/*  Check if cooling period is needed */
int atimer_needs_cooldown(AdaptiveTimer *at);

/*  Get current stats as string */
void atimer_stats_str(AdaptiveTimer *at, char *out, int out_sz);

/*  Reset on new device */
void atimer_reset(AdaptiveTimer *at, double initial_delay_ms);

#ifdef __cplusplus
}
#endif
#endif
