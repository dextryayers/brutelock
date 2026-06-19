#include "pin_ai.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <algorithm>
#include <vector>
#include <string>
#include <map>

/* ── Markov Chain PIN Model ───────────────────────────────────── */
/* Position-specific digit frequencies (counts) */
static double g_probs[10][10] = {
    {0.030, 0.068, 0.078, 0.074, 0.107, 0.166, 0.161, 0.095, 0.100, 0.121},
    {0.100, 0.115, 0.130, 0.125, 0.100, 0.065, 0.060, 0.080, 0.105, 0.120},
    {0.080, 0.100, 0.140, 0.150, 0.120, 0.085, 0.075, 0.070, 0.085, 0.095},
    {0.205, 0.035, 0.035, 0.040, 0.050, 0.120, 0.045, 0.040, 0.060, 0.370},
    {0.100, 0.080, 0.080, 0.080, 0.100, 0.120, 0.120, 0.080, 0.100, 0.140},
    {0.180, 0.030, 0.030, 0.030, 0.060, 0.120, 0.040, 0.040, 0.070, 0.400},
    {0.090, 0.070, 0.080, 0.090, 0.110, 0.120, 0.120, 0.080, 0.090, 0.150},
    {0.150, 0.040, 0.040, 0.050, 0.070, 0.110, 0.050, 0.040, 0.080, 0.370},
    {0.080, 0.080, 0.090, 0.100, 0.110, 0.120, 0.110, 0.080, 0.090, 0.140},
    {0.200, 0.030, 0.030, 0.030, 0.050, 0.110, 0.040, 0.030, 0.060, 0.420}
};

/* Bigram transition probabilities */
static double g_bigram[10][10] = {
    {0.08, 0.12, 0.14, 0.13, 0.10, 0.07, 0.06, 0.08, 0.10, 0.12},
    {0.09, 0.11, 0.13, 0.14, 0.11, 0.08, 0.06, 0.07, 0.09, 0.12},
    {0.08, 0.10, 0.12, 0.14, 0.12, 0.09, 0.07, 0.08, 0.09, 0.11},
    {0.07, 0.09, 0.11, 0.12, 0.13, 0.10, 0.08, 0.09, 0.10, 0.11},
    {0.08, 0.10, 0.12, 0.13, 0.11, 0.10, 0.07, 0.09, 0.10, 0.10},
    {0.06, 0.08, 0.10, 0.11, 0.12, 0.11, 0.09, 0.10, 0.11, 0.12},
    {0.07, 0.09, 0.10, 0.12, 0.13, 0.11, 0.08, 0.09, 0.10, 0.11},
    {0.08, 0.11, 0.12, 0.13, 0.11, 0.09, 0.07, 0.08, 0.10, 0.11},
    {0.07, 0.10, 0.11, 0.13, 0.12, 0.10, 0.08, 0.09, 0.09, 0.11},
    {0.10, 0.12, 0.13, 0.12, 0.10, 0.08, 0.06, 0.07, 0.09, 0.13}
};

struct PinEntry {
    std::string pin;
    int score;
};

static std::vector<PinEntry> g_predictions;
static int g_pred_idx = 0;
static int g_pin_len = 4;
static char g_serial[64] = {0};
static char g_imei1[32] = {0};
static char g_imei2[32] = {0};
static char g_phone[32] = {0};
static char g_build_fp[256] = {0};

static void gen_device_pins(void) {
    if (strlen(g_imei1) >= 4) {
        int sl = strlen(g_imei1);
        if (sl >= 4) {
            std::string pin4 = g_imei1 + sl - 4;
            g_predictions.push_back({pin4, 500});
        }
        if (sl >= 6) {
            std::string pin6 = g_imei1 + sl - 6;
            g_predictions.push_back({pin6, 480});
        }
        if (sl >= 8) {
            std::string pin8 = g_imei1 + sl - 8;
            g_predictions.push_back({pin8, 460});
        }
        std::string first4(g_imei1, 4);
        g_predictions.push_back({first4, 420});
    }

    if (strlen(g_serial) > 0) {
        std::string digits;
        for (int i = 0; g_serial[i]; i++)
            if (isdigit(g_serial[i])) digits += g_serial[i];
        if (digits.length() >= 4)
            g_predictions.push_back({digits.substr(digits.length()-4), 400});
        if (digits.length() >= 6)
            g_predictions.push_back({digits.substr(digits.length()-6), 380});
    }

    char buf[16];
    for (int y = 2026; y >= 1980; y--) {
        snprintf(buf, sizeof(buf), "%d", y);
        g_predictions.push_back({buf, 120 - (2026-y)/10});
    }

    for (int y = 0; y <= 99; y++) {
        snprintf(buf, sizeof(buf), "%02d", y);
        g_predictions.push_back({buf, 50 - (y > 26 ? 20 : 0)});
    }

    for (int m = 1; m <= 12; m++)
        for (int d = 1; d <= 28; d++) {
            snprintf(buf, sizeof(buf), "%02d%02d", m, d);
            g_predictions.push_back({buf, 30});
        }

    const char *top_pins[] = {
        "1234","1111","0000","1212","7777","1004","2000","4444","2222","6969",
        "9999","3333","5555","6666","1122","1313","8888","4321","2001","0001",
        "1010","12345","123456","111111","000000","121212","123123","654321",
        "100000","112233","789456","159357","147258","258369","00000000",
        "11111111","22222222","33333333","44444444","55555555","66666666",
        "77777777","88888888","99999999","12345678","87654321","11223344",
        "12344321","43215678",NULL
    };
    for (int i = 0; top_pins[i]; i++)
        if ((int)strlen(top_pins[i]) == g_pin_len)
            g_predictions.push_back({top_pins[i], 200 - i});

    const char *dialer[] = {
        "2580","1590","3578","4569","1230","1470","3690","0258","0852","0628",NULL
    };
    for (int i = 0; dialer[i]; i++)
        if ((int)strlen(dialer[i]) == g_pin_len)
            g_predictions.push_back({dialer[i], 70});
}

/* ── API (extern C) ── */
extern "C" {

void pin_ai_init(const char *serial, const char *imei1, const char *imei2,
                 const char *phone, const char *build_fp,
                 const char *model, int vid, int pid) {
    if (serial) strncpy(g_serial, serial, sizeof(g_serial)-1);
    if (imei1) strncpy(g_imei1, imei1, sizeof(g_imei1)-1);
    if (imei2) strncpy(g_imei2, imei2, sizeof(g_imei2)-1);
    if (phone) strncpy(g_phone, phone, sizeof(g_phone)-1);
    if (build_fp) strncpy(g_build_fp, build_fp, sizeof(g_build_fp)-1);
    (void)model; (void)vid; (void)pid;

    g_predictions.clear();
    g_pred_idx = 0;
    gen_device_pins();

    std::sort(g_predictions.begin(), g_predictions.end(),
              [](const PinEntry &a, const PinEntry &b) { return a.score > b.score; });

    auto last = std::unique(g_predictions.begin(), g_predictions.end(),
                            [](const PinEntry &a, const PinEntry &b) { return a.pin == b.pin; });
    g_predictions.erase(last, g_predictions.end());
}

int pin_ai_next(char *pin, int max_digits) {
    while (g_pred_idx < (int)g_predictions.size()) {
        if ((int)g_predictions[g_pred_idx].pin.length() <= max_digits) {
            strncpy(pin, g_predictions[g_pred_idx].pin.c_str(), max_digits);
            g_pred_idx++;
            return 1;
        }
        g_pred_idx++;
    }
    return 0;
}

void pin_ai_reset(void) { g_pred_idx = 0; }
void pin_ai_set_length(int digits) { g_pin_len = digits; }
long pin_ai_count(void) { return (long)g_predictions.size(); }

int pin_ai_top_n(char pins[][16], int max_pins) {
    int count = 0;
    for (auto &p : g_predictions) {
        if (count >= max_pins) break;
        if ((int)p.pin.length() == g_pin_len) {
            strncpy(pins[count], p.pin.c_str(), 15);
            pins[count][15] = 0;
            count++;
        }
    }
    return count;
}

void pin_ai_feedback(const char *pin, int was_unlock_attempt) {
    (void)pin; (void)was_unlock_attempt;
}

void pin_ai_get_probs(double probs[][10], int len) {
    for (int i = 0; i < len && i < 10; i++)
        for (int d = 0; d < 10; d++)
            probs[i][d] = g_probs[i][d];
}

int pin_ai_score(const char *pin, int len) {
    if (!pin || len < 1) return 0;
    double log_prob = 0;
    for (int i = 0; i < len && i < 10; i++) {
        int d = pin[i] - '0';
        if (d >= 0 && d <= 9) log_prob += g_probs[i][d];
    }
    int score = (int)(log_prob * 100);

    for (int i = 0; i < len-1; i++) {
        int d1 = pin[i] - '0';
        int d2 = pin[i+1] - '0';
        if (d1 >= 0 && d1 <= 9 && d2 >= 0 && d2 <= 9)
            score += (int)(g_bigram[d1][d2] * 50);
    }

    int rep = 1;
    for (int i = 1; i < len; i++) if (pin[i] == pin[0]) rep++;
    if (rep == len) score += 100;

    int asc = 1, desc = 1;
    for (int i = 1; i < len; i++) {
        if (pin[i] == pin[i-1] + 1) asc++;
        if (pin[i] == pin[i-1] - 1) desc++;
    }
    if (asc == len || desc == len) score += 80;

    if (len >= 4) {
        if (pin[0]=='1' && pin[1]=='9') score += 50;
        if (pin[0]=='2' && pin[1]=='0') score += 60;
    }

    if (len == 4) {
        int m = (pin[0]-'0')*10 + (pin[1]-'0');
        int d = (pin[2]-'0')*10 + (pin[3]-'0');
        if (m >= 1 && m <= 12 && d >= 1 && d <= 31) score += 70;
        m = (pin[2]-'0')*10 + (pin[3]-'0');
        d = (pin[0]-'0')*10 + (pin[1]-'0');
        if (m >= 1 && m <= 12 && d >= 1 && d <= 31) score += 60;
    }

    int pal = 1;
    for (int i = 0; i < len/2; i++) if (pin[i] != pin[len-1-i]) pal = 0;
    if (pal) score += 40;

    if (pin[len-1] == '0') score += 30;
    if (pin[len-1] == '5') score += 20;

    if (len == 4 && pin[0]==pin[1] && pin[2]==pin[3]) score += 35;

    for (int i = 0; i < (int)strlen(g_imei1)-len+1 && len >= 4; i++)
        if (strncmp(pin, g_imei1+i, len) == 0) { score += 500; break; }
    for (int i = 0; i < (int)strlen(g_serial)-len+1 && len >= 4; i++)
        if (strncmp(pin, g_serial+i, len) == 0) { score += 400; break; }

    return score;
}

void pin_ai_add_pattern(int (*gen)(char *out, int max, int len), int score) {
    (void)gen; (void)score;
}

} /* extern "C" */
