#include "device_profile.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

extern "C" {

void dprof_build(DeviceProfile *dp, const DeviceInfo *info) {
    memset(dp, 0, sizeof(*dp));
    if (!info) return;

    strncpy(dp->vendor, info->vendor, sizeof(dp->vendor)-1);
    strncpy(dp->model, info->model, sizeof(dp->model)-1);
    strncpy(dp->android_ver, info->android, sizeof(dp->android_ver)-1);
    dp->sdk = atoi(info->sdk);
    strncpy(dp->lock_type, info->lock_type, sizeof(dp->lock_type)-1);
    dp->pin_len = info->pin_len;
    strncpy(dp->serial, info->serial, sizeof(dp->serial)-1);
    strncpy(dp->imei1, info->imei1, sizeof(dp->imei1)-1);
    strncpy(dp->imei2, info->imei2_2, sizeof(dp->imei2)-1);
    strncpy(dp->phone, info->phone_number, sizeof(dp->phone)-1);

    /* Parse brand from vendor/model */
    char brand[128] = {0};
    strncpy(brand, info->vendor, sizeof(brand)-1);
    if (strlen(brand) < 2) strncpy(brand, info->model, sizeof(brand)-1);
    for (char *p = brand; *p; p++) *p = tolower(*p);
    if (strstr(brand, "samsung")) strncpy(dp->brand, "Samsung", sizeof(dp->brand)-1);
    else if (strstr(brand, "xiaomi")) strncpy(dp->brand, "Xiaomi", sizeof(dp->brand)-1);
    else if (strstr(brand, "redmi")) strncpy(dp->brand, "Redmi", sizeof(dp->brand)-1);
    else if (strstr(brand, "poco")) strncpy(dp->brand, "POCO", sizeof(dp->brand)-1);
    else if (strstr(brand, "oppo")) strncpy(dp->brand, "OPPO", sizeof(dp->brand)-1);
    else if (strstr(brand, "oneplus")) strncpy(dp->brand, "OnePlus", sizeof(dp->brand)-1);
    else if (strstr(brand, "realme")) strncpy(dp->brand, "Realme", sizeof(dp->brand)-1);
    else if (strstr(brand, "vivo")) strncpy(dp->brand, "Vivo", sizeof(dp->brand)-1);
    else if (strstr(brand, "iqoo")) strncpy(dp->brand, "iQOO", sizeof(dp->brand)-1);
    else if (strstr(brand, "huawei")) strncpy(dp->brand, "Huawei", sizeof(dp->brand)-1);
    else if (strstr(brand, "honor")) strncpy(dp->brand, "Honor", sizeof(dp->brand)-1);
    else if (strstr(brand, "motorola") || strstr(brand, "moto")) strncpy(dp->brand, "Motorola", sizeof(dp->brand)-1);
    else if (strstr(brand, "sony") || strstr(brand, "xperia")) strncpy(dp->brand, "Sony", sizeof(dp->brand)-1);
    else if (strstr(brand, "nokia")) strncpy(dp->brand, "Nokia", sizeof(dp->brand)-1);
    else if (strstr(brand, "asus") || strstr(brand, "zenfone") || strstr(brand, "rog")) strncpy(dp->brand, "ASUS", sizeof(dp->brand)-1);
    else if (strstr(brand, "google") || strstr(brand, "pixel")) strncpy(dp->brand, "Google", sizeof(dp->brand)-1);
    else if (strstr(brand, "tecno")) strncpy(dp->brand, "Tecno", sizeof(dp->brand)-1);
    else if (strstr(brand, "infinix")) strncpy(dp->brand, "Infinix", sizeof(dp->brand)-1);
    else if (strstr(brand, "advan")) strncpy(dp->brand, "Advan", sizeof(dp->brand)-1);
    else if (strstr(brand, "lg")) strncpy(dp->brand, "LG", sizeof(dp->brand)-1);
    else if (strstr(brand, "lenovo")) strncpy(dp->brand, "Lenovo", sizeof(dp->brand)-1);
    else if (strstr(brand, "htc")) strncpy(dp->brand, "HTC", sizeof(dp->brand)-1);
    else strncpy(dp->brand, info->vendor[0] ? info->vendor : "Unknown", sizeof(dp->brand)-1);

    /* Parse screen size */
    if (strlen(info->screen_size) > 0) {
        const char *x = strchr(info->screen_size, 'x');
        if (x) {
            char tmp[128];
            strncpy(tmp, info->screen_size, sizeof(tmp)-1);
            char *xp = strchr(tmp, 'x');
            if (xp) {
                *xp = 0;
                dp->screen_w = atoi(tmp);
                dp->screen_h = atoi(xp + 1);
            }
        }
    }
    if (dp->screen_w <= 0) dp->screen_w = 1080;
    if (dp->screen_h <= 0) dp->screen_h = 2400;

    /* Detect grid preset */
    dp->grid_preset = dprof_detect_grid(info->vendor, info->model);

    /* Recommend method */
    dp->preferred_method = dprof_recommend_method(dp);

    /* Recommended delay based on Android version and device age */
    dp->recommended_delay_ms = dprof_recommend_delay(dp);

    /* Max attempts before lockout (varies by Android version & device) */
    if (dp->sdk >= 30) dp->max_before_lockout = 5;  /* Android 11+ */
    else if (dp->sdk >= 28) dp->max_before_lockout = 10;
    else dp->max_before_lockout = 15;

    snprintf(dp->notes, sizeof(dp->notes),
             "%s %s | %s %d | %dx%d | %s preset | %.0fms | lockout=%d",
             dp->brand, dp->model, dp->android_ver, dp->sdk,
             dp->screen_w, dp->screen_h,
             dp->grid_preset == 0 ? "AOSP" :
             dp->grid_preset == 1 ? "Samsung" :
             dp->grid_preset == 2 ? "Xiaomi" :
             dp->grid_preset == 3 ? "OPPO" :
             dp->grid_preset == 4 ? "Vivo" :
             dp->grid_preset == 5 ? "Huawei" :
             dp->grid_preset == 6 ? "Motorola" :
             dp->grid_preset == 7 ? "Sony" :
             dp->grid_preset == 8 ? "Nokia" :
             dp->grid_preset == 9 ? "ASUS" :
             dp->grid_preset == 10 ? "Tecno" :
             dp->grid_preset == 11 ? "Infinix" :
             dp->grid_preset == 12 ? "Advan" :
             dp->grid_preset == 13 ? "Honor" : "Auto",
             dp->recommended_delay_ms, dp->max_before_lockout);
}

int dprof_detect_grid(const char *vendor, const char *model) {
    if (!vendor && !model) return 0;
    char buf[512]; buf[0] = 0;
    if (vendor) { strncat(buf, vendor, sizeof(buf)-1); }
    if (model) { strncat(buf, " ", sizeof(buf)-1); strncat(buf, model, sizeof(buf)-1); }
    for (char *p = buf; *p; p++) *p = tolower(*p);

    if (strstr(buf, "samsung")) return 1;
    if (strstr(buf, "xiaomi") || strstr(buf, "redmi") || strstr(buf, "poco") || strstr(buf, "mi ")) return 2;
    if (strstr(buf, "oppo") || strstr(buf, "oneplus") || strstr(buf, "realme")) return 3;
    if (strstr(buf, "vivo") || strstr(buf, "iqoo")) return 4;
    if (strstr(buf, "huawei")) return 5;
    if (strstr(buf, "motorola") || strstr(buf, "moto ")) return 6;
    if (strstr(buf, "sony") || strstr(buf, "xperia")) return 7;
    if (strstr(buf, "nokia")) return 8;
    if (strstr(buf, "asus") || strstr(buf, "zenfone") || strstr(buf, "rog ")) return 9;
    if (strstr(buf, "tecno") || strstr(buf, "camon")) return 10;
    if (strstr(buf, "infinix") || strstr(buf, "zero ") || strstr(buf, "note ")) return 11;
    if (strstr(buf, "advan") || strstr(buf, "i5") || strstr(buf, "s5e")) return 12;
    if (strstr(buf, "honor")) return 13;
    return 0;
}

int dprof_recommend_method(DeviceProfile *dp) {
    int method = 0;
    switch (dp->grid_preset) {
        case 1: /* Samsung */
            method = (1<<1) | (1<<0) | (1<<2);
            break;
        case 2: /* Xiaomi */
            method = (1<<0) | (1<<1) | (1<<2);
            break;
        case 3: /* OPPO */
            method = (1<<0) | (1<<2);
            break;
        case 4: /* Vivo */
            method = (1<<0) | (1<<1) | (1<<2);
            break;
        case 5: /* Huawei */
            method = (1<<0) | (1<<2);
            break;
        case 6: /* Motorola */
            method = (1<<0) | (1<<2);
            break;
        case 7: /* Sony */
            method = (1<<0) | (1<<2);
            break;
        case 8: /* Nokia */
            method = (1<<0) | (1<<2);
            break;
        case 9: /* ASUS */
            method = (1<<0) | (1<<1) | (1<<2);
            break;
        case 10: /* Tecno */
            method = (1<<0) | (1<<2);
            break;
        case 11: /* Infinix */
            method = (1<<0) | (1<<2);
            break;
        case 12: /* Advan */
            method = (1<<0) | (1<<2);
            break;
        case 13: /* Honor */
            method = (1<<0) | (1<<2);
            break;
        default:
            method = (1<<0) | (1<<2);
            break;
    }
    return method;
}

double dprof_recommend_delay(DeviceProfile *dp) {
    /* Newer devices process faster but lockout quicker */
    if (dp->sdk >= 33) return 30.0;   /* Android 13+ — be careful */
    if (dp->sdk >= 30) return 20.0;   /* Android 11-12 */
    if (dp->sdk >= 28) return 10.0;   /* Android 9-10 */
    return 5.0;                         /* Older — can go fast */
}

void dprof_print(const DeviceProfile *dp) {
    if (!dp) return;
    printf("\n" BOLD "=== Device Profile ===" RESET "\n");
    printf("  Brand          : %s\n", dp->brand);
    printf("  Model          : %s %s\n", dp->vendor, dp->model);
    printf("  Android        : %s (SDK %d)\n", dp->android_ver, dp->sdk);
    printf("  Screen         : %dx%d\n", dp->screen_w, dp->screen_h);
    printf("  Grid Preset    : %s\n",
           dp->grid_preset == 0 ? "AOSP" :
           dp->grid_preset == 1 ? "Samsung" :
           dp->grid_preset == 2 ? "Xiaomi" :
           dp->grid_preset == 3 ? "OPPO" :
           dp->grid_preset == 4 ? "Vivo" :
           dp->grid_preset == 5 ? "Huawei" :
           dp->grid_preset == 6 ? "Motorola" :
           dp->grid_preset == 7 ? "Sony" :
           dp->grid_preset == 8 ? "Nokia" :
           dp->grid_preset == 9 ? "ASUS" :
           dp->grid_preset == 10 ? "Tecno" :
           dp->grid_preset == 11 ? "Infinix" :
           dp->grid_preset == 12 ? "Advan" :
           dp->grid_preset == 13 ? "Honor" : "Auto");
    printf("  Lock Type      : %s\n", dp->lock_type);
    printf("  PIN Length     : %d\n", dp->pin_len);
    printf("  Input Method   : %s%s%s\n",
           (dp->preferred_method & (1<<0)) ? "KEYEVENT " : "",
           (dp->preferred_method & (1<<1)) ? "TAP " : "",
           (dp->preferred_method & (1<<2)) ? "TEXT" : "");
    printf("  Delay          : %.0fms\n", dp->recommended_delay_ms);
    printf("  Lockout After  : %d attempts\n", dp->max_before_lockout);
    printf("  %s\n", dp->notes);
}

void dprof_to_json(const DeviceProfile *dp, char *out, int out_sz) {
    snprintf(out, out_sz,
             "{\"brand\":\"%s\",\"model\":\"%s %s\",\"android\":\"%s\",\"sdk\":%d,"
             "\"screen\":\"%dx%d\",\"grid\":%d,\"lock_type\":\"%s\",\"pin_len\":%d,"
             "\"method\":%d,\"delay\":%.0f,\"lockout_max\":%d}",
             dp->brand, dp->vendor, dp->model,
             dp->android_ver, dp->sdk,
             dp->screen_w, dp->screen_h, dp->grid_preset,
             dp->lock_type, dp->pin_len,
             dp->preferred_method, dp->recommended_delay_ms,
             dp->max_before_lockout);
}

} // extern "C"
