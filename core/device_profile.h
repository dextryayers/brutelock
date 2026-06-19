#ifndef DEVICE_PROFILE_H
#define DEVICE_PROFILE_H

#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

/*  Device profile — customizes brute approach per device */
typedef struct {
    char vendor[64];
    char model[128];
    char brand[64];
    char android_ver[16];
    int  sdk;
    int  screen_w;
    int  screen_h;
    char lock_type[32];
    int  pin_len;
    char serial[64];
    char imei1[32];
    char imei2[32];
    char phone[32];

    /* Derived */
    int  grid_preset;           /* 0=AOSP, 1=Samsung, 2=Xiaomi, etc */
    int  preferred_method;      /* Bitmask: KEYEVENT, TAP, TEXT */
    double recommended_delay_ms;/* Optimal delay for this device */
    int  max_before_lockout;    /* Attempts before lockout (5-30) */
    char notes[1024];
} DeviceProfile;

/*  Build profile from DeviceInfo */
void dprof_build(DeviceProfile *dp, const DeviceInfo *info);

/*  Auto-detect grid preset from brand/model */
int dprof_detect_grid(const char *vendor, const char *model);

/*  Get recommended PIN entry method for this device */
int dprof_recommend_method(DeviceProfile *dp);

/*  Get recommended delay based on device */
double dprof_recommend_delay(DeviceProfile *dp);

/*  Print profile summary */
void dprof_print(const DeviceProfile *dp);

/*  Export profile as JSON string */
void dprof_to_json(const DeviceProfile *dp, char *out, int out_sz);

#ifdef __cplusplus
}
#endif
#endif
