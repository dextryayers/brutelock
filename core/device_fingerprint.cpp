#include "device_fingerprint.h"
#include "adb.h"
#include "util.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <cctype>

extern "C" {

static std::string exec_adb(const char *cmd) {
    char *out = adb_shell(cmd);
    if (!out) return "";
    std::string s(out);
    while (!s.empty() && (s.back()=='\n' || s.back()=='\r')) s.pop_back();
    return s;
}

static std::string getprop(const char *prop) {
    char buf[512];
    snprintf(buf, sizeof(buf), "adb shell getprop %s 2>/dev/null", prop);
    return exec_adb(buf);
}

FingerprintData fp_analyze_device(void) {
    FingerprintData fp;

    fp.serial = getprop("ro.serialno");
    fp.model = getprop("ro.product.model");
    fp.brand = getprop("ro.product.brand");
    fp.android = getprop("ro.build.version.release");
    fp.sdk = getprop("ro.build.version.sdk");
    fp.build_fp = getprop("ro.build.fingerprint");
    fp.security_patch = getprop("ro.build.version.security_patch");
    fp.kernel = exec_adb("adb shell uname -r 2>/dev/null");
    fp.bootloader = getprop("ro.bootloader");
    fp.hardware = getprop("ro.hardware");
    fp.chipset = exec_adb("adb shell getprop ro.chipset.name 2>/dev/null || "
                          "adb shell getprop ro.board.platform 2>/dev/null");
    fp.cpu_cores = exec_adb("adb shell nproc 2>/dev/null || adb shell grep -c ^processor /proc/cpuinfo 2>/dev/null");
    fp.ram = exec_adb("adb shell free -m 2>/dev/null | grep Mem | awk '{print $2}' 2>/dev/null");
    fp.screen_size = exec_adb("adb shell wm size 2>/dev/null");
    fp.density = exec_adb("adb shell wm density 2>/dev/null");
    fp.battery = exec_adb("adb shell dumpsys battery 2>/dev/null | grep level | awk '{print $2}' 2>/dev/null");
    fp.lock_type = exec_adb("adb shell locksettings get-password-type 2>/dev/null || "
                            "adb shell settings get secure lockscreen.password_type 2>/dev/null");

    fp.imei1 = getprop("ro.ril.oem.imei1");
    if (fp.imei1.empty()) fp.imei1 = getprop("gsm.imei");
    fp.imei2 = getprop("ro.ril.oem.imei2");
    if (fp.imei2.empty()) fp.imei2 = getprop("gsm.imei2");
    fp.phone = getprop("ro.ril.mcc");
    if (fp.phone.empty()) fp.phone = getprop("gsm.phone.number");

    /* Detect biometrics */
    std::string fp_status = exec_adb("adb shell dumpsys fingerprint 2>/dev/null | head -5");
    fp.has_fp = !fp_status.empty() && fp_status.find("not")==std::string::npos;
    std::string face_status = exec_adb("adb shell dumpsys face 2>/dev/null | head -5");
    fp.has_face = !face_status.empty() && face_status.find("not")==std::string::npos;
    fp.has_biometric = fp.has_fp || fp.has_face;

    /* Device ID hash */
    std::string raw = fp.serial + fp.imei1 + fp.model + fp.brand;
    unsigned long h = 5381;
    for (char c : raw) h = ((h << 5) + h) + (unsigned char)c;
    char hbuf[32];
    snprintf(hbuf, sizeof(hbuf), "%08lx", h);
    fp.device_id_hash = hbuf;

    fp.probable_pin_length = fp_predict_pin_length(&fp);
    return fp;
}

int fp_predict_pin_length(const FingerprintData *fp) {
    if (!fp->lock_type.empty()) {
        int t = atoi(fp->lock_type.c_str());
        if (t == 65536) return 4;
        if (t == 131072 || t == 196608) return 6;
        if (t == 262144) return 8;
    }
    /* Guess from security patch recency */
    int yr = 0, mo = 0;
    if (!fp->security_patch.empty()) {
        sscanf(fp->security_patch.c_str(), "%d-%d", &yr, &mo);
    }
    if (yr >= 2023) return 6; /* modern devices tend to use 6-digit */
    if (yr >= 2020) return 4;
    return 4;
}

int fp_generate_pins(const FingerprintData *fp, char pins[][16], int max) {
    int count = 0;
    auto add = [&](const std::string &s) {
        if (count < max && !s.empty()) {
            std::string clean;
            for (char c : s) if (isdigit(c)) clean += c;
            if (clean.length() >= 4) {
                snprintf(pins[count], 16, "%s", clean.c_str());
                count++;
            }
        }
    };

    /* From IMEI (last 4-8 digits) */
    add(fp->imei1);
    add(fp->imei2);
    /* From phone */
    add(fp->phone);
    /* From serial (usually alphanumeric, extract digits) */
    add(fp->serial);
    /* From device ID hash */
    add(fp->device_id_hash);
    /* From build fingerprint numbers */
    add(fp->build_fp);

    return count;
}

void fp_print_info(const FingerprintData *fp) {
    printf("  " BOLD "%-20s" RESET ": %s\n", "Device ID Hash", fp->device_id_hash.c_str());
    printf("  " BOLD "%-20s" RESET ": %s\n", "Probable PIN Len", fp->probable_pin_length > 0 ?
           std::to_string(fp->probable_pin_length).c_str() : "Unknown");
    printf("  " BOLD "%-20s" RESET ": %s\n", "Has Fingerprint", fp->has_fp ? "Yes" : "No");
    printf("  " BOLD "%-20s" RESET ": %s\n", "Has Face Unlock", fp->has_face ? "Yes" : "No");
    printf("  " BOLD "%-20s" RESET ": %s\n", "Security Patch", fp->security_patch.c_str());
    printf("  " BOLD "%-20s" RESET ": %s\n", "Build FP", fp->build_fp.c_str());
    printf("  " BOLD "%-20s" RESET ": %s\n", "Bootloader", fp->bootloader.c_str());
    printf("  " BOLD "%-20s" RESET ": %s\n", "Lock Type", fp->lock_type.c_str());
}

}
