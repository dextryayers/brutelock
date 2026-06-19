#ifndef DEEP_SCAN_H
#define DEEP_SCAN_H

#include "util.h"
#include "fastboot.h"
#include "usb_raw.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Deep scan fills DeviceInfo from ALL available sources:
   - ADB getprop (if available)
   - Fastboot variables (if in bootloader)
   - USB device descriptors (always available)
   - /proc/cpuinfo fallback
   Returns 1 if any info found, 0 if nothing */
int deep_scan_all(DeviceInfo *info);

/* Scan without ADB - only USB descriptors + fastboot */
/* Returns 1 if USB device found, 0 if not */
int deep_scan_no_adb(DeviceInfo *info);

/* Fill DeviceInfo from a raw USB descriptor */
void deep_scan_from_usb(DeviceInfo *info, const UsbRawDevice *raw);

/* Fill DeviceInfo from fastboot variables */
void deep_scan_from_fastboot(DeviceInfo *info, const FastbootInfo *fb);

/* Detect SoC from USB descriptor (VID/PID) */
void deep_scan_soc_from_usb(DeviceInfo *info, int vid, int pid);

/* Get vendor name from VID (returns NULL if unknown) */
const char* vendor_by_vid(int vid);

#ifdef __cplusplus
}
#endif

#endif
