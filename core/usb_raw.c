#define _GNU_SOURCE
#include "usb_raw.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#if OS_POSIX && !OS_MAC
#include <dirent.h>
#include <fcntl.h>
#include <linux/usbdevice_fs.h>

/* ── Helper: read USB string descriptor ── */
int usb_raw_read_string(UsbDevice *dev, uint8_t index, char *buf, int maxlen) {
    if (!dev || dev->fd < 0 || index == 0) return -1;
    unsigned char raw[256];
    struct usbdevfs_ctrltransfer ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.bRequestType = 0x80 | 0x00 | 0x01;  /* DEVICE to HOST, STANDARD, DEVICE */
    ctrl.bRequest = 6;                         /* GET_DESCRIPTOR */
    ctrl.wValue  = (3 << 8) | index;           /* STRING descriptor */
    ctrl.wIndex  = 0x0409;                     /* English US */
    ctrl.wLength = sizeof(raw);
    ctrl.data    = raw;
    ctrl.timeout = 5000;
    int r = ioctl(dev->fd, USBDEVFS_CONTROL, &ctrl);
    if (r < 2) return -1;
    int slen = raw[0] - 2;
    if (slen > maxlen - 1) slen = maxlen - 1;
    if (slen > 0) {
        for (int i = 0; i < slen; i++)
            buf[i] = (char)raw[2 + i*2];
        buf[slen] = 0;
    } else {
        buf[0] = 0;
    }
    return slen;
}

/* ── Parse configuration descriptor and extract interfaces ── */
static int parse_config_descriptor(int fd, unsigned char *buf, int buflen,
                                    UsbRawInterface *ifaces, int max_ifaces) {
    int ilen = buf[0];
    unsigned char *p = buf + 9;  /* skip 9-byte config descriptor header */
    int left = ilen - 9;
    int nif = 0;

    while (left >= 3 && nif < max_ifaces) {
        int len = p[0];
        if (len < 3 || len > left) break;

        if (p[1] == 4 && len >= 9) {
            /* INTERFACE descriptor */
            UsbRawInterface *iface = &ifaces[nif];
            memset(iface, 0, sizeof(*iface));
            iface->bInterfaceNumber   = p[2];
            iface->bAlternateSetting  = p[3];
            iface->bNumEndpoints      = p[4];
            iface->bInterfaceClass    = p[5];
            iface->bInterfaceSubClass = p[6];
            iface->bInterfaceProtocol = p[7];
            iface->ep_count = 0;
            nif++;
        } else if (p[1] == 5 && len >= 7 && nif > 0) {
            /* ENDPOINT descriptor within current interface */
            UsbRawInterface *iface = &ifaces[nif - 1];
            if (iface->ep_count < USBRAW_MAX_EPS) {
                UsbRawEndpoint *ep = &iface->endpoints[iface->ep_count];
                ep->bEndpointAddress = p[2];
                ep->bmAttributes     = p[3];
                ep->wMaxPacketSize   = p[4] | (p[5] << 8);
                ep->bInterval        = p[6];
                iface->ep_count++;
            }
        } else if (p[1] == 15 && len >= 6) {
            /* INTERFACE ASSOCIATION descriptor (IAD) */
            /* Skip - interfaces follow normally */
        }

        left -= len;
        p    += len;
    }
    return nif;
}

/* ── Open a specific USB device by bus/device number ── */
int usb_raw_open_device(int bus, int devnum, UsbRawDevice *info) {
    if (!info) return -1;
    memset(info, 0, sizeof(*info));
    info->dev.fd = -1;

    char fp[128];
    snprintf(fp, sizeof(fp), "/dev/bus/usb/%03d/%03d", bus, devnum);
    int fd = open(fp, O_RDWR);
    if (fd < 0) return -1;

    /* Read device descriptor (18 bytes) */
    unsigned char devdesc[18];
    struct usbdevfs_ctrltransfer ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.bRequestType = 0x80 | 0x00 | 0x00;
    ctrl.bRequest = 6;
    ctrl.wValue  = (1 << 8) | 0;
    ctrl.wIndex  = 0;
    ctrl.wLength = sizeof(devdesc);
    ctrl.data    = devdesc;
    ctrl.timeout = 5000;
    int r = ioctl(fd, USBDEVFS_CONTROL, &ctrl);
    if (r < 18) { close(fd); return -1; }

    info->present      = 1;
    info->dev.fd       = fd;
    info->bus          = bus;
    info->dev_num      = devnum;
    info->idVendor     = devdesc[8]  | (devdesc[9]  << 8);
    info->idProduct    = devdesc[10] | (devdesc[11] << 8);
    info->bcdDevice    = devdesc[12] | (devdesc[13] << 8);
    info->bDeviceClass    = devdesc[4];
    info->bDeviceSubClass = devdesc[5];
    info->bDeviceProtocol = devdesc[6];
    info->bMaxPacketSize0 = devdesc[7];
    info->bNumConfigurations = devdesc[17];

    /* Read string descriptors */
    if (devdesc[14]) usb_raw_read_string(&info->dev, devdesc[14], info->manufacturer, sizeof(info->manufacturer));
    if (devdesc[15]) usb_raw_read_string(&info->dev, devdesc[15], info->product_str, sizeof(info->product_str));
    if (devdesc[16]) usb_raw_read_string(&info->dev, devdesc[16], info->serial, sizeof(info->serial));

    /* Read config descriptor */
    unsigned char cfgbuf[2048];
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.bRequestType = 0x80 | 0x00 | 0x00;
    ctrl.bRequest = 6;
    ctrl.wValue  = (2 << 8) | 0;
    ctrl.wIndex  = 0;
    ctrl.wLength = sizeof(cfgbuf);
    ctrl.data    = cfgbuf;
    ctrl.timeout = 5000;
    r = ioctl(fd, USBDEVFS_CONTROL, &ctrl);
    if (r >= 9) {
        info->iface_count = parse_config_descriptor(fd, cfgbuf, r,
                                                     info->ifaces, USBRAW_MAX_IFACES);
    }

    /* Analyze interfaces */
    for (int i = 0; i < info->iface_count; i++) {
        UsbRawInterface *iface = &info->ifaces[i];
        if (iface->bInterfaceClass == 0xFF && iface->bInterfaceSubClass == 0x42) {
            if (iface->bInterfaceProtocol == 0x01) info->has_adb = 1;
            if (iface->bInterfaceProtocol == 0x02) info->has_sideload = 1;
            if (iface->bInterfaceProtocol == 0x03) info->has_fastboot = 1;
        }
        if (iface->bInterfaceClass == 0xFF && iface->bInterfaceSubClass == 0xFF)
            info->has_mtp = 1;
        if (iface->bInterfaceClass == 0x02 && iface->bInterfaceSubClass == 0x02 &&
            iface->bInterfaceProtocol == 0x01)
            info->has_rndis = 1;
        if (iface->bInterfaceClass == 0xEF && iface->bInterfaceSubClass == 0x04 &&
            iface->bInterfaceProtocol == 0x01)
            info->has_rndis = 1;
        if (iface->bInterfaceClass == 0x03)
            info->has_hid = 1;
        if (iface->bInterfaceClass == 0x01)
            info->has_audio = 1;
        if (iface->bInterfaceClass == 0x0E)
            info->has_video = 1;
    }

    /* Classify mode */
    strncpy(info->mode, usb_raw_classify_mode(info), sizeof(info->mode)-1);

    return 1;
}

/* ── Open from existing UsbDevice (already claimed) ── */
int usb_raw_open_from(UsbDevice *dev, UsbRawDevice *info) {
    if (!dev || !info) return -1;
    return usb_raw_open_device(dev->bus, dev->dev_num, info);
}

/* ── Scan all USB buses ── */
int usb_raw_scan_all(UsbRawDevice *devices, int max_devices) {
    if (!devices) return -1;
    int ndev = 0;

    for (int b = 1; b <= 99 && ndev < max_devices; b++) {
        char path[64];
        snprintf(path, sizeof(path), "/dev/bus/usb/%03d", b);
        DIR *dir = opendir(path);
        if (!dir) continue;

        struct dirent *e;
        while ((e = readdir(dir)) && ndev < max_devices) {
            if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;
            int d = atoi(e->d_name);
            if (d < 1) continue;

            UsbRawDevice *info = &devices[ndev];
            if (usb_raw_open_device(b, d, info) > 0)
                ndev++;
        }
        closedir(dir);
    }
    return ndev;
}

/* ── Close ── */
void usb_raw_close(UsbRawDevice *info) {
    if (!info) return;
    if (info->dev.fd >= 0) close(info->dev.fd);
    memset(info, 0, sizeof(*info));
    info->dev.fd = -1;
}

#else /* not Linux */

int usb_raw_read_string(UsbDevice *d, uint8_t i, char *b, int m) { return -1; }
int usb_raw_open_device(int b, int d, UsbRawDevice *i) { return -1; }
int usb_raw_open_from(UsbDevice *d, UsbRawDevice *i) { return -1; }
int usb_raw_scan_all(UsbRawDevice *d, int m) { return 0; }
void usb_raw_close(UsbRawDevice *i) {}

#endif /* OS_POSIX */

/* ── Mode classification ── */
const char* usb_raw_classify_mode(const UsbRawDevice *info) {
    if (!info || !info->present) return "NONE";
    if (info->has_fastboot) return "FASTBOOT";
    if (info->has_sideload && info->has_adb) return "RECOVERY";
    if (info->has_adb) {
        /* If only ADB with no other typical Android interfaces, might be recovery */
        if (info->iface_count <= 2) return "RECOVERY";
        return "ADB";
    }
    if (info->has_mtp) return "MTP";
    if (info->has_rndis) return "RNDIS";
    if (info->has_hid)  return "HID";
    return "CHARGING";
}

/* ── Print all device info ── */
void usb_raw_print_info(const UsbRawDevice *info) {
    if (!info || !info->present) {
        printf("  No device\n");
        return;
    }

    printf("  Vendor : 0x%04X", info->idVendor);
    if (info->manufacturer[0]) printf(" (%s)", info->manufacturer);
    printf("\n");

    printf("  Product: 0x%04X", info->idProduct);
    if (info->product_str[0]) printf(" (%s)", info->product_str);
    printf("  Rev: 0x%04X\n", info->bcdDevice);

    printf("  Serial : %s\n", info->serial[0] ? info->serial : "(none)");
    printf("  Bus    : %03d:%03d\n", info->bus, info->dev_num);
    printf("  Class  : 0x%02X/0x%02X/0x%02X\n",
           info->bDeviceClass, info->bDeviceSubClass, info->bDeviceProtocol);
    printf("  Mode   : %s\n", info->mode);
    printf("  Configs: %d  Interfaces: %d\n",
           info->bNumConfigurations, info->iface_count);

    printf("  Flags  :");
    if (info->has_adb)      printf(" ADB");
    if (info->has_fastboot) printf(" FASTBOOT");
    if (info->has_sideload) printf(" SIDELOAD");
    if (info->has_mtp)      printf(" MTP");
    if (info->has_rndis)    printf(" RNDIS");
    if (info->has_hid)      printf(" HID");
    if (info->has_audio)    printf(" AUDIO");
    if (info->has_video)    printf(" VIDEO");
    printf("\n\n");

    /* Print each interface */
    for (int i = 0; i < info->iface_count; i++) {
        const UsbRawInterface *iface = &info->ifaces[i];
        printf("  ├─ Interface %d: Class=0x%02X Sub=0x%02X Prot=0x%02X",
               iface->bInterfaceNumber,
               iface->bInterfaceClass,
               iface->bInterfaceSubClass,
               iface->bInterfaceProtocol);
        if (iface->bInterfaceClass == 0xFF && iface->bInterfaceSubClass == 0x42) {
            if (iface->bInterfaceProtocol == 0x01) printf(" [ADB]");
            if (iface->bInterfaceProtocol == 0x02) printf(" [SIDELOAD]");
            if (iface->bInterfaceProtocol == 0x03) printf(" [FASTBOOT]");
        }
        printf("\n");
        for (int e = 0; e < iface->ep_count; e++) {
            const UsbRawEndpoint *ep = &iface->endpoints[e];
            printf("  │  ├─ EP 0x%02X %s sz=%d\n",
                   ep->bEndpointAddress,
                   (ep->bEndpointAddress & 0x80) ? "IN " : "OUT",
                   ep->wMaxPacketSize);
        }
    }
}
