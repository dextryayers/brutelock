#ifndef DETECT_H
#define DETECT_H

#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MODE_NONE = 0,
    MODE_ADB,         /* Android ADB interface (FF/42/01) */
    MODE_FASTBOOT,    /* Fastboot interface (FF/42/03) */
    MODE_RECOVERY,    /* Recovery ADB (sideload present) */
    MODE_SIDELOAD,    /* Sideload only (FF/42/02) */
    MODE_MTP,         /* MTP interface only */
    MODE_RNDIS,       /* USB tethering */
    MODE_CHARGING,    /* No ADB/Fastboot, just charging */
    MODE_HID,         /* HID device interface */
    MODE_UNKNOWN
} DeviceMode;

/* Device mode info struct */
typedef struct {
    DeviceMode mode;
    char       mode_str[32];
    char       serial[128];
    char       vendor_name[64];
    int        vid, pid;
    int        bus, dev_num;
    int        adb_available;
    int        fastboot_available;
    int        recovery_available;
} ModeInfo;

ChipsetFamily detect_chipset(DeviceInfo *info);
void detect_device(DeviceInfo *info);
int  verify_device(const DeviceInfo *info);
int  choose_pin_len(const DeviceInfo *info);

/* Multi-mode detection */
int detect_all_modes(ModeInfo *modes, int max_modes);
const char* device_mode_str(DeviceMode m);

/* Fastboot-specific detect */
int detect_fastboot_mode(ModeInfo *info);

/* Check if any device is available (any mode) */
int detect_any_device(void);

/* Print device mode info */
void print_mode_info(const ModeInfo *info);

/* Full info display (defined in main.c) */
void print_full_info(const DeviceInfo *info);

#ifdef __cplusplus
}
#endif

#endif
