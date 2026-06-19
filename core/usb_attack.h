#ifndef USB_ATTACK_H
#define USB_ATTACK_H

#ifdef __cplusplus
extern "C" {
#endif

/* Multi-channel: spawn multiple ADB connections for parallel PIN attempts */
int usb_multi_attack(int digits, int channels, int delay_ms);

/* USB timing: measure precise timing of ADB responses to infer lockout state */
int usb_detect_lockout_timing(void);

/* Rapid fire: send PINs with minimal delay, detect lockout automatically */
int usb_rapid_fire(const char *start_pin, const char *end_pin);

/* Parallel PIN try on multiple serials at once */
int usb_multi_device_brute(const char *serials[], int nserials, int digits, int delay_ms);

/* Enum all ADB devices (including unauthorized) via raw USB scan */
int usb_enum_all_devices(char out[][64], int max);

#ifdef __cplusplus
}
#endif

#endif
