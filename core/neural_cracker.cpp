#include "neural_cracker.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <set>

const int NeuralCracker::COMMON_COUNT = 500;

const char *NeuralCracker::COMMON_PINS[] = {
    "1234","0000","1111","2222","3333","4444","5555","6666","7777","8888","9999",
    "12345","00000","11111","123456","000000","111111","654321",
    "1122","1313","2000","2001","4321","5678","6969","7777","1212",
    "1000","2000","3000","4000","5000","6000","7000","8000","9000",
    "1230","1245","1345","1456","1567","1678","1789","1890",
    "2580","0852","1004","1010","1020","1030","1040","1050",
    "1212","1221","1230","1245","1250","1256","1260","1270",
    "1345","1357","1369","1379","1478","1479","1480","1490",
    "1500","1515","1520","1530","1540","1550","1560","1570",
    "1590","1593","1597","1600","1616","1620","1630","1640",
    "1680","1690","1700","1717","1720","1730","1740","1750",
    "1760","1770","1780","1790","1800","1818","1820","1830",
    "1840","1850","1860","1870","1880","1890","1900","1919",
    "1920","1930","1940","1945","1950","1960","1970","1980",
    "1990","1991","1992","1993","1994","1995","1996","1997",
    "1998","1999","2000","2001","2002","2003","2004","2005",
    "2006","2007","2008","2009","2010","2011","2012","2013",
    "2014","2015","2016","2017","2018","2019","2020","2021",
    "2022","2023","2024","2025","2026","2027","2028","2029",
    "2030","2112","2121","2211","2222","2233","2244","2255",
    "2266","2277","2288","2299","2323","2332","2345","2356",
    "2424","2442","2468","2486","2500","2525","2552","2569",
    "2580","2589","2626","2639","2662","2684","2690","2718",
    "2727","2742","2759","2772","2790","2812","2828","2842",
    "2850","2874","2882","2900","2929","2941","2963","2985",
    "3000","3030","3112","3131","3141","3159","3186","3200",
    "3210","3211","3222","3232","3245","3256","3269","3280",
    "3300","3322","3333","3344","3355","3366","3377","3388",
    "3399","3400","3434","3456","3478","3489","3500","3535",
    "3553","3579","3600","3630","3652","3675","3690","3699",
    "3700","3737","3759","3780","3790","3800","3838","3850",
    "3870","3890","3900","3939","3960","3980","4000","4040",
    "4112","4141","4200","4210","4224","4242","4256","4278",
    "4300","4321","4343","4356","4380","4400","4422","4444",
    "4455","4466","4477","4488","4499","4500","4545","4567",
    "4580","4600","4620","4646","4660","4680","4700","4747",
    "4760","4780","4800","4848","4860","4880","4900","4949",
    "4960","4980","5000","5050","5100","5112","5151","5200",
    "5210","5222","5252","5280","5300","5321","5353","5400",
    "5432","5454","5460","5480","5500","5522","5533","5544",
    "5555","5566","5577","5588","5599","5600","5656","5680",
    "5700","5757","5780","5800","5858","5880","5900","5959",
    "5980","6000","6060","6100","6112","6161","6200","6210",
    "6222","6252","6280","6300","6321","6353","6400","6420",
    "6456","6480","6500","6520","6543","6565","6580","6600",
    "6622","6655","6666","6677","6688","6699","6700","6767",
    "6780","6789","6800","6868","6880","6900","6969","6980",
    "7000","7070","7100","7112","7171","7200","7210","7222",
    "7252","7280","7300","7321","7353","7400","7420","7456",
    "7480","7500","7520","7543","7575","7580","7600","7620",
    "7654","7676","7680","7700","7722","7755","7777","7788",
    "7799","7800","7860","7878","7880","7900","7979","7980",
    "8000","8080","8100","8112","8181","8200","8210","8222",
    "8252","8280","8300","8321","8353","8400","8420","8456",
    "8480","8500","8520","8543","8575","8580","8600","8620",
    "8654","8680","8700","8720","8765","8780","8800","8822",
    "8855","8888","8899","8900","8980","9000","9090","9100",
    "9112","9181","9200","9210","9222","9252","9280","9300",
    "9321","9353","9400","9420","9456","9480","9500","9520",
    "9543","9575","9580","9600","9620","9654","9680","9700",
    "9720","9765","9780","9800","9822","9855","9880","9900",
    "9960","9980","9999"
};

NeuralCracker::NeuralCracker()
    : m_digits(4)
    , m_markov_order(MAX_MARKOV_ORDER)
    , m_attempt_count(0)
    , m_rng(std::chrono::steady_clock::now().time_since_epoch().count())
    , w_markov(0.35)
    , w_common(0.25)
    , w_device(0.15)
    , w_breed(0.10)
    , w_pattern(0.15)
{
}

NeuralCracker::~NeuralCracker()
{
}

void NeuralCracker::build_markov(const std::string &pin)
{
    int len = (int)pin.length();
    for (int order = 1; order <= MAX_MARKOV_ORDER; ++order) {
        for (int i = 0; i + order < len; ++i) {
            std::string prefix = pin.substr(i, order);
            char next = pin[i + order];

            MarkovState &st = m_markov[prefix];
            if (st.prefix.empty()) {
                st.prefix = prefix;
                st.total = 0;
                st.probability = 0.0;
            }

            bool found = false;
            for (size_t j = 0; j < st.next_chars.size(); ++j) {
                if (st.next_chars[j] == next) {
                    st.counts[j]++;
                    found = true;
                    break;
                }
            }
            if (!found) {
                st.next_chars.push_back(next);
                st.counts.push_back(1);
            }
            st.total++;
        }
    }
}

void NeuralCracker::train_from_common_pins()
{
    for (int i = 0; i < COMMON_COUNT; ++i) {
        build_markov(std::string(COMMON_PINS[i]));
    }
}

void NeuralCracker::train_from_patterns()
{
    std::vector<std::string> patterns;
    for (int d = 4; d <= 8; ++d) {
        std::vector<std::string> kp = common_keypad_patterns(d);
        patterns.insert(patterns.end(), kp.begin(), kp.end());
    }
    for (size_t i = 0; i < patterns.size(); ++i) {
        build_markov(patterns[i]);
    }
}

void NeuralCracker::train_from_list(const std::vector<std::string> &pins)
{
    for (size_t i = 0; i < pins.size(); ++i) {
        build_markov(pins[i]);
    }
}

void NeuralCracker::add_device_numbers(const std::vector<std::string> &numbers)
{
    for (size_t i = 0; i < numbers.size(); ++i) {
        if (!numbers[i].empty()) {
            std::string clean;
            for (size_t j = 0; j < numbers[i].length(); ++j) {
                if (isdigit(numbers[i][j]))
                    clean += numbers[i][j];
            }
            if (!clean.empty() && clean.length() >= 4) {
                build_markov(clean);
            }
        }
    }
}

std::vector<char> NeuralCracker::predict_next(const std::string &prefix)
{
    std::vector<char> result;
    for (int order = MAX_MARKOV_ORDER; order >= 1; --order) {
        if ((int)prefix.length() < order)
            continue;
        std::string key = prefix.substr(prefix.length() - order, order);
        auto it = m_markov.find(key);
        if (it != m_markov.end() && it->second.total > 0) {
            const MarkovState &st = it->second;
            for (size_t i = 0; i < st.next_chars.size(); ++i) {
                result.push_back(st.next_chars[i]);
            }
            return result;
        }
    }
    for (char c = '0'; c <= '9'; ++c)
        result.push_back(c);
    return result;
}

std::string NeuralCracker::generate_from_model()
{
    std::string pin;
    while ((int)pin.length() < m_digits) {
        std::vector<char> next = predict_next(pin);
        if (next.empty()) {
            pin += '0' + (m_rng() % 10);
            continue;
        }
        int total_weight = 0;
        std::string key;
        if (!pin.empty()) {
            for (int order = MAX_MARKOV_ORDER; order >= 1; --order) {
                if ((int)pin.length() >= order) {
                    key = pin.substr(pin.length() - order, order);
                    auto it = m_markov.find(key);
                    if (it != m_markov.end()) {
                        total_weight = it->second.total;
                        break;
                    }
                }
            }
        }
        if (total_weight == 0) {
            total_weight = (int)next.size();
        }
        int roll = m_rng() % total_weight;
        int cumulative = 0;
        char chosen = next[0];
        std::string key2;
        if (!pin.empty()) {
            for (int order = MAX_MARKOV_ORDER; order >= 1; --order) {
                if ((int)pin.length() >= order) {
                    key2 = pin.substr(pin.length() - order, order);
                    auto it = m_markov.find(key2);
                    if (it != m_markov.end()) {
                        for (size_t i = 0; i < it->second.next_chars.size(); ++i) {
                            cumulative += it->second.counts[i];
                            if (roll < cumulative) {
                                chosen = it->second.next_chars[i];
                                break;
                            }
                        }
                        break;
                    }
                }
            }
        }
        if (cumulative == 0) {
            chosen = next[roll % next.size()];
        }
        pin += chosen;
    }
    return pin;
}

std::vector<PinScore> NeuralCracker::generate_markov_candidates(int max_count, int digits)
{
    int saved = m_digits;
    m_digits = digits;
    std::set<std::string> seen;
    std::vector<PinScore> candidates;
    int attempts = 0;
    int max_attempts = max_count * 10;
    while ((int)candidates.size() < max_count && attempts < max_attempts) {
        std::string pin = generate_from_model();
        attempts++;
        if (pin.length() != (size_t)digits)
            continue;
        if (seen.find(pin) != seen.end())
            continue;
        seen.insert(pin);
        PinScore ps;
        ps.pin = pin;
        ps.score = score_pin(pin);
        ps.source = 0;
        ps.source_name = "Markov";
        candidates.push_back(ps);
    }
    m_digits = saved;
    std::sort(candidates.begin(), candidates.end(),
              [](const PinScore &a, const PinScore &b) { return a.score > b.score; });
    if ((int)candidates.size() > max_count)
        candidates.resize(max_count);
    return candidates;
}

std::vector<PinScore> NeuralCracker::generate_common_candidates(int max_count, int digits)
{
    std::vector<PinScore> candidates;
    std::set<std::string> seen;
    for (int i = 0; i < COMMON_COUNT; ++i) {
        std::string pin(COMMON_PINS[i]);
        if ((int)pin.length() != digits)
            continue;
        if (seen.find(pin) != seen.end())
            continue;
        seen.insert(pin);
        PinScore ps;
        ps.pin = pin;
        ps.score = score_pin(pin);
        ps.source = 1;
        ps.source_name = "Common";
        candidates.push_back(ps);
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const PinScore &a, const PinScore &b) { return a.score > b.score; });
    if ((int)candidates.size() > max_count)
        candidates.resize(max_count);
    return candidates;
}

std::vector<PinScore> NeuralCracker::generate_device_specific(int digits)
{
    std::vector<PinScore> candidates;
    std::set<std::string> seen;
    std::vector<std::string> derived = device_derived_pins(digits);
    for (size_t i = 0; i < derived.size(); ++i) {
        if ((int)derived[i].length() != digits)
            continue;
        if (seen.find(derived[i]) != seen.end())
            continue;
        seen.insert(derived[i]);
        PinScore ps;
        ps.pin = derived[i];
        ps.score = score_pin(derived[i]);
        ps.source = 2;
        ps.source_name = "DeviceSpec";
        candidates.push_back(ps);
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const PinScore &a, const PinScore &b) { return a.score > b.score; });
    return candidates;
}

std::vector<PinScore> NeuralCracker::generate_pattern_based(int digits)
{
    std::vector<PinScore> candidates;
    std::set<std::string> seen;
    std::vector<std::string> patterns = common_keypad_patterns(digits);
    for (size_t i = 0; i < patterns.size(); ++i) {
        if ((int)patterns[i].length() != digits)
            continue;
        if (seen.find(patterns[i]) != seen.end())
            continue;
        seen.insert(patterns[i]);
        PinScore ps;
        ps.pin = patterns[i];
        ps.score = score_pin(patterns[i]);
        ps.source = 4;
        ps.source_name = "Pattern";
        candidates.push_back(ps);
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const PinScore &a, const PinScore &b) { return a.score > b.score; });
    return candidates;
}

std::vector<PinScore> NeuralCracker::generate_sequential(int digits, long start, long end)
{
    std::vector<PinScore> candidates;
    char fmt[32];
    for (long i = start; i <= end; ++i) {
        snprintf(fmt, sizeof(fmt), "%0*ld", digits, i);
        std::string pin(fmt);
        if ((int)pin.length() != digits)
            continue;
        PinScore ps;
        ps.pin = pin;
        ps.score = score_pin(pin);
        ps.source = 0;
        ps.source_name = "Sequential";
        candidates.push_back(ps);
    }
    return candidates;
}

std::vector<PinScore> NeuralCracker::generate_ai_priority(int digits)
{
    std::vector<PinScore> candidates;
    std::set<std::string> seen;

    int half = TOP_N / 2;
    std::vector<PinScore> markov = generate_markov_candidates(half, digits);
    for (size_t i = 0; i < markov.size(); ++i) {
        if (seen.find(markov[i].pin) != seen.end())
            continue;
        seen.insert(markov[i].pin);
        PinScore ps = markov[i];
        ps.source = 0;
        ps.source_name = "AIPriority";
        candidates.push_back(ps);
    }

    int remaining = TOP_N - (int)candidates.size();
    if (remaining > 0) {
        std::vector<PinScore> common = generate_common_candidates(remaining, digits);
        for (size_t i = 0; i < common.size(); ++i) {
            if (seen.find(common[i].pin) != seen.end())
                continue;
            seen.insert(common[i].pin);
            PinScore ps = common[i];
            ps.source = 1;
            ps.source_name = "AIPriority";
            candidates.push_back(ps);
        }
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const PinScore &a, const PinScore &b) { return a.score > b.score; });
    if ((int)candidates.size() > TOP_N)
        candidates.resize(TOP_N);
    return candidates;
}

std::vector<PinScore> NeuralCracker::generate_candidates(int max_count, int digits)
{
    std::set<std::string> seen;
    std::vector<PinScore> all;

    int markov_count = std::max(1, max_count / 3);
    int common_count = std::max(1, max_count / 4);
    int pattern_count = std::max(1, max_count / 6);
    int device_count = std::max(1, max_count / 8);
    int seq_count = max_count - markov_count - common_count - pattern_count - device_count;

    std::vector<PinScore> markov = generate_markov_candidates(markov_count, digits);
    std::vector<PinScore> common = generate_common_candidates(common_count, digits);
    std::vector<PinScore> pattern = generate_pattern_based(digits);
    std::vector<PinScore> device = generate_device_specific(digits);
    std::vector<PinScore> seq = generate_sequential(digits, 0, (long)pow(10, digits) - 1);

    auto add_unique = [&](std::vector<PinScore> &src, int limit) {
        int added = 0;
        for (size_t i = 0; i < src.size() && added < limit; ++i) {
            if (seen.find(src[i].pin) != seen.end())
                continue;
            seen.insert(src[i].pin);
            all.push_back(src[i]);
            added++;
        }
    };

    add_unique(markov, markov_count);
    add_unique(common, common_count);
    add_unique(pattern, pattern_count);
    add_unique(device, device_count);
    add_unique(seq, seq_count);

    std::sort(all.begin(), all.end(),
              [](const PinScore &a, const PinScore &b) { return a.score > b.score; });
    if ((int)all.size() > max_count)
        all.resize(max_count);
    return all;
}

double NeuralCracker::markov_probability(const std::string &pin)
{
    if (pin.empty())
        return 0.0;
    double prob = 1.0;
    for (int order = 1; order <= MAX_MARKOV_ORDER; ++order) {
        double order_prob = 0.0;
        int matches = 0;
        for (size_t i = 0; i + (size_t)order < pin.length(); ++i) {
            std::string prefix = pin.substr(i, order);
            char observed = pin[i + order];
            auto it = m_markov.find(prefix);
            if (it != m_markov.end() && it->second.total > 0) {
                const MarkovState &st = it->second;
                for (size_t j = 0; j < st.next_chars.size(); ++j) {
                    if (st.next_chars[j] == observed) {
                        double p = (double)st.counts[j] / (double)st.total;
                        order_prob += p;
                        matches++;
                        break;
                    }
                }
            }
        }
        if (matches > 0) {
            prob *= (order_prob / matches);
        }
    }
    return prob;
}

double NeuralCracker::common_penalty(const std::string &pin)
{
    for (int i = 0; i < COMMON_COUNT; ++i) {
        if (pin == COMMON_PINS[i]) {
            return 1.0 - ((double)i / (double)COMMON_COUNT);
        }
    }
    return 0.0;
}

double NeuralCracker::device_similarity(const std::string &pin)
{
    double best = 0.0;
    std::vector<std::string> device_numbers;
    if (!m_phone.empty()) device_numbers.push_back(m_phone);
    if (!m_imei.empty()) device_numbers.push_back(m_imei);
    if (!m_serial.empty()) device_numbers.push_back(m_serial);

    for (size_t n = 0; n < device_numbers.size(); ++n) {
        std::string clean;
        for (size_t i = 0; i < device_numbers[n].length(); ++i) {
            if (isdigit(device_numbers[n][i]))
                clean += device_numbers[n][i];
        }
        if (clean.empty())
            continue;
        int match_len = 0;
        int max_len = std::min((int)pin.length(), (int)clean.length());
        for (int i = 1; i <= max_len; ++i) {
            if (pin.substr(pin.length() - i) == clean.substr(clean.length() - i)) {
                match_len = i;
            } else {
                break;
            }
        }
        double sim = (double)match_len / (double)pin.length();
        if (sim > best) best = sim;

        std::string rev_clean(clean.rbegin(), clean.rend());
        match_len = 0;
        for (int i = 1; i <= max_len; ++i) {
            if (pin.substr(pin.length() - i) == rev_clean.substr(rev_clean.length() - i)) {
                match_len = i;
            } else {
                break;
            }
        }
        sim = (double)match_len / (double)pin.length();
        if (sim > best) best = sim;
    }

    for (size_t i = 0; i < m_breed_pins.size(); ++i) {
        const std::string &bp = m_breed_pins[i];
        int match_len = 0;
        int max_len = std::min((int)pin.length(), (int)bp.length());
        for (int j = 1; j <= max_len; ++j) {
            if (pin.substr(pin.length() - j) == bp.substr(bp.length() - j)) {
                match_len = j;
            } else {
                break;
            }
        }
        double sim = (double)match_len / (double)pin.length();
        if (sim > best) best = sim;
    }

    return best;
}

double NeuralCracker::score_pin(const std::string &pin)
{
    double s_markov = markov_probability(pin) * w_markov;
    double s_common = common_penalty(pin) * w_common;
    double s_device = device_similarity(pin) * w_device;
    double s_breed = 0.0;
    for (size_t i = 0; i < m_breed_pins.size(); ++i) {
        if (pin == m_breed_pins[i]) {
            s_breed = 1.0 * w_breed;
            break;
        }
    }

    double s_pattern = 0.0;
    std::vector<std::string> patterns = common_keypad_patterns((int)pin.length());
    for (size_t i = 0; i < patterns.size(); ++i) {
        if (pin == patterns[i]) {
            s_pattern = 1.0 * w_pattern;
            break;
        }
    }

    return s_markov + s_common + s_device + s_breed + s_pattern;
}

std::vector<std::string> NeuralCracker::common_keypad_patterns(int digits)
{
    std::vector<std::string> patterns;

    if (digits <= 0) return patterns;

    /* Sequential ascending */
    for (int start = 0; start <= 9; ++start) {
        std::string p;
        for (int i = 0; i < digits; ++i) {
            p += '0' + ((start + i) % 10);
        }
        patterns.push_back(p);
    }

    /* Sequential descending */
    for (int start = 0; start <= 9; ++start) {
        std::string p;
        for (int i = 0; i < digits; ++i) {
            int d = (start - i) % 10;
            if (d < 0) d += 10;
            p += '0' + d;
        }
        patterns.push_back(p);
    }

    /* Repeated digits */
    for (int d = 0; d <= 9; ++d) {
        std::string p(digits, '0' + d);
        patterns.push_back(p);
    }

    /* Palindromes */
    if (digits >= 2) {
        for (int start = 0; start <= 9; ++start) {
            std::string p;
            for (int i = 0; i < digits; ++i) {
                if (i < digits / 2) {
                    p += '0' + ((start + i) % 10);
                } else {
                    p += p[digits - 1 - i];
                }
            }
            patterns.push_back(p);
        }
    }

    /* Keypad rows - standard phone keypad
       1 2 3
       4 5 6
       7 8 9
       * 0 #
    */
    const char *rows[] = {"123", "456", "789", "147", "258", "369", "048"};
    int row_count = 7;
    for (int r = 0; r < row_count; ++r) {
        std::string row(rows[r]);
        if ((int)row.length() >= digits) {
            patterns.push_back(row.substr(0, digits));
        }
        if ((int)row.length() >= digits) {
            std::string rev(row.rbegin(), row.rend());
            patterns.push_back(rev.substr(0, digits));
        }
        /* Extended patterns along rows */
        if (digits > (int)row.length()) {
            std::string ext;
            for (int i = 0; i < digits; ++i) {
                ext += row[i % row.length()];
            }
            patterns.push_back(ext);
        }
    }

    /* Keypad diagonals */
    const char *diags[] = {"159", "357"};
    int diag_count = 2;
    for (int d = 0; d < diag_count; ++d) {
        std::string diag(diags[d]);
        if ((int)diag.length() >= digits) {
            patterns.push_back(diag.substr(0, digits));
        }
        if ((int)diag.length() >= digits) {
            std::string rev(diag.rbegin(), diag.rend());
            patterns.push_back(rev.substr(0, digits));
        }
        if (digits > (int)diag.length()) {
            std::string ext;
            for (int i = 0; i < digits; ++i) {
                ext += diag[i % diag.length()];
            }
            patterns.push_back(ext);
        }
    }

    /* Cross pattern: 1379 corners, then 5 */
    std::string cross = "1379";
    patterns.push_back(cross);
    patterns.push_back("13795");
    patterns.push_back("1397");
    patterns.push_back("13795");

    /* Keypad binary pattern: center columns */
    patterns.push_back("2580");
    patterns.push_back("0852");

    /* Dates MMDD for 4 digits */
    if (digits == 4) {
        for (int m = 1; m <= 12; ++m) {
            int max_d = 31;
            if (m == 2) max_d = 28;
            else if (m == 4 || m == 6 || m == 9 || m == 11) max_d = 30;
            for (int d = 1; d <= max_d; ++d) {
                char buf[16];
                snprintf(buf, sizeof(buf), "%02d%02d", m, d);
                patterns.push_back(std::string(buf));
            }
        }
    }

    /* Years for 4 digits */
    if (digits == 4) {
        for (int y = 1900; y <= 2099; ++y) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%04d", y);
            patterns.push_back(std::string(buf));
        }
    }

    /* Common repeating pairs */
    for (int d1 = 0; d1 <= 9; ++d1) {
        for (int d2 = 0; d2 <= 9; ++d2) {
            if (d1 == d2) continue;
            std::string pair;
            pair += '0' + d1;
            pair += '0' + d2;
            std::string p;
            while ((int)p.length() < digits) {
                p += pair;
            }
            if ((int)p.length() > digits)
                p.resize(digits);
            patterns.push_back(p);
        }
    }

    return patterns;
}

std::vector<std::string> NeuralCracker::device_derived_pins(int digits)
{
    std::vector<std::string> pins;
    std::vector<std::string> sources;

    if (!m_imei.empty()) sources.push_back(m_imei);
    if (!m_phone.empty()) sources.push_back(m_phone);
    if (!m_serial.empty()) sources.push_back(m_serial);

    for (size_t s = 0; s < sources.size(); ++s) {
        std::string clean;
        for (size_t i = 0; i < sources[s].length(); ++i) {
            if (isdigit(sources[s][i]))
                clean += sources[s][i];
        }
        if (clean.empty())
            continue;

        /* Last N digits */
        if ((int)clean.length() >= digits) {
            pins.push_back(clean.substr(clean.length() - digits));
        }

        /* First N digits */
        if ((int)clean.length() >= digits) {
            pins.push_back(clean.substr(0, digits));
        }

        /* Middle N digits */
        if ((int)clean.length() > digits) {
            int mid = ((int)clean.length() - digits) / 2;
            pins.push_back(clean.substr(mid, digits));
        }

        /* Every consecutive substring of length digits */
        if ((int)clean.length() > digits) {
            for (size_t i = 0; i + (size_t)digits <= clean.length(); ++i) {
                pins.push_back(clean.substr(i, digits));
            }
        }

        /* Reversed */
        std::string rev(clean.rbegin(), clean.rend());
        if ((int)rev.length() >= digits)
            pins.push_back(rev.substr(0, digits));
        if ((int)rev.length() >= digits)
            pins.push_back(rev.substr(rev.length() - digits));

        /* Incremented variants */
        for (int delta = -3; delta <= 3; ++delta) {
            if (delta == 0) continue;
            if ((int)clean.length() >= digits) {
                std::string last = clean.substr(clean.length() - digits);
                std::string var;
                for (size_t i = 0; i < last.length(); ++i) {
                    int d = (last[i] - '0' + delta) % 10;
                    if (d < 0) d += 10;
                    var += '0' + d;
                }
                pins.push_back(var);
            }
        }
    }

    /* Remove duplicates */
    std::set<std::string> uniq(pins.begin(), pins.end());
    pins.assign(uniq.begin(), uniq.end());

    /* Filter to matching length */
    std::vector<std::string> filtered;
    for (size_t i = 0; i < pins.size(); ++i) {
        if ((int)pins[i].length() == digits) {
            filtered.push_back(pins[i]);
        }
    }

    return filtered;
}

void NeuralCracker::record_attempt(const std::string &pin, bool success, double response_time)
{
    (void)response_time;
    m_attempt_history.push_back(pin);
    m_attempt_results.push_back(success);
    m_attempt_count++;

    if (m_attempt_count > 1000) {
        m_attempt_history.erase(m_attempt_history.begin());
        m_attempt_results.erase(m_attempt_results.begin());
        m_attempt_count = 1000;
    }

    if (success) {
        build_markov(pin);
        for (size_t i = 0; i + 4 <= pin.length(); ++i) {
            std::string sub = pin.substr(i, 4);
            build_markov(sub);
        }
        w_markov *= 1.02;
    } else {
        adjust_weights();
    }
}

void NeuralCracker::adjust_weights()
{
    double decay = 0.98;
    w_common *= decay;
    w_device *= decay;
    w_breed *= decay;
    w_pattern *= decay;

    double total = w_markov + w_common + w_device + w_breed + w_pattern;
    if (total > 0.0) {
        w_markov /= total;
        w_common /= total;
        w_device /= total;
        w_breed /= total;
        w_pattern /= total;
    } else {
        w_markov = 0.35;
        w_common = 0.25;
        w_device = 0.15;
        w_breed = 0.10;
        w_pattern = 0.15;
    }
}

double NeuralCracker::accuracy() const
{
    if (m_attempt_count == 0)
        return 0.0;
    int successes = 0;
    int count = std::min(m_attempt_count, 1000);
    int start = (int)m_attempt_results.size() - count;
    if (start < 0) start = 0;
    for (int i = start; i < (int)m_attempt_results.size(); ++i) {
        if (m_attempt_results[i])
            successes++;
    }
    return (double)successes / (double)count;
}

/* Kneser-Ney smoothing for Markov models */
double NeuralCracker::kneser_ney_prob(const std::string &context, char next) {
    auto it = m_markov.find(context);
    if (it == m_markov.end()) {
        /* Fall back to lower-order */
        if (context.length() > 1) return kneser_ney_prob(context.substr(1), next);
        return 0.1; /* uniform for single digit */
    }
    MarkovState &state = it->second;
    int count = 0;
    for (size_t i = 0; i < state.next_chars.size(); i++) {
        if (state.next_chars[i] == next) { count = state.counts[i]; break; }
    }
    double d = 0.75; /* Discount */
    double numerator = std::max(0.0, (double)count - d);
    double denominator = (double)state.total;
    /* Add continuation probability for unseen n-grams */
    double lambda = d * (double)state.next_chars.size() / denominator;
    double continuation = 0.0;
    if (context.length() > 1)
        continuation = kneser_ney_prob(context.substr(1), next);
    else
        continuation = 0.1;
    return numerator / denominator + lambda * continuation;
}

/* Temperature-based generation */
std::vector<PinScore> NeuralCracker::generate_with_temperature(int count, int digits, double temp) {
    std::vector<PinScore> candidates;
    std::set<std::string> seen;

    for (int i = 0; i < count * 10 && (int)candidates.size() < count; i++) {
        std::string pin;
        for (int d = 0; d < digits; d++) {
            std::string ctx = pin.length() > 3 ? pin.substr(pin.length() - 3) : pin;
            auto it = m_markov.find(ctx);
            std::vector<char> choices;
            std::vector<double> probs;
            if (it != m_markov.end()) {
                double total = 0;
                for (size_t j = 0; j < it->second.next_chars.size(); j++) {
                    double p = pow((double)it->second.counts[j] / it->second.total, 1.0 / temp);
                    choices.push_back(it->second.next_chars[j]);
                    probs.push_back(p);
                    total += p;
                }
                if (total > 0) {
                    double r = (double)rand() / RAND_MAX * total;
                    double cum = 0;
                    for (size_t j = 0; j < choices.size(); j++) {
                        cum += probs[j];
                        if (r <= cum) { pin += choices[j]; break; }
                    }
                } else {
                    pin += '0' + (rand() % 10);
                }
            } else {
                pin += '0' + (rand() % 10);
            }
        }
        if (pin.length() == (size_t)digits && seen.find(pin) == seen.end()) {
            seen.insert(pin);
            PinScore ps;
            ps.pin = pin;
            ps.score = score_pin(pin);
            ps.source = 0;
            ps.source_name = temp < 0.5 ? "Conservative" : (temp > 1.5 ? "Creative" : "Balanced");
            candidates.push_back(ps);
        }
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const PinScore &a, const PinScore &b) { return a.score > b.score; });
    if ((int)candidates.size() > count) candidates.resize(count);
    return candidates;
}

/* Bayesian PIN scoring */
double NeuralCracker::score_pin_bayesian(const std::string &pin) {
    double prior = 0.001; /* Default low prior */
    /* Check if PIN is in common list */
    for (int i = 0; i < COMMON_COUNT; i++) {
        if (pin == COMMON_PINS[i]) { prior = 1.0 / (i + 1); break; }
    }
    /* Likelihood from Markov model */
    double likelihood = markov_probability(pin);
    /* Evidence from device-specific data */
    double device_evidence = device_similarity(pin);
    /* Bayesian posterior */
    double posterior = prior * likelihood * (1.0 + device_evidence);
    return posterior;
}

/* PIN blocklist */
void NeuralCracker::add_to_blocklist(const std::string &pin) {
    m_attempt_history.push_back(pin);
    m_attempt_results.push_back(false);
    m_attempt_count++;
}

/* Ensemble scoring: combine multiple models */
double NeuralCracker::ensemble_score(const std::string &pin) {
    double s1 = score_pin(pin);
    double s2 = score_pin_bayesian(pin);
    double s3 = markov_probability(pin) * 100.0;
    return (s1 * 0.4 + s2 * 0.4 + s3 * 0.2);
}

std::vector<PinScore> NeuralCracker::generate_ensemble(int count, int digits) {
    std::vector<PinScore> all = generate_ai_priority(digits);
    for (auto &ps : all) ps.score = ensemble_score(ps.pin);
    std::sort(all.begin(), all.end(),
              [](const PinScore &a, const PinScore &b) { return a.score > b.score; });
    if ((int)all.size() > count) all.resize(count);
    return all;
}
