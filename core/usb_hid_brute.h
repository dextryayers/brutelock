#ifndef USB_HID_BRUTE_H
#define USB_HID_BRUTE_H

#include "util.h"

/* USB HID keyboard usage codes for digits 0-9 */
#define HID_KEY_0     0x27
#define HID_KEY_1     0x1E
#define HID_KEY_2     0x1F
#define HID_KEY_3     0x20
#define HID_KEY_4     0x21
#define HID_KEY_5     0x22
#define HID_KEY_6     0x23
#define HID_KEY_7     0x24
#define HID_KEY_8     0x25
#define HID_KEY_9     0x26
#define HID_KEY_ENTER 0x28

typedef struct {
    int bus;
    int dev;
    int fd;
    int ep_out;
    int timeout;
    int aoa_supported;
    int aoa_version;
    int hid_ready;
    char last_error[256];
    int debug;
} UsbHidBrute;

/* Initialize HID brute engine */
int hid_init(UsbHidBrute *hid, int bus, int dev);

/* Try to switch device to AOA (Android Open Accessory) mode */
int hid_aoa_handshake(UsbHidBrute *hid);

/* Register HID keyboard via AOA protocol */
int hid_aoa_register_hid(UsbHidBrute *hid);

/* Send a single keyboard report via AOA */
int hid_aoa_send_report(UsbHidBrute *hid, unsigned char keycode);

/* Send PIN as keyboard presses via AOA */
int hid_aoa_send_pin(UsbHidBrute *hid, const char *pin);

/* Send ENTER key */
int hid_aoa_send_enter(UsbHidBrute *hid);

/* Direct USB HID report (alternative to AOA) */
int hid_send_raw_report(UsbHidBrute *hid, const unsigned char *report, int len);

/* Full PIN attempt via HID */
int hid_try_pin(UsbHidBrute *hid, const char *pin);

/* Clean up */
void hid_close(UsbHidBrute *hid);

/* Utilities */
unsigned char hid_digit_to_keycode(char digit);
const char* hid_last_error(UsbHidBrute *hid);

#endif
