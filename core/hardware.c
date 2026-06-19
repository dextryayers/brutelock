#include "hardware.h"
#include "adb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

void detect_hardware(DeviceInfo *info) {
    char *tmp;

    tmp = adb_shell("adb shell dumpsys display 2>/dev/null | head -80 | grep -A3 'mDisplayInfo'");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        if (strstr(tmp, "refreshRate")) {
            char *p = strstr(tmp, "refreshRate");
            char *q = strchr(p, '=');
            if (q) { q++; while (*q && *q == ' ') q++; char *r = strchr(q, ','); if (r) *r = 0; strncpy(info->display_hz, q, sizeof(info->display_hz)-1); }
        }
        if (strstr(tmp, "HDR")) strncpy(info->display_hdr, "HDR Supported", sizeof(info->display_hdr)-1);
    }

    if (strlen(info->display_hz) == 0) {
        tmp = adb_shell("adb shell dumpsys display 2>/dev/null | grep -iE 'fps|refresh|hz' | head -3");
        trim_newline(tmp);
        if (strlen(tmp) > 0) {
            char *p = strstr(tmp, "fps");
            if (p) { p -= 4; while (*p && *p != '=' && p > tmp) p--; if (*p == '=') { p++; while (*p == ' ') p++; char *q = strchr(p, ','); if (q) *q = 0; strncpy(info->display_hz, p, sizeof(info->display_hz)-1); } }
        }
    }

    tmp = adb_shell("adb shell dumpsys gfxinfo 2>/dev/null | grep -E 'GLES|OpenGL' | head -3");
    trim_newline(tmp);
    if (strlen(tmp) == 0) tmp = adb_shell("adb shell dumpsys 2>/dev/null | grep 'GLES:' | head -1");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        char *p = strstr(tmp, "GLES:");
        if (p) strncpy(info->gpu_renderer, p+5, sizeof(info->gpu_renderer)-1);
        else strncpy(info->gpu_renderer, tmp, sizeof(info->gpu_renderer)-1);
    }

    if (strlen(info->gpu) == 0) {
        tmp = adb_shell("adb shell getprop ro.hardware.egl 2>/dev/null");
        trim_newline(tmp);
        if (strlen(tmp) > 0) strncpy(info->gpu, tmp, sizeof(info->gpu)-1);
    }

    if (strlen(info->gpu) == 0) {
        tmp = adb_shell("adb shell getprop ro.vendor.gpu 2>/dev/null");
        trim_newline(tmp);
        if (strlen(tmp) > 0) strncpy(info->gpu, tmp, sizeof(info->gpu)-1);
    }

    if (strlen(info->gpu) == 0) {
        tmp = adb_shell("adb shell cat /proc/gpu 2>/dev/null | head -1");
        trim_newline(tmp);
        if (strlen(tmp) > 0) strncpy(info->gpu, tmp, sizeof(info->gpu)-1);
    }

    if (strlen(info->gpu) == 0) {
        tmp = adb_shell("adb shell dumpsys 2>/dev/null | grep -i 'gpu' | head -1");
        trim_newline(tmp);
        if (strlen(tmp) > 0) strncpy(info->gpu, tmp, sizeof(info->gpu)-1);
    }

    if (strlen(info->gpu) == 0) {
        tmp = adb_shell("adb shell getprop | grep -i 'gpu\\|gl\\|egl\\|renderer' 2>/dev/null | head -3");
        trim_newline(tmp);
        if (strlen(tmp) > 0) {
            char *p = strchr(tmp, ':'); if (p) { p++; while (*p == ' ' || *p == '[') p++;
                char *end = strchr(p, ']'); if (end) *end = 0;
                strncpy(info->gpu, p, sizeof(info->gpu)-1); }
            else strncpy(info->gpu, tmp, sizeof(info->gpu)-1);
        }
    }

    if (strlen(info->gpu) == 0 && strlen(info->chipset_name) > 0) {
        const char *chip_gpu[] = {
            "mt6765", "PowerVR GE8320", "mt6768", "Mali-G52 MC2",
            "mt6785", "Mali-G76 MC4", "mt6877", "Mali-G68 MC4",
            "mt6891", "Mali-G77 MC9", "mt6983", "Mali-G710 MC10",
            "mt8675", "Mali-G610", "sm8350", "Adreno 660",
            "sm8450", "Adreno 730", "sm8550", "Adreno 740",
            "sm8650", "Adreno 750", "sm6375", "Adreno 619",
            "sm7325", "Adreno 642L", "sm7250", "Adreno 620",
            "sm6150", "Adreno 618", "sm6125", "Adreno 610",
            "sdm845", "Adreno 630", "sdm855", "Adreno 640",
            "sdm865", "Adreno 650", "sdm888", "Adreno 660",
            "exynos1380", "Mali-G68", "exynos2200", "RDNA2 Xclipse 920",
            "exynos2100", "Mali-G78 MP14", "exynos990", "Mali-G77 MP11",
            "exynos9820", "Mali-G76 MP12", "kirin9000", "Mali-G78 MP24",
            "kirin990", "Mali-G76 MP16", "kirin810", "Mali-G52 MP6",
            "tensor", "Mali-G78 MP20", "t310", "PowerVR GE8300",
            NULL
        };
        char chip_low[128];
        strncpy(chip_low, info->chipset_name, sizeof(chip_low)-1);
        for (int i = 0; chip_low[i]; i++) chip_low[i] = tolower((unsigned char)chip_low[i]);
        for (int i = 0; chip_gpu[i]; i += 2) {
            if (strstr(chip_low, chip_gpu[i])) {
                snprintf(info->gpu, sizeof(info->gpu), "%s (inferred)", chip_gpu[i+1]);
                break;
            }
        }
    }

    if (strlen(info->gpu_renderer) == 0 && strlen(info->gpu) > 0) {
        snprintf(info->gpu_renderer, sizeof(info->gpu_renderer)-1, "%s (sysfs)", info->gpu);
    }

    if (strlen(info->gpu_version) == 0) {
        tmp = adb_shell("adb shell dumpsys gfxinfo 2>/dev/null | grep -iE 'version|driver' | head -3");
        trim_newline(tmp);
        if (strlen(tmp) > 0) strncpy(info->gpu_version, tmp, sizeof(info->gpu_version)-1);
        else {
            tmp = adb_shell("adb shell getprop ro.gpu.version 2>/dev/null");
            trim_newline(tmp);
            if (strlen(tmp) > 0) snprintf(info->gpu_version, sizeof(info->gpu_version), "v%s", tmp);
            else {
                tmp = adb_shell("adb shell getprop ro.vendor.gpu.version 2>/dev/null");
                trim_newline(tmp);
                if (strlen(tmp) > 0) snprintf(info->gpu_version, sizeof(info->gpu_version), "v%s", tmp);
            }
        }
    }

    if (strlen(info->audio_codecs) == 0) {
        tmp = adb_shell("adb shell dumpsys media.audio_policy 2>/dev/null | grep -iE 'codec|aac|aptx|ldac|sbc' | head -5");
        trim_newline(tmp);
        if (strlen(tmp) > 0) strncpy(info->audio_codecs, tmp, sizeof(info->audio_codecs)-1);
        else {
            tmp = adb_shell("adb shell getprop ro.vendor.audio.codec 2>/dev/null");
            trim_newline(tmp);
            if (strlen(tmp) > 0) snprintf(info->audio_codecs, sizeof(info->audio_codecs), "Codec: %s", tmp);
        }
    }

    tmp = adb_shell("adb shell dumpsys fingerprint 2>/dev/null | head -5");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && !strstr(tmp, "not found") && !strstr(tmp, "not supported")) {
        strncpy(info->fingerprint, "Present", sizeof(info->fingerprint)-1);
    }
    if (strlen(info->fingerprint) == 0) {
        tmp = adb_shell("adb shell service call fingerprint 1 2>/dev/null | head -3");
        trim_newline(tmp);
        if (strlen(tmp) > 0 && !strstr(tmp, "not found") && strlen(tmp) > 10)
            strncpy(info->fingerprint, "Present (service)", sizeof(info->fingerprint)-1);
    }

    tmp = adb_shell("adb shell dumpsys face 2>/dev/null | head -5");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && !strstr(tmp, "not found") && !strstr(tmp, "not supported"))
        strncpy(info->face_unlock, "Present", sizeof(info->face_unlock)-1);

    tmp = adb_shell("adb shell dumpsys camera 2>/dev/null | grep -c 'Camera' | head -1");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && atoi(tmp) > 0 && strlen(info->camera_info) == 0)
        snprintf(info->camera_info, sizeof(info->camera_info), "%s camera(s)", tmp);

    tmp = adb_shell("adb shell dumpsys sensorservice 2>/dev/null | grep -c 'Sensor:' | head -1");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && atoi(tmp) > 0 && strlen(info->sensors) == 0)
        snprintf(info->sensors, sizeof(info->sensors), "%s sensor(s)", tmp);

    tmp = adb_shell("adb shell dumpsys media.audio_policy 2>/dev/null | grep -c 'codec' | head -1");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->audio_codec) == 0)
        snprintf(info->audio_codec, sizeof(info->audio_codec), "%s codec(s)", tmp);

    tmp = adb_shell("adb shell dumpsys audio 2>/dev/null | grep -i 'jack' | head -3");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strstr(tmp, "plugged") && strlen(info->audio_feat) == 0)
        strncpy(info->audio_feat, "3.5mm jack", sizeof(info->audio_feat)-1);

    char bio[256] = {0};
    if (strlen(info->fingerprint)) append_str(bio, sizeof(bio), "fingerprint ");
    if (strlen(info->face_unlock)) append_str(bio, sizeof(bio), "face ");
    tmp = adb_shell("adb shell dumpsys iris 2>/dev/null | head -3");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && !strstr(tmp, "not found")) append_str(bio, sizeof(bio), "iris ");
    if (strlen(bio)) strncpy(info->biometrics, bio, sizeof(info->biometrics)-1);
}

void print_hardware_info(const DeviceInfo *info) {
    if (strlen(info->gpu_renderer)) printf("  " BOLD "%-17s" RESET ": %s\n", "GPU Renderer", info->gpu_renderer);
    if (strlen(info->gpu)) printf("  " BOLD "%-17s" RESET ": %s\n", "GPU HW", info->gpu);
    if (strlen(info->gpu_version)) printf("  " BOLD "%-17s" RESET ": %s\n", "GPU Version", info->gpu_version);
    if (strlen(info->display_hz)) printf("  " BOLD "%-17s" RESET ": %s Hz\n", "Refresh Rate", info->display_hz);
    if (strlen(info->display_hdr)) printf("  " BOLD "%-17s" RESET ": %s\n", "HDR", info->display_hdr);
    if (strlen(info->biometrics)) printf("  " BOLD "%-17s" RESET ": %s\n", "Biometrics", info->biometrics);
    if (strlen(info->camera_info)) printf("  " BOLD "%-17s" RESET ": %s\n", "Camera", info->camera_info);
    if (strlen(info->sensors)) printf("  " BOLD "%-17s" RESET ": %s\n", "Sensors", info->sensors);
    if (strlen(info->audio_codecs)) printf("  " BOLD "%-17s" RESET ": %s\n", "Audio Codecs", info->audio_codecs);
}
