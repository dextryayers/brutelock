#ifndef USB_H
#define USB_H

#ifdef __cplusplus
extern "C" {
#endif

int usb_scan_devices(void);
int usb_wait_for_device(int timeout);
int usb_get_device_count(void);
const char* usb_get_device_serial(int idx);
const char* usb_get_device_mode(int idx);
const char* usb_get_device_vendor(int idx);
int usb_has_adb(int idx);
int usb_has_fastboot(int idx);

#ifdef __cplusplus
}
#endif

#endif
