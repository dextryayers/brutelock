#ifndef NEURAL_CRACKER_H
#define NEURAL_CRACKER_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include <random>

#define MAX_PIN_DIGITS 16
#define MAX_MARKOV_ORDER 4
#define MAX_STATES 100000
#define TOP_N 256

struct MarkovState {
    std::string prefix;             /* Previous N digits */
    std::vector<char> next_chars;   /* Observed next digits */
    std::vector<int> counts;        /* Transition counts */
    int total;
    double probability;
};

struct PinScore {
    std::string pin;
    double score;
    int source; /* 0=Markov, 1=Common, 2=DeviceSpec, 3=Breed, 4=Pattern */
    std::string source_name;
};

class NeuralCracker {
public:
    NeuralCracker();
    ~NeuralCracker();

    /* Train Markov model from breach data */
    void train_from_common_pins();
    void train_from_patterns();
    void train_from_list(const std::vector<std::string> &pins);
    void add_device_numbers(const std::vector<std::string> &numbers);

    /* Generate candidate PINs */
    std::vector<PinScore> generate_candidates(int max_count, int digits);
    std::vector<PinScore> generate_markov_candidates(int max_count, int digits);
    std::vector<PinScore> generate_common_candidates(int max_count, int digits);
    std::vector<PinScore> generate_device_specific(int digits);
    std::vector<PinScore> generate_pattern_based(int digits);
    std::vector<PinScore> generate_sequential(int digits, long start, long end);
    std::vector<PinScore> generate_ai_priority(int digits);
    std::vector<PinScore> generate_ensemble(int count, int digits);

    /* Feedback learning */
    void record_attempt(const std::string &pin, bool success, double response_time);
    void adjust_weights();

    /* Temperature-based sampling */
    std::string generate_with_temperature(double temp);
    std::vector<PinScore> generate_candidates_temperature(int max_count, int digits, double temp);
    std::vector<PinScore> generate_with_temperature(int count, int digits, double temp);

    /* Bayesian inference */
    double score_pin_bayesian(const std::string &pin);
    double ensemble_score(const std::string &pin);

    /* Blocklist */
    void add_to_blocklist(const std::string &pin);
    void clear_blocklist();
    bool is_blocklisted(const std::string &pin) const;

    /* Custom keypad layout */
    void set_keypad_layout(const std::vector<std::vector<char>> &layout);
    std::vector<std::string> custom_keypad_patterns(int digits);

    /* Kneser-Ney smoothing */
    double kn_probability(const std::string &prefix, char next) const;
    double kneser_ney_prob(const std::string &context, char next);

    /* Scoring */
    double score_pin(const std::string &pin);
    double markov_probability(const std::string &pin);
    double common_penalty(const std::string &pin);
    double device_similarity(const std::string &pin);

    /* Config */
    void set_digits(int d) { m_digits = d; }
    void set_phone(const std::string &p) { m_phone = p; }
    void set_imei(const std::string &i) { m_imei = i; }
    void set_serial(const std::string &s) { m_serial = s; }
    void set_breed_data(const std::vector<std::string> &data) { m_breed_pins = data; }

    /* Stats */
    int attempted() const { return m_attempt_count; }
    double accuracy() const;

    static const char *COMMON_PINS[];
    static const int COMMON_COUNT;

private:
    int m_digits;
    int m_markov_order;
    std::string m_phone, m_imei, m_serial;
    std::vector<std::string> m_breed_pins;
    std::vector<std::string> m_attempt_history;
    std::vector<bool> m_attempt_results;
    int m_attempt_count;

    /* Markov chains */
    std::map<std::string, MarkovState> m_markov;
    std::mt19937 m_rng;

    /* Weights */
    double w_markov, w_common, w_device, w_breed, w_pattern;

    /* Kneser-Ney discount */
    double m_kn_discount;
    double m_continuation_count;
    std::map<std::string, int> m_context_counts;

    /* Blocklist */
    std::set<std::string> m_blocklist;

    /* Custom keypad layout (vector of rows, each row is a vector of chars) */
    std::vector<std::vector<char>> m_keypad_layout;
    bool m_has_custom_layout;

    void build_markov(const std::string &pin);
    std::vector<char> predict_next(const std::string &prefix);
    std::string generate_from_model();
    std::string generate_from_model_temperature(double temp);
    std::vector<std::string> common_keypad_patterns(int digits);
    std::vector<std::string> device_derived_pins(int digits);
};

#endif
