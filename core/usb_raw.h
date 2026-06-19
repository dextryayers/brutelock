#ifndef USB_RAW_H
#define USB_RAW_H

#include "platform.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum number of interfaces per device */
#define USBRAW_MAX_IFACES  16
#define USBRAW_MAX_EPS     8
#define USBRAW_MAX_DEVICES 16

/* Endpoint descriptor */
typedef struct {
    uint8_t  bEndpointAddress;
    uint8_t  bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t  bInterval;
} UsbRawEndpoint;

/* Interface descriptor */
typedef struct {
    uint8_t  bInterfaceNumber;
    uint8_t  bAlternateSetting;
    uint8_t  bNumEndpoints;
    uint8_t  bInterfaceClass;
    uint8_t  bInterfaceSubClass;
    uint8_t  bInterfaceProtocol;
    char     iInterface[64];          /* interface string if available */
    int      ep_count;
    UsbRawEndpoint  endpoints[USBRAW_MAX_EPS];
} UsbRawInterface;

/* Device descriptor + full config */
typedef struct {
    int      present;
    UsbDevice dev;                     /* open device handle (or -1 fd) */
    
    /* Device descriptor fields */
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint8_t  bNumConfigurations;
    char     manufacturer[128];
    char     product_str[128];
    char     serial[128];
    int      bus, dev_num;           /* Linux bus/device numbers */
    
    /* Interfaces */
    int      iface_count;
    UsbRawInterface ifaces[USBRAW_MAX_IFACES];
    
    /* Flags for known Android interfaces */
    int      has_adb;                 /* 0xFF/0x42/0x01 */
    int      has_fastboot;            /* 0xFF/0x42/0x03 */
    int      has_sideload;            /* 0xFF/0x42/0x02 */
    int      has_mtp;                 /* 0xFF/0xFF/0xFF usually */
    int      has_rndis;               /* 0x02/0x02/0x01 or 0xEF/0x04/0x01 */
    int      has_hid;                 /* 0x03 HID */
    int      has_audio;               /* 0x01 AUDIO */
    int      has_video;               /* 0x0E VIDEO */
    int      has_charging;            /* only config 0 with no interfaces */
    
    /* Device mode classification */
    char     mode[32];                /* "FASTBOOT", "ADB", "RECOVERY", "MTP", "CHARGING", "SIDELOAD", "UNKNOWN" */
} UsbRawDevice;

/* Scan all USB devices, return Android devices found (populate array) */
int usb_raw_scan_all(UsbRawDevice *devices, int max_devices);

/* Scan a single device by opening /dev/bus/usb/BBB/DDD */
int usb_raw_open_device(int bus, int devnum, UsbRawDevice *info);

/* Open device from an already-found UsbDevice */
int usb_raw_open_from(UsbDevice *dev, UsbRawDevice *info);

/* Close and free device */
void usb_raw_close(UsbRawDevice *info);

/* Print all interface info to stdout */
void usb_raw_print_info(const UsbRawDevice *info);

/* Classify device mode based on available interfaces */
const char* usb_raw_classify_mode(const UsbRawDevice *info);

/* Read string descriptor (index) */
int usb_raw_read_string(UsbDevice *dev, uint8_t index, char *buf, int maxlen);

#ifdef __cplusplus
}
#endif

#endif
