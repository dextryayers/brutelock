#include "strategy_engine.h"
#include "pin_ai.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern "C" {

void strat_init(StrategyConfig *cfg, int digits) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->digits = digits;
    cfg->delay_ms = 3;
    cfg->use_ai = 1;
    cfg->use_common = 1;
    cfg->use_device = 1;
    cfg->use_sequential = 1;
    cfg->max_ai_pins = 256;
    cfg->max_common_pins = 50;
    cfg->adapt_phase = 1;
    cfg->phase_switch_threshold = 0.001; /* 0.1% success rate → switch */
}

const char* strat_phase_name(BrutePhase p) {
    switch (p) {
        case PHASE_AI_MARKOV:    return "AI Markov-chain";
        case PHASE_COMMON_PINS:  return "Common PINs";
        case PHASE_DEVICE_SPECIFIC: return "Device-specific";
        case PHASE_SEQUENTIAL:   return "Sequential";
        default: return "Done";
    }
}

static const char* common_4[] = {
    "0000","1234","1111","2222","3333","4444","5555","6666","7777","8888",
    "9999","1212","4321","2000","2001","6969","1000","1122","1313","0001",
    "0007","1004","1337","2580","1590","1235","2468","1357","5683","0852",
    "111111","000000","123456","654321","121212","123123","112233","789456",
    "159753","258000","00000000","12345678","87654321","11223344","12121212",
    NULL
};

int strat_next_pin(BrutePhase phase, long phase_idx, char *pin, int pin_sz,
                   const DeviceInfo *info) {
    (void)info;
    pin[0] = 0;

    switch (phase) {
        case PHASE_AI_MARKOV: {
            return pin_ai_next(pin, pin_sz) ? 1 : 0;
        }
        case PHASE_COMMON_PINS: {
            int idx = 0;
            for (int i = 0; common_4[i] != NULL; i++) {
                if ((int)strlen(common_4[i]) == info->pin_len) {
                    if (idx == phase_idx) {
                        strncpy(pin, common_4[i], pin_sz - 1);
                        return 1;
                    }
                    idx++;
                }
            }
            return 0;
        }
        case PHASE_DEVICE_SPECIFIC: {
            static char dev_pins[32][16];
            static int ndev = 0;
            if (phase_idx == 0) {
                ndev = 0;
                if (strlen(info->serial) >= 4) {
                    snprintf(dev_pins[ndev++], 16, "%.*s", (int)info->pin_len, info->serial);
                }
                if (strlen(info->imei1) >= 4) {
                    snprintf(dev_pins[ndev++], 16, "%.*s", (int)info->pin_len, info->imei1 + strlen(info->imei1) - info->pin_len);
                }
                if (strlen(info->imei2) >= 4) {
                    snprintf(dev_pins[ndev++], 16, "%.*s", (int)info->pin_len, info->imei2 + strlen(info->imei2) - info->pin_len);
                }
                if (strlen(info->phone_number) >= 4) {
                    snprintf(dev_pins[ndev++], 16, "%.*s", (int)info->pin_len, info->phone_number + strlen(info->phone_number) - info->pin_len);
                }
            }
            if (phase_idx < ndev) {
                strncpy(pin, dev_pins[phase_idx], pin_sz - 1);
                return 1;
            }
            return 0;
        }
        case PHASE_SEQUENTIAL: {
            long total = 1;
            for (int i = 0; i < info->pin_len; i++) total *= 10;
            if (phase_idx >= total) return 0;
            snprintf(pin, pin_sz, "%0*ld", info->pin_len, phase_idx);
            return 1;
        }
        default:
            return 0;
    }
}

long strat_phase_total(BrutePhase phase, int digits, const DeviceInfo *info) {
    (void)info;
    switch (phase) {
        case PHASE_AI_MARKOV: {
            long c = pin_ai_count();
            return c > 256 ? 256 : c;
        }
        case PHASE_COMMON_PINS: {
            long c = 0;
            for (int i = 0; common_4[i] != NULL; i++) {
                if ((int)strlen(common_4[i]) == digits) c++;
            }
            return c;
        }
        case PHASE_DEVICE_SPECIFIC: {
            long c = 0;
            if (strlen(info->serial) >= 4) c++;
            if (strlen(info->imei1) >= 4) c++;
            if (strlen(info->imei2) >= 4) c++;
            if (strlen(info->phone_number) >= 4) c++;
            return c;
        }
        case PHASE_SEQUENTIAL: {
            long total = 1;
            for (int i = 0; i < digits; i++) total *= 10;
            return total;
        }
        default: return 0;
    }
}

int strat_should_switch(PhaseStats *stats, StrategyConfig *cfg) {
    if (!cfg->adapt_phase) return 0;
    if (stats->attempts < 10) return 0; /* Don't switch too early */
    double rate = stats->attempts > 0 ?
        (double)stats->successes / stats->attempts : 0;
    return rate < cfg->phase_switch_threshold;
}

void strat_print_plan(StrategyConfig *cfg, const DeviceInfo *info) {
    printf("\n" BOLD "=== Brute Strategy Plan ===" RESET "\n");
    printf("  %d-digit PIN | Delay: %dms\n", cfg->digits, cfg->delay_ms);
    printf("\n  Phases:\n");
    if (cfg->use_ai)
        printf("    " CYAN "1" RESET ". %-20s %ld PINs\n",
               "AI Markov-chain", strat_phase_total(PHASE_AI_MARKOV, cfg->digits, info));
    if (cfg->use_common)
        printf("    " CYAN "2" RESET ". %-20s %ld PINs\n",
               "Common PINs", strat_phase_total(PHASE_COMMON_PINS, cfg->digits, info));
    if (cfg->use_device)
        printf("    " CYAN "3" RESET ". %-20s %ld PINs\n",
               "Device-specific", strat_phase_total(PHASE_DEVICE_SPECIFIC, cfg->digits, info));
    if (cfg->use_sequential)
        printf("    " CYAN "4" RESET ". %-20s %ld PINs\n",
               "Sequential", strat_phase_total(PHASE_SEQUENTIAL, cfg->digits, info));
    printf("\n");
}

} // extern "C"
