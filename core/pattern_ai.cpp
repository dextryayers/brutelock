#include "pattern_ai.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <ctime>

static double freq_score(const std::string &pin) {
    double score = 0.0;
    bool asc = true, desc = true, same = true;
    for (size_t i = 1; i < pin.size(); i++) {
        if (pin[i] <= pin[i-1]) asc = false;
        if (pin[i] >= pin[i-1]) desc = false;
        if (pin[i] != pin[i-1]) same = false;
    }
    if (same) score += 5.0;
    if (asc)  score += 3.0;
    if (desc) score += 3.0;

    if (pin.find("123") != std::string::npos ||
        pin.find("234") != std::string::npos ||
        pin.find("345") != std::string::npos ||
        pin.find("456") != std::string::npos ||
        pin.find("567") != std::string::npos ||
        pin.find("678") != std::string::npos ||
        pin.find("789") != std::string::npos)
        score += 4.0;

    if (pin.find("000") != std::string::npos ||
        pin.find("111") != std::string::npos ||
        pin.find("222") != std::string::npos ||
        pin.find("333") != std::string::npos ||
        pin.find("444") != std::string::npos ||
        pin.find("555") != std::string::npos ||
        pin.find("666") != std::string::npos ||
        pin.find("777") != std::string::npos ||
        pin.find("888") != std::string::npos ||
        pin.find("999") != std::string::npos)
        score += 3.0;

    int sum = 0;
    for (char c : pin) sum += (c - '0');
    if (sum == 10 || sum == 20 || sum == 30) score += 2.0;
    if (pin[0] == pin[pin.size()-1]) score += 1.0;

    return score;
}

extern "C" int ai_predict_pins(const char *device_json, char results[][16], int max) {
    (void)device_json;
    srand(time(NULL));
    int count = 0;

    static const char *top[] = {
        "1234","0000","1111","2222","3333","4444","5555","6666","7777",
        "8888","9999","1212","4321","2000","2001","6969","1000","1122",
        "1313","0001","0007","1004","1337","2580","1590","1235","2468",
        "1357","5683","0852"
    };
    for (auto p : top) {
        if (count < max) snprintf(results[count++], 16, "%s", p);
    }

    std::vector<std::string> candidates;
    int digits = 4;
    long total = 1;
    for (int d = 0; d < digits; d++) total *= 10;
    for (long i = 0; i < total && candidates.size() < 10000; i++) {
        char buf[16]; snprintf(buf, sizeof(buf), "%0*ld", digits, i);
        candidates.push_back(buf);
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const std::string &a, const std::string &b) {
                  return freq_score(a) > freq_score(b);
              });

    for (auto &c : candidates) {
        if (count >= max) break;
        bool dup = false;
        for (int i = 0; i < count; i++)
            if (strcmp(results[i], c.c_str()) == 0) { dup = true; break; }
        if (!dup) snprintf(results[count++], 16, "%s", c.c_str());
    }

    return count;
}

extern "C" int ai_generate_sequence(int digits, int count, char results[][16]) {
    srand(time(NULL));
    for (int i = 0; i < count; i++) {
        long v = 0;
        for (int d = 0; d < digits; d++)
            v = v * 10 + (rand() % 10);
        snprintf(results[i], 16, "%0*ld", digits, v);
    }
    return count;
}
