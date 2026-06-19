#ifndef USB_DEVICE_RAW_H
#define USB_DEVICE_RAW_H

#include "util.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define USBRAW_MAX_ENDPOINTS 16
#define USBRAW_TIMEOUT_MS 5000
#define USBRAW_BUF_SIZE 65536

typedef struct {
    int fd;
    int bus;
    int dev_num;
    int idVendor;
    int idProduct;
    int bcdDevice;
    int bDeviceClass;
    int bDeviceSubClass;
    int bDeviceProtocol;
    int nEndpoints;
    struct { int ep_addr; int ep_type; int ep_maxpkt; } eps[USBRAW_MAX_ENDPOINTS];
    char manufacturer[128];
    char product[128];
    char serial[128];
    int connected;
    char last_error[256];
} UsbDeviceRaw;

typedef struct {
    int fd;
    int iface_num;
    int alt_setting;
    int opened;
    int kernel_attached;
    char last_error[256];
} UsbInterfaceRaw;

int udr_open(UsbDeviceRaw *dev, int bus, int dev_num);
int udr_claim_interface(UsbDeviceRaw *dev, UsbInterfaceRaw *iface, int iface_num);
int udr_release_interface(UsbInterfaceRaw *iface);
int udr_control_transfer(UsbDeviceRaw *dev, uint8_t bmReqType, uint8_t bRequest,
                         uint16_t wValue, uint16_t wIndex,
                         unsigned char *data, uint16_t wLength, int timeout_ms);
int udr_bulk_write(UsbDeviceRaw *dev, int ep_addr, const unsigned char *data, int len);
int udr_bulk_read(UsbDeviceRaw *dev, int ep_addr, unsigned char *data, int max_len);
int udr_interrupt_write(UsbDeviceRaw *dev, int ep_addr, const unsigned char *data, int len);
int udr_interrupt_read(UsbDeviceRaw *dev, int ep_addr, unsigned char *data, int max_len);
int udr_reset_device(UsbDeviceRaw *dev);
int udr_reset_endpoint(UsbDeviceRaw *dev, int ep_addr);
int udr_set_configuration(UsbDeviceRaw *dev, int config);
int udr_set_interface(UsbDeviceRaw *dev, UsbInterfaceRaw *iface, int alt);
int udr_detach_kernel(UsbDeviceRaw *dev, int iface_num);
int udr_attach_kernel(UsbDeviceRaw *dev, int iface_num);
int udr_get_descriptor(UsbDeviceRaw *dev, int type, int index, unsigned char *buf, int len);
int udr_send_custom_descriptor(UsbDeviceRaw *dev, const unsigned char *desc, int len);
int udr_exploit_descriptor_overflow(UsbDeviceRaw *dev);
int udr_inject_adb_key(UsbDeviceRaw *dev, const char *pubkey);
int udr_kernel_usbip_attack(UsbDeviceRaw *dev);
int udr_scan_endpoints(UsbDeviceRaw *dev);
int udr_scan_all_devices(UsbDeviceRaw *devs, int max_devs);
void udr_close(UsbDeviceRaw *dev);
void udr_close_interface(UsbInterfaceRaw *iface);

#ifdef __cplusplus
}
#endif
#endif
