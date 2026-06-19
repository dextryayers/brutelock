#ifndef AUTO_PWN_CHAIN_H
#define AUTO_PWN_CHAIN_H

#include <string>
#include <vector>
#include "kernel_exploit_orchestrator.h"
#include "hw_control_expert.h"
#include "pin_entry_expert.h"
#include "hardware_scanner_deep.h"
#include "soc_exploit_engine.h"
#include "trustzone_pwn.h"
#include "adb_native.h"

typedef enum {
    PWN_PHASE_SCAN,
    PWN_PHASE_KERNEL,
    PWN_PHASE_HARDWARE,
    PWN_PHASE_BYPASS,
    PWN_PHASE_BRUTE,
    PWN_PHASE_CLEANUP,
    PWN_PHASE_MAX
} PwnPhase;

typedef struct {
    PwnPhase phase;
    std::string phase_name;
    int success;
    int duration_ms;
    std::string details;
} PwnStepResult;

typedef struct {
    int pin_length;
    std::string pin_type;
    int use_touch;
    int use_sendevent;
    int use_keyevent;
    int use_gpio;
    int inter_digit_us;
    int grid_x;
    int grid_y;
    int key_w;
    int key_h;
    int screen_w;
    int screen_h;
    int batch_size;
    int max_attempts;
} BruteConfig;

class AutoPwnChain {
public:
    AutoPwnChain();
    ~AutoPwnChain();

    int run_full_chain();
    int run_phase(PwnPhase phase);
    int run_scan_phase();
    int run_kernel_phase();
    int run_hardware_phase();
    int run_bypass_phase();
    int run_brute_phase();
    int run_cleanup_phase();

    int select_pin_method(BruteConfig *config);
    int scan_then_brute();
    int auto_configure();
    int is_device_ready();

    int get_phase_count() const { return (int)results_.size(); }
    int get_success_count() const;
    PwnStepResult get_result(int idx) const { return results_[idx]; }

    void print_chain_summary() const;
    void print_brute_config(const BruteConfig *config) const;

    KernelExploitOrchestrator *get_kernel() { return &kernel_; }
    HWControlExpert *get_hw() { return &hw_; }
    PinEntryExpert *get_pin_entry() { return &pin_; }
    HardwareScannerDeep *get_scanner() { return &scanner_; }
    SocExploitEngine *get_soc() { return &soc_; }
    TrustZonePwn *get_tz() { return &tz_; }

private:
    KernelExploitOrchestrator kernel_;
    HWControlExpert hw_;
    PinEntryExpert pin_;
    HardwareScannerDeep scanner_;
    SocExploitEngine soc_;
    TrustZonePwn tz_;
    BruteConfig config_;
    DeepHWInfo hw_info_;
    std::vector<PwnStepResult> results_;

    void add_result(PwnPhase phase, const std::string &name, int success,
                    int duration_ms, const std::string &details);
    int select_pin_length();
    int select_pin_grid();
    int select_input_method();
    int try_user_selection();
    int run_brute_loop(const std::vector<std::string> &pins, BruteConfig *config);
    int generate_pin_list(std::vector<std::string> &pins, BruteConfig *config);
};

#endif
