#include "adaptive_timer.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

extern "C" {

void atimer_init(AdaptiveTimer *at, double initial_delay_ms) {
    memset(at, 0, sizeof(*at));
    at->current_delay_ms = initial_delay_ms;
    at->min_delay = 1.0;      /* 1ms minimum */
    at->max_delay = 5000.0;   /* 5s maximum (lockout avoidance) */
    at->target_rate = 100.0;  /* Target 100/s initially */
    at->max_consecutive = 100;
    at->cooling_period_ms = 500.0;
    at->lockout_threshold = 5; /* Assume 5 failed = lockout */
}

void atimer_record(AdaptiveTimer *at, double delay_ms,
                   int locked_out, double response_time_ms) {
    at->response_latency_ms = response_time_ms;
    at->adapt_count++;

    if (locked_out) {
        /* Lockout detected — increase delay significantly */
        at->current_delay_ms *= 2.0;
        if (at->current_delay_ms > at->max_delay)
            at->current_delay_ms = at->max_delay;
        at->consecutive_attempts = 0;
        at->cooling_period_ms = at->current_delay_ms;
    } else {
        at->consecutive_attempts++;

        /* Learn optimal delay from response time */
        double suggested = response_time_ms * 1.5; /* 50% overhead */
        if (suggested > 0 && suggested < at->max_delay) {
            /* Smooth adaptation */
            at->current_delay_ms = at->current_delay_ms * 0.7 + suggested * 0.3;
        }

        /* Speed up if too conservative */
        if (at->consecutive_attempts > 20 && at->current_delay_ms > 2.0) {
            at->current_delay_ms *= 0.95; /* Gradually reduce */
        }

        /* Reset cooling after successful streak */
        if (at->consecutive_attempts > 10) {
            at->cooling_period_ms = 200.0;
        }

        /* Clamp */
        if (at->current_delay_ms < at->min_delay)
            at->current_delay_ms = at->min_delay;
        if (at->current_delay_ms > at->max_delay)
            at->current_delay_ms = at->max_delay;
    }
}

double atimer_get_delay(AdaptiveTimer *at) {
    /* Add cooling if needed */
    double delay = at->current_delay_ms;
    if (atimer_needs_cooldown(at)) {
        delay += at->cooling_period_ms;
    }
    return delay;
}

int atimer_needs_cooldown(AdaptiveTimer *at) {
    return at->consecutive_attempts >= at->max_consecutive;
}

void atimer_stats_str(AdaptiveTimer *at, char *out, int out_sz) {
    double rate = at->current_delay_ms > 0 ?
        1000.0 / at->current_delay_ms : 0;
    snprintf(out, out_sz,
             "delay=%.1fms rate=%.0f/s adapt=%d consec=%d cool=%.0fms",
             at->current_delay_ms, rate, at->adapt_count,
             at->consecutive_attempts, at->cooling_period_ms);
}

void atimer_reset(AdaptiveTimer *at, double initial_delay_ms) {
    atimer_init(at, initial_delay_ms);
}

} // extern "C"
