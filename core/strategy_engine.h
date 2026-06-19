#ifndef STRATEGY_ENGINE_H
#define STRATEGY_ENGINE_H

#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

/*  Brute force phases */
typedef enum {
    PHASE_AI_MARKOV = 0,
    PHASE_COMMON_PINS,
    PHASE_DEVICE_SPECIFIC,
    PHASE_SEQUENTIAL,
    PHASE_DONE
} BrutePhase;

/*  Phase result tracking */
typedef struct {
    BrutePhase phase;
    long attempts;
    long successes;
    double success_rate;
    double avg_time_ms;
    int enabled;
} PhaseStats;

/*  Strategy configuration */
typedef struct {
    int digits;
    int delay_ms;
    long start;
    long end;
    int use_ai;           /* 0/1 */
    int use_common;       /* 0/1 */
    int use_device;       /* 0/1 */
    int use_sequential;   /* 0/1 */
    int max_ai_pins;      /* Max AI predictions to try (256) */
    int max_common_pins;  /* Max common pins (50) */
    int adapt_phase;      /* Auto-adapt phase order */
    double phase_switch_threshold; /* Switch phase if success rate < this */
} StrategyConfig;

/*  Initialize strategy engine */
void strat_init(StrategyConfig *cfg, int digits);

/*  Get current phase name */
const char* strat_phase_name(BrutePhase p);

/*  Get next PIN from current phase. Returns 1 if PIN available, 0 if phase exhausted. */
int strat_next_pin(BrutePhase phase, long phase_idx, char *pin, int pin_sz,
                   const DeviceInfo *info);

/*  Get total PINs available in a phase */
long strat_phase_total(BrutePhase phase, int digits, const DeviceInfo *info);

/*  Should we switch to next phase? */
int strat_should_switch(PhaseStats *stats, StrategyConfig *cfg);

/*  Print phase plan */
void strat_print_plan(StrategyConfig *cfg, const DeviceInfo *info);

#ifdef __cplusplus
}
#endif
#endif
