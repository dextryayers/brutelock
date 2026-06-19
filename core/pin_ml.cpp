#include "pin_ml.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <algorithm>
#include <string>
#include <map>

struct PinScore {
    char pin[16];
    double score;
};

static bool cmp_score(const PinScore &a, const PinScore &b) {
    return a.score > b.score;
}

static int default_freq[10] = {1,2,3,4,5,6,7,8,9,0};

void ml_set_frequency_data(const int freq[10]) {
    for (int i=0; i<10; i++) default_freq[i] = freq[i];
}

int ml_score_pin(const char *pin) {
    int len = (int)strlen(pin);
    if (len < 3) return 0;
    double score = 0;

    /* Repeated digits: 1111 */
    int rep = 0;
    for (int i=1; i<len; i++) if (pin[i]==pin[0]) rep++;
    if (rep == len-1) score += 100;
    else score += rep * 5;

    /* Ascending/descending sequences */
    int asc=0, desc=0;
    for (int i=1; i<len; i++) {
        if (pin[i] == pin[i-1]+1) asc++;
        if (pin[i] == pin[i-1]-1) desc++;
    }
    score += asc * 8 + desc * 7;

    /* Common pairs: 12, 69, 00 */
    for (int i=0; i<len-1; i++) {
        if (pin[i]=='1' && pin[i+1]=='2') score += 4;
        if (pin[i]=='6' && pin[i+1]=='9') score += 6;
        if (pin[i]=='0' && pin[i+1]=='0') score += 3;
        if (pin[i]=='2' && pin[i+1]=='5') score += 3;
        if (pin[i]=='5' && pin[i+1]=='8') score += 3;
        if (pin[i]=='1' && pin[i+1]=='3') score += 3;
        if (pin[i]=='1' && pin[i+1]=='4') score += 2;
    }

    /* Digit sum popularity */
    int sum = 0;
    for (int i=0; i<len; i++) sum += pin[i]-'0';
    if (sum == 10 || sum == 20) score += 5;
    if (sum == 15 || sum == 25) score += 3;
    if (sum == 0) score += 2;

    /* First==last digit */
    if (len >= 2 && pin[0] == pin[len-1]) score += 4;

    /* Palindrome */
    int pal = 1;
    for (int i=0; i<len/2 && pal; i++)
        if (pin[i]!=pin[len-1-i]) pal=0;
    if (pal) score += 15;

    /* Year pattern */
    if (len == 4) {
        int y = atoi(pin);
        if ((y >= 1950 && y <= 2026) || (y >= 195 && y <= 226)) score += 6;
    }

    /* Digit frequency */
    int f[10]={0};
    for (int i=0; i<len; i++) f[pin[i]-'0']++;
    for (int i=0; i<10; i++) {
        if (f[i] >= 2) score += 2 * f[i];
    }

    /* Common patterns */
    if (strcmp(pin,"0000")==0||strcmp(pin,"1234")==0||strcmp(pin,"1111")==0) score += 50;
    if (strcmp(pin,"1212")==0||strcmp(pin,"4321")==0||strcmp(pin,"6969")==0) score += 30;
    if (len==6) {
        if (strcmp(pin,"000000")==0||strcmp(pin,"123456")==0) score += 60;
    }
    if (len==8) {
        if (strcmp(pin,"00000000")==0||strcmp(pin,"12345678")==0) score += 70;
    }

    /* Dialer pattern (2580, 1590) */
    if (strcmp(pin,"2580")==0||strcmp(pin,"1590")==0) score += 20;
    if (strcmp(pin,"2580")==0||strcmp(pin,"1590")==0) score += 20;
    if (strcmp(pin,"0852")==0||strcmp(pin,"0951")==0) score += 15;

    /* Phone keypad column */
    if (pin[0]=='1'&&pin[1]=='4'&&pin[2]=='7'&&pin[3]=='*') score += 10;
    if (pin[0]=='2'&&pin[1]=='5'&&pin[2]=='8'&&pin[3]=='0') score += 12;
    if (pin[0]=='3'&&pin[1]=='6'&&pin[2]=='9'&&pin[3]=='#') score += 10;

    return (int)score;
}

int ml_predict_pins(const char *info_json, int digits, char pins[][16], int max) {
    (void)info_json;
    std::vector<PinScore> scored;

    long total = 1;
    for (int i=0; i<digits; i++) total *= 10;

    for (long i=0; i<total; i++) {
        char p[16];
        snprintf(p, sizeof(p), "%0*ld", digits, i);
        int s = ml_score_pin(p);
        if (s > 5) {
            PinScore ps;
            strncpy(ps.pin, p, 15); ps.pin[15]=0;
            ps.score = s;
            scored.push_back(ps);
        }
    }

    std::sort(scored.begin(), scored.end(), cmp_score);

    int count = 0;
    for (size_t i=0; i<scored.size() && count < max; i++) {
        /* Deduplicate */
        int dup = 0;
        for (int j=0; j<count; j++)
            if (strcmp(pins[j], scored[i].pin)==0) { dup=1; break; }
        if (!dup) {
            snprintf(pins[count], 16, "%s", scored[i].pin);
            count++;
        }
    }
    return count;
}

int ml_sequence_to_pins(const double probs[10], int digits, char pins[][16], int max) {
    /* Generate PINs ordered by digit probability */
    std::vector<PinScore> scored;
    long total = 1;
    for (int i=0; i<digits; i++) total *= 10;

    for (long i=0; i<total && (long)scored.size() < max*2; i++) {
        char p[16];
        snprintf(p, sizeof(p), "%0*ld", digits, i);
        double prob = 1.0;
        for (int d=0; d<digits; d++) {
            int v = p[d]-'0';
            prob *= probs[v] > 0 ? probs[v] : 0.001;
        }
        PinScore ps;
        strncpy(ps.pin, p, 15); ps.pin[15]=0;
        ps.score = prob;
        scored.push_back(ps);
    }

    std::sort(scored.begin(), scored.end(), cmp_score);

    int count = 0;
    for (size_t i=0; i<scored.size() && count<max; i++) {
        int dup=0;
        for (int j=0; j<count; j++)
            if (strcmp(pins[j], scored[i].pin)==0) { dup=1; break; }
        if (!dup) {
            snprintf(pins[count], 16, "%s", scored[i].pin);
            count++;
        }
    }
    return count;
}

int ml_predict_bootloop_pins(int digits, char pins[][16], int max) {
    /* Special bootloop PIN predictor - uses frequencies from real-world data */
    /* Most common PINs on bootloop: digits repeating, dates, simple sequences */
    std::vector<PinScore> all;

    auto add = [&](const char *p, double s) {
        PinScore ps;
        strncpy(ps.pin, p, 15); ps.pin[15]=0;
        ps.score = s;
        all.push_back(ps);
    };

    /* Known top bootloop PINs */
    const char *top[] = {
        "0000","1234","1111","2222","3333","4444","5555","6666","7777","8888","9999",
        "1212","4321","2000","2001","6969","1004","1122","1313","0001","0007",
        "1337","2580","1590","1235","2468","1357","5683","0852","0123","1230"
    };
    for (size_t i=0; i<sizeof(top)/sizeof(top[0]); i++)
        add(top[i], 1000-i*10);

    if (digits==6) {
        const char *top6[] = {
            "000000","123456","111111","222222","333333","444444","555555",
            "666666","777777","888888","999999","123123","654321","112233",
            "121212","789456","159753","258000","000001","100200","696969"
        };
        for (size_t i=0; i<sizeof(top6)/sizeof(top6[0]); i++)
            add(top6[i], 900-i*10);
    }

    if (digits==8) {
        const char *top8[] = {
            "00000000","12345678","11111111","22222222","33333333",
            "44444444","55555555","66666666","77777777","88888888",
            "99999999","12341234","87654321","11223344"
        };
        for (size_t i=0; i<sizeof(top8)/sizeof(top8[0]); i++)
            add(top8[i], 800-i*10);
    }

    /* Add year-based pins */
    for (int y=2026; y>=1990; y--) {
        char buf[16];
        if (digits==4) {
            snprintf(buf,sizeof(buf),"%d",y%10000);
            add(buf, 100 + (2026-y));
        } else if (digits>=6) {
            snprintf(buf,sizeof(buf),"%d",y);
            if ((int)strlen(buf)==digits) add(buf, 100 + (2026-y));
        }
    }

    /* Add all scored generated */
    long total = 1;
    for (int i=0; i<digits; i++) total *= 10;
    for (long i=0; i<total; i++) {
        char p[16];
        snprintf(p, sizeof(p), "%0*ld", digits, i);
        double s = ml_score_pin(p) / 100.0;
        if (s > 0.1) add(p, s);
    }

    std::sort(all.begin(), all.end(), cmp_score);

    int count=0;
    for (size_t i=0; i<all.size() && count<max; i++) {
        int dup=0;
        for (int j=0; j<count; j++)
            if (strcmp(pins[j], all[i].pin)==0) { dup=1; break; }
        if (!dup) {
            snprintf(pins[count], 16, "%s", all[i].pin);
            count++;
        }
    }
    return count;
}

extern "C" void ml_free_pins(char pins[][16], int count) {
    (void)pins; (void)count;
}
