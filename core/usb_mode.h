#ifndef USB_MODE_H
#define USB_MODE_H

#include "platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* USB mode switching - try to switch device to useful mode via control transfers */
/* Returns 1 if mode changed, 0 if not */

/* Try all known Android USB mode switching methods */
int usb_mode_try_switch(UsbDevice *dev);

/* Send Android-specific USB control request to switch to MTP */
int usb_mode_set_mtp(UsbDevice *dev);

/* Send control request to switch to ADB */
int usb_mode_set_adb(UsbDevice *dev);

/* Send control request to switch to RNDIS (USB tethering) */
int usb_mode_set_rndis(UsbDevice *dev);

/* Try to trigger accessory mode (AOAv2) - device becomes host, we become HID keyboard */
int usb_mode_try_accessory(UsbDevice *dev);

/* Detect which USB configurations are available */
typedef struct {
    int has_mtp;
    int has_adb;
    int has_fastboot;
    int has_rndis;
    int has_ptp;
    int configs_count;
    char config_names[8][64];
} UsbConfigInfo;

int usb_mode_get_configs(UsbDevice *dev, UsbConfigInfo *info);

/* Switch to a specific configuration by index */
int usb_mode_set_config(UsbDevice *dev, int config_index);

/* Try all possible USB configurations and find one that gives us useful access */
/* Returns: 0=charging only, 1=MTP, 2=ADB, 3=Fastboot, 4=RNDIS */
int usb_mode_find_best(UsbDevice *dev);

#ifdef __cplusplus
}
#endif

#endif
