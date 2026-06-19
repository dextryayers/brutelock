#ifndef USB_DIRECT_INPUT_H
#define USB_DIRECT_INPUT_H

#include "util.h"

#define UDI_MAX_DEVICES 32
#define UDI_REPORT_SZ 8

/* AOA protocol constants */
#define AOA_VID 0x18D1
#define AOA_PID_ACC 0x2D00
#define AOA_PID_ACC_ADB 0x2D01
#define AOA_PID_AUDIO 0x2D02
#define AOA_PID_AUDIO_ADB 0x2D03
#define AOA_PID_MISC 0x2D04
#define AOA_PID_MISC_ADB 0x2D05

/* AOA v2 control request codes */
#define AOA_REQ_GET_PROTOCOL   51
#define AOA_REQ_SEND_STRING    52
#define AOA_REQ_START          53
#define AOA_REQ_REGISTER_HID   54
#define AOA_REQ_UNREGISTER_HID 55
#define AOA_REQ_SET_HID_DESC   56
#define AOA_REQ_SEND_HID_EVENT 57

/* HID keycodes for digits */
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
#define HID_KEY_HOME  0x4A

typedef struct {
    int bus;
    int dev;
    int fd;
    int aoa_version;
    int aoa_ready;
    int hid_registered;
    int touch_registered;
    int screen_w;
    int screen_h;
    char last_error[512];
    int debug;
} UsbDirectInput;

/* Initialize: open USB device */
int udi_init(UsbDirectInput *udi, int bus, int dev);
void udi_close(UsbDirectInput *udi);

/* AOA handshake — switch device to accessory mode */
int udi_aoa_handshake(UsbDirectInput *udi);

/* Re-scan USB for accessory device after handshake */
int udi_find_accessory(UsbDirectInput *udi);

/* Register HID keyboard */
int udi_register_hid_keyboard(UsbDirectInput *udi);

/* Send a single key press+release */
int udi_send_key(UsbDirectInput *udi, unsigned char keycode);
int udi_send_enter(UsbDirectInput *udi);
int udi_send_home(UsbDirectInput *udi);

/* Send full PIN via HID keyboard */
int udi_send_pin(UsbDirectInput *udi, const char *pin);

/* Send ENTER, then check result */
int udi_try_pin(UsbDirectInput *udi, const char *pin);

/* Touch input via AOA (register as digitizer) */
int udi_register_touch(UsbDirectInput *udi);
int udi_send_touch(UsbDirectInput *udi, int x, int y, int pressure);

/* Swipe gesture */
int udi_send_swipe(UsbDirectInput *udi, int x1, int y1, int x2, int y2, int steps);

/* Wake screen via HID (power button is not in standard HID keyboard) */
int udi_wake_screen(UsbDirectInput *udi);

/* Send HOME + recent apps to navigate */
int udi_go_home(UsbDirectInput *udi);

/* Unlock check — try to detect if device is unlocked without ADB */
int udi_check_unlocked(UsbDirectInput *udi);

/* Full brute sequence */
int udi_brute_sequence(UsbDirectInput *udi, const char *pin, int *unlocked);

/* Utility */
const char* udi_last_error(UsbDirectInput *udi);
void udi_print_status(UsbDirectInput *udi);

#endif
