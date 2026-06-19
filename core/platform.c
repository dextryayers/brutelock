#include "platform.h"
#include <string.h>

#if OS_POSIX && !OS_MAC
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <linux/usbdevice_fs.h>

/* ── Raw USB I/O via /dev/bus/usb/ ────────────────────────────────── */

/* Read config descriptor and parse interface descriptors.
   Returns 1 if interface with given class/subclass/protocol found,
   and populates ep_in, ep_out, iface_num. */
static int find_iface_on_device(int fd, int cls, int sub, int prot,
                                 int *ep_in, int *ep_out, int *iface_n) {
    unsigned char buf[1024];
    struct usbdevfs_ctrltransfer ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.bRequestType = 0x80 | 0x00 | 0x00;
    ctrl.bRequest = 6;              /* GET_DESCRIPTOR */
    ctrl.wValue  = (2 << 8) | 0;    /* CONFIGURATION descriptor index 0 */
    ctrl.wIndex  = 0;
    ctrl.wLength = sizeof(buf);
    ctrl.data    = buf;
    ctrl.timeout = 5000;
    int ret = ioctl(fd, USBDEVFS_CONTROL, &ctrl);
    if (ret < 9) return 0;

    int ilen = buf[0];               /* total config descriptor length */
    unsigned char *p = buf + 9;       /* skip 8-byte config + 1-byte interface? no... */
    /* Actually: config descriptor is 9 bytes, then interface+endpoint */
    p = buf + 9;
    int left = ilen - 9;

    while (left >= 3) {
        int len = p[0];
        if (len < 3 || len > left) break;

        if (p[1] == 4 && len >= 9) {
            /* INTERFACE descriptor */
            if (p[3] == cls && p[4] == sub && p[5] == prot) {
                *iface_n = p[2];
                /* Walk endpoints within this interface */
                unsigned char *ep = p + len;
                int epl = left - len;
                while (epl >= 7) {
                    if (ep[0] < 3) break;
                    if (ep[1] == 5) { /* ENDPOINT descriptor */
                        if (ep[2] & 0x80) *ep_in  = ep[2];
                        else              *ep_out = ep[2];
                    }
                    epl -= ep[0]; ep += ep[0];
                }
                return (*ep_in && *ep_out) ? 1 : 1;
            }
        }
        left -= len;
        p    += len;
    }
    return 0;
}

/* Generic: find first USB device with interface matching class/sub/prot */
int platform_usb_find_by_class(UsbDevice *dev, int cls, int sub, int prot) {
    memset(dev, 0, sizeof(*dev));
    dev->fd = -1;

    for (int b = 1; b <= 99; b++) {
        char buspath[64];
        snprintf(buspath, sizeof(buspath), "/dev/bus/usb/%03d", b);
        DIR *dir = opendir(buspath);
        if (!dir) continue;

        struct dirent *e;
        while ((e = readdir(dir))) {
            if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;

            char fp[128];
            snprintf(fp, sizeof(fp), "%s/%s", buspath, e->d_name);
            int fd = open(fp, O_RDWR);
            if (fd < 0) continue;

            /* Read device descriptor for VID/PID */
            unsigned char devdesc[18];
            struct usbdevfs_ctrltransfer ctrl;
            memset(&ctrl, 0, sizeof(ctrl));
            ctrl.bRequestType = 0x80 | 0x00 | 0x00;
            ctrl.bRequest = 6;
            ctrl.wValue  = (1 << 8) | 0;  /* DEVICE descriptor */
            ctrl.wIndex  = 0;
            ctrl.wLength = sizeof(devdesc);
            ctrl.data    = devdesc;
            ctrl.timeout = 5000;
            int r = ioctl(fd, USBDEVFS_CONTROL, &ctrl);
            if (r < 18) { close(fd); continue; }

            int vid = devdesc[8] | (devdesc[9] << 8);
            int pid = devdesc[10] | (devdesc[11] << 8);

            int ep_in = 0, ep_out = 0, iface_n = -1;
            if (find_iface_on_device(fd, cls, sub, prot, &ep_in, &ep_out, &iface_n)) {
                struct usbdevfs_ioctl ioc;
                ioc.ifno = iface_n;
                ioc.ioctl_code = USBDEVFS_DISCONNECT;
                ioc.data = NULL;
                ioctl(fd, USBDEVFS_IOCTL, &ioc);

                if (ioctl(fd, USBDEVFS_CLAIMINTERFACE, &iface_n) == 0) {
                    dev->fd        = fd;
                    dev->ep_in     = ep_in;
                    dev->ep_out    = ep_out;
                    dev->iface_num = iface_n;
                    dev->has_iface = 1;
                    dev->vid       = vid;
                    dev->pid       = pid;
                    dev->bus       = b;
                    dev->dev_num   = atoi(e->d_name);

                    /* Read serial from iSerialNumber string descriptor */
                    dev->serial[0] = 0;
                    unsigned char strdesc[256];
                    memset(&ctrl, 0, sizeof(ctrl));
                    ctrl.bRequestType = 0x80 | 0x00 | 0x01;  /* DEVICE to HOST, STANDARD, INTERFACE */
                    ctrl.bRequest = 6;
                    ctrl.wValue  = (3 << 8) | devdesc[16];  /* STRING descriptor, index = iSerialNumber */
                    ctrl.wIndex  = 0x0409;                  /* English US */
                    ctrl.wLength = sizeof(strdesc);
                    ctrl.data    = strdesc;
                    ctrl.timeout = 5000;
                    r = ioctl(fd, USBDEVFS_CONTROL, &ctrl);
                    if (r > 2) {
                        int slen = strdesc[0] - 2;
                        if (slen > 0 && slen < (int)sizeof(dev->serial)) {
                            for (int i = 0; i < slen; i++)
                                dev->serial[i] = (char)strdesc[2 + i*2];
                            dev->serial[slen] = 0;
                        }
                    }

                    closedir(dir);
                    return 1;
                }
            }
            close(fd);
        }
        closedir(dir);
    }
    return 0;
}

int platform_usb_find_adb(UsbDevice *dev) {
    return platform_usb_find_by_class(dev, 0xFF, 0x42, 0x01);
}

int platform_usb_find_fastboot(UsbDevice *dev) {
    return platform_usb_find_by_class(dev, 0xFF, 0x42, 0x03);
}

int platform_usb_write(UsbDevice *dev, int ep, const void *buf, int len, int timeout_ms) {
    struct usbdevfs_bulktransfer bt;
    bt.ep = ep; bt.len = len; bt.timeout = timeout_ms; bt.data = (void*)buf;
    return ioctl(dev->fd, USBDEVFS_BULK, &bt);
}

int platform_usb_read(UsbDevice *dev, int ep, void *buf, int maxlen, int timeout_ms) {
    struct usbdevfs_bulktransfer bt;
    bt.ep = ep; bt.len = maxlen; bt.timeout = timeout_ms; bt.data = buf;
    return ioctl(dev->fd, USBDEVFS_BULK, &bt);
}

void platform_usb_close(UsbDevice *dev) {
    if (dev->has_iface && dev->fd >= 0) {
        int iface = dev->iface_num;
        ioctl(dev->fd, USBDEVFS_RELEASEINTERFACE, &iface);
    }
    if (dev->fd >= 0) close(dev->fd);
    memset(dev, 0, sizeof(*dev));
    dev->fd = -1;
}

int platform_usb_control(UsbDevice *dev, uint8_t bmReqType, uint8_t bRequest,
                          uint16_t wValue, uint16_t wIndex,
                          void *data, uint16_t wLength, int timeout_ms) {
    struct usbdevfs_ctrltransfer ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.bRequestType = bmReqType;
    ctrl.bRequest     = bRequest;
    ctrl.wValue       = wValue;
    ctrl.wIndex       = wIndex;
    ctrl.wLength      = wLength;
    ctrl.data         = data;
    ctrl.timeout      = timeout_ms;
    return ioctl(dev->fd, USBDEVFS_CONTROL, &ctrl);
}

int platform_usb_get_device_desc(UsbDevice *dev, unsigned char *buf, int bufsz) {
    struct usbdevfs_ctrltransfer ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.bRequestType = 0x80 | 0x00 | 0x00;
    ctrl.bRequest = 6;
    ctrl.wValue  = (1 << 8) | 0;
    ctrl.wIndex  = 0;
    ctrl.wLength = bufsz < 18 ? bufsz : 18;
    ctrl.data    = buf;
    ctrl.timeout = 5000;
    return ioctl(dev->fd, USBDEVFS_CONTROL, &ctrl);
}

int platform_usb_get_serial(UsbDevice *dev, char *serial, int maxlen) {
    if (dev->serial[0]) {
        strncpy(serial, dev->serial, maxlen-1);
        serial[maxlen-1] = 0;
        return 0;
    }
    return -1;
}

int platform_usb_reset(UsbDevice *dev) {
    if (dev->fd < 0) return -1;
    return ioctl(dev->fd, USBDEVFS_RESET);
}

#elif OS_MAC
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/usb/USBSpec.h>

/* ── macOS USB via IOKit ── */

/* Helper: get USB string descriptor via device interface */
static CFStringRef mac_get_string_ref(IOUSBDeviceInterface **devif, UCHAR index) {
    if (!devif || !*devif) return NULL;
    CFStringRef str = NULL;
    (*devif)->USBDeviceCopyStringDescriptor(devif, 0, index, &str);
    return str;
}

/* Convert CFString to C string buffer */
static int cfstr_to_cstr(CFStringRef cf, char *buf, int maxlen) {
    if (!cf) return -1;
    Boolean ok = CFStringGetCString(cf, buf, maxlen, kCFStringEncodingUTF8);
    if (!ok) buf[0] = 0;
    CFRelease(cf);
    return ok ? (int)strlen(buf) : -1;
}

/* Iterate endpoints to find bulk IN/OUT */
static void mac_find_endpoints(IOUSBInterfaceInterface **intf, UCHAR *ep_in, UCHAR *ep_out) {
    *ep_in = *ep_out = 0;
    if (!intf || !*intf) return;
    UCHAR num_eps = 0;
    (*intf)->GetNumEndpoints(intf, &num_eps);
    for (UCHAR i = 1; i <= num_eps; i++) {
        UCHAR dir, num, ttype;
        UWORD maxpkt;
        UCHAR interval;
        IOReturn kr = (*intf)->GetPipeProperties(intf, i, &dir, &num, &ttype, &maxpkt, &interval);
        if (kr != kIOReturnSuccess) continue;
        if (ttype == kUSBBulk) {
            UCHAR addr = num | (dir << 7);
            if (dir == kUSBIn) *ep_in = addr;
            else               *ep_out = addr;
        }
    }
}

int platform_usb_find_by_class(UsbDevice *dev, int cls, int sub, int prot) {
    memset(dev, 0, sizeof(*dev));
    dev->fd = -1;

    /* Matching dictionary for USB devices */
    CFMutableDictionaryRef dict = IOServiceMatching(kIOUSBDeviceClassName);
    if (!dict) return 0;

    io_iterator_t iter = 0;
    IOReturn kr = IOServiceGetMatchingServices(kIOMasterPortDefault, dict, &iter);
    if (kr != kIOReturnSuccess || !iter) return 0;

    int found = 0;
    io_service_t usbDevice;
    while ((usbDevice = IOIteratorNext(iter)) && !found) {
        IOCFPlugInInterface **plugIn = NULL;
        SInt32 score = 0;
        kr = IOCreatePlugInInterfaceForService(usbDevice,
                                               kIOUSBDeviceUserClientTypeID,
                                               kIOCFPlugInInterfaceID,
                                               &plugIn, &score);
        IOObjectRelease(usbDevice);
        if (kr != kIOReturnSuccess || !plugIn) continue;

        IOUSBDeviceInterface **devif = NULL;
        HRESULT res = (*plugIn)->QueryInterface(plugIn,
                                                CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID),
                                                (LPVOID*)&devif);
        (*plugIn)->Release(plugIn);
        if (res != S_OK || !devif) continue;

        /* Open device */
        kr = (*devif)->USBDeviceOpen(devif);
        if (kr != kIOReturnSuccess) { (*devif)->Release(devif); continue; }

        /* Get device descriptor info */
        UInt16 vendor = 0, product = 0;
        UInt8 num_config = 0;
        (*devif)->GetDeviceVendor(devif, &vendor);
        (*devif)->GetDeviceProduct(devif, &product);
        (*devif)->GetNumberOfConfigurations(devif, &num_config);

        IOUSBConfigurationDescriptorPtr cfgDesc = NULL;
        kr = (*devif)->GetConfigurationDescriptorPtr(devif, 0, &cfgDesc);
        if (kr != kIOReturnSuccess || !cfgDesc) {
            (*devif)->USBDeviceClose(devif);
            (*devif)->Release(devif);
            continue;
        }

        /* Walk interface descriptors */
        UInt8 *cfg = (UInt8*)cfgDesc;
        int cfg_len = cfgDesc->wTotalLength;
        UInt8 *p = cfg + 9;
        int left = cfg_len - 9;
        UCHAR ep_in = 0, ep_out = 0;
        int iface_found = 0;

        while (left >= 3 && !iface_found) {
            int len = p[0];
            if (len < 3 || len > left) break;

            if (p[1] == 4 && len >= 9) {
                if (p[3] == cls && p[4] == sub && p[5] == prot) {
                    UInt8 iface_num = p[2];

                    /* Find the interface runtime to get endpoints */
                    IOUSBFindInterfaceRequest ifReq;
                    ifReq.bInterfaceClass = cls;
                    ifReq.bInterfaceSubClass = sub;
                    ifReq.bInterfaceProtocol = prot;
                    ifReq.bAlternateSetting = kIOUSBFindInterfaceDontCare;

                    io_iterator_t ifIter = 0;
                    (*devif)->CreateInterfaceIterator(devif, &ifReq, &ifIter);
                    if (ifIter) {
                        io_service_t ifObj;
                        while ((ifObj = IOIteratorNext(ifIter))) {
                            IOCFPlugInInterface **ifPlug = NULL;
                            SInt32 ifScore = 0;
                            IOCreatePlugInInterfaceForService(ifObj,
                                                              kIOUSBInterfaceUserClientTypeID,
                                                              kIOCFPlugInInterfaceID,
                                                              &ifPlug, &ifScore);
                            IOObjectRelease(ifObj);
                            if (ifPlug) {
                                IOUSBInterfaceInterface **intf = NULL;
                                (*ifPlug)->QueryInterface(ifPlug,
                                    CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID),
                                    (LPVOID*)&intf);
                                (*ifPlug)->Release(ifPlug);
                                if (intf) {
                                    (*intf)->USBInterfaceOpen(intf);
                                    mac_find_endpoints(intf, &ep_in, &ep_out);
                                    if (ep_in && ep_out) {
                                        dev->mac_intf = (void*)intf;
                                        iface_found = 1;
                                    }
                                }
                            }
                        }
                        IOObjectRelease(ifIter);
                    }

                    if (!iface_found) {
                        /* Fallback: parse from descriptor */
                        UInt8 *ep = p + len;
                        int epl = left - len;
                        while (epl >= 7) {
                            if (ep[0] < 3) break;
                            if (ep[1] == 5) {
                                if (ep[2] & 0x80) ep_in = ep[2];
                                else ep_out = ep[2];
                            }
                            epl -= ep[0]; ep += ep[0];
                        }
                        iface_found = (ep_in && ep_out);
                    }
                }
            }
            left -= len; p += len;
        }

        if (iface_found) {
            dev->fd = 1;              /* non-zero = valid */
            dev->ep_in = ep_in;
            dev->ep_out = ep_out;
            dev->has_iface = 1;
            dev->vid = vendor;
            dev->pid = product;
            dev->mac_dev = (void*)devif;
            /* Keep device open */

            /* Get serial */
            UCHAR serial_idx = 0;
            (*devif)->GetSerialNumberStringIndex(devif, &serial_idx);
            if (serial_idx) {
                CFStringRef cf = mac_get_string_ref(devif, serial_idx);
                if (cf) cfstr_to_cstr(cf, dev->serial, sizeof(dev->serial));
            }

            found = 1;
        } else {
            (*devif)->USBDeviceClose(devif);
            (*devif)->Release(devif);
        }
    }

    IOObjectRelease(iter);
    return found;
}

int platform_usb_find_adb(UsbDevice *d) {
    return platform_usb_find_by_class(d, 0xFF, 0x42, 0x01);
}

int platform_usb_find_fastboot(UsbDevice *d) {
    return platform_usb_find_by_class(d, 0xFF, 0x42, 0x03);
}

int platform_usb_write(UsbDevice *d, int ep, const void *b, int l, int t) {
    if (!d->mac_intf) return -1;
    IOUSBInterfaceInterface **intf = (IOUSBInterfaceInterface**)d->mac_intf;
    UInt8 pipe = ep & 0x0F;
    IOReturn kr = (*intf)->WritePipe(intf, pipe, (void*)b, l);
    return (kr == kIOReturnSuccess) ? l : -1;
}

int platform_usb_read(UsbDevice *d, int ep, void *b, int m, int t) {
    if (!d->mac_intf) return -1;
    IOUSBInterfaceInterface **intf = (IOUSBInterfaceInterface**)d->mac_intf;
    UInt8 pipe = ep & 0x0F;
    UInt32 len = m;
    IOReturn kr = (*intf)->ReadPipe(intf, pipe, b, &len);
    return (kr == kIOReturnSuccess) ? (int)len : -1;
}

void platform_usb_close(UsbDevice *d) {
    if (d->mac_intf) {
        IOUSBInterfaceInterface **intf = (IOUSBInterfaceInterface**)d->mac_intf;
        (*intf)->USBInterfaceClose(intf);
        (*intf)->Release(intf);
    }
    if (d->mac_dev) {
        IOUSBDeviceInterface **devif = (IOUSBDeviceInterface**)d->mac_dev;
        (*devif)->USBDeviceClose(devif);
        (*devif)->Release(devif);
    }
    memset(d, 0, sizeof(*d));
    d->fd = -1;
}

int platform_usb_control(UsbDevice *d, uint8_t a, uint8_t b, uint16_t c, uint16_t d2, void *e, uint16_t f, int g) { (void)d; return -1; }
int platform_usb_get_device_desc(UsbDevice *d, unsigned char *b, int s) {
    if (!d->mac_dev) return -1;
    IOUSBDeviceInterface **devif = (IOUSBDeviceInterface**)d->mac_dev;
    UInt16 vendor = 0, product = 0;
    (*devif)->GetDeviceVendor(devif, &vendor);
    (*devif)->GetDeviceProduct(devif, &product);
    if (s >= 2) { b[8] = vendor & 0xFF; b[9] = (vendor>>8) & 0xFF; }
    if (s >= 4) { b[10] = product & 0xFF; b[11] = (product>>8) & 0xFF; }
    return s;
}
int platform_usb_get_serial(UsbDevice *d, char *s, int m) {
    if (d->serial[0]) { strncpy(s, d->serial, m-1); s[m-1]=0; return 0; }
    return -1;
}
int platform_usb_reset(UsbDevice *d) { (void)d; return -1; }

#elif OS_WIN
#include <windows.h>
#include <winusb.h>
#include <setupapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <initguid.h>
#include <usbiodef.h>

/* ── Windows USB via SetupAPI + WinUSB ── */

/* Helper: get device string descriptor */
static int win_get_string_desc(HANDLE h, UCHAR index, char *buf, int maxlen) {
    if (!h || index == 0) return -1;
    USB_STRING_DESCRIPTOR *sd = malloc(sizeof(USB_STRING_DESCRIPTOR) + 256);
    if (!sd) return -1;
    ULONG len = 0;
    if (!WinUsb_GetDescriptor(h, USB_STRING_DESCRIPTOR_TYPE, index, 0x0409, sd,
                               sizeof(USB_STRING_DESCRIPTOR) + 256, &len)) {
        free(sd); return -1;
    }
    int slen = (sd->bLength - 2) / 2;
    if (slen > maxlen-1) slen = maxlen-1;
    for (int i = 0; i < slen; i++)
        buf[i] = (char)sd->bString[i];
    buf[slen] = 0;
    free(sd);
    return slen;
}

/* Parse config descriptor to find interface class/sub/protocol */
static int win_find_iface(const unsigned char *cfg, int cfg_len,
                           int cls, int sub, int prot,
                           UCHAR *ep_in, UCHAR *ep_out) {
    const unsigned char *p = cfg + 9;
    int left = cfg_len - 9;
    *ep_in = *ep_out = 0;

    while (left >= 3) {
        int len = p[0];
        if (len < 3 || len > left) break;
        if (p[1] == 4 && len >= 9) {
            if (p[3] == cls && p[4] == sub && p[5] == prot) {
                const unsigned char *ep = p + len;
                int epl = left - len;
                while (epl >= 7) {
                    if (ep[0] < 3) break;
                    if (ep[1] == 5) {
                        if (ep[2] & 0x80) *ep_in = ep[2];
                        else *ep_out = ep[2];
                    }
                    epl -= ep[0]; ep += ep[0];
                }
                return 1;
            }
        }
        left -= len; p += len;
    }
    return 0;
}

int platform_usb_find_by_class(UsbDevice *dev, int cls, int sub, int prot) {
    memset(dev, 0, sizeof(*dev));
    dev->fd = -1;

    /* Get device info set for all USB devices */
    HDEVINFO devs = SetupDiGetClassDevs(&GUID_DEVINTERFACE_USB_DEVICE, NULL, NULL,
                                         DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (devs == INVALID_HANDLE_VALUE) return 0;

    SP_DEVICE_INTERFACE_DATA did = { .cbSize = sizeof(did) };
    int found = 0;

    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(devs, NULL,
                                                   &GUID_DEVINTERFACE_USB_DEVICE, i, &did); i++) {
        /* Get detail size */
        DWORD req = 0;
        SetupDiGetDeviceInterfaceDetail(devs, &did, NULL, 0, &req, NULL);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) continue;

        PSP_DEVICE_INTERFACE_DETAIL_DATA detail = malloc(req);
        if (!detail) continue;
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        if (!SetupDiGetDeviceInterfaceDetail(devs, &did, detail, req, &req, NULL)) {
            free(detail); continue;
        }

        /* Open device for WinUSB */
        HANDLE h = CreateFile(detail->DevicePath, GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        free(detail);

        if (h == INVALID_HANDLE_VALUE) continue;

        /* Get WinUSB handle */
        WINUSB_INTERFACE_HANDLE winusb;
        if (!WinUsb_Initialize(h, &winusb)) {
            CloseHandle(h); continue;
        }

        /* Get device descriptor */
        USB_DEVICE_DESCRIPTOR dd;
        ULONG len = 0;
        if (!WinUsb_GetDescriptor(winusb, USB_DEVICE_DESCRIPTOR_TYPE, 0, 0x0409,
                                   (PUCHAR)&dd, sizeof(dd), &len) || len < sizeof(dd)) {
            WinUsb_Free(winusb); CloseHandle(h); continue;
        }

        /* Get config descriptor */
        unsigned char cfg[1024];
        len = 0;
        if (!WinUsb_GetDescriptor(winusb, USB_CONFIGURATION_DESCRIPTOR_TYPE, 0, 0x0409,
                                   cfg, sizeof(cfg), &len) || len < 9) {
            WinUsb_Free(winusb); CloseHandle(h); continue;
        }

        /* Find interface */
        UCHAR ep_in = 0, ep_out = 0;
        if (!win_find_iface(cfg, len, cls, sub, prot, &ep_in, &ep_out)) {
            WinUsb_Free(winusb); CloseHandle(h); continue;
        }

        /* Found matching device */
        dev->fd = (int)(intptr_t)h;  /* store handle as fd */
        dev->ep_in = ep_in;
        dev->ep_out = ep_out;
        dev->has_iface = 1;
        dev->vid = dd.idVendor;
        dev->pid = dd.idProduct;
        dev->dev_handle = h;
        dev->winusb = winusb;

        /* Get pipes for bulk transfers */
        if (ep_in) WinUsb_QueryPipe(winusb, 0, (UCHAR)(ep_in & 0x0F), NULL);
        if (ep_out) WinUsb_QueryPipe(winusb, 0, (UCHAR)(ep_out & 0x0F), NULL);

        /* Get serial */
        if (dd.iSerialNumber) win_get_string_desc(winusb, dd.iSerialNumber,
                                                   dev->serial, sizeof(dev->serial));

        found = 1;
        break;
    }

    SetupDiDestroyDeviceInfoList(devs);
    return found;
}

int platform_usb_find_adb(UsbDevice *d) {
    return platform_usb_find_by_class(d, 0xFF, 0x42, 0x01);
}

int platform_usb_find_fastboot(UsbDevice *d) {
    return platform_usb_find_by_class(d, 0xFF, 0x42, 0x03);
}

int platform_usb_write(UsbDevice *d, int ep, const void *b, int l, int t) {
    ULONG s = 0;
    if (d->winusb) {
        /* Set pipe policy for timeout */
        UCHAR timeout_val = (UCHAR)(t / 1000);
        WinUsb_SetPipePolicy(d->winusb, d->ep_out, PIPE_TRANSFER_TIMEOUT,
                             sizeof(timeout_val), &timeout_val);
        if (!WinUsb_WritePipe(d->winusb, d->ep_out, (PUCHAR)b, l, &s, NULL))
            return -1;
    }
    return (int)s;
}

int platform_usb_read(UsbDevice *d, int ep, void *b, int m, int t) {
    ULONG s = 0;
    if (d->winusb) {
        UCHAR timeout_val = (UCHAR)(t / 1000);
        WinUsb_SetPipePolicy(d->winusb, d->ep_in, PIPE_TRANSFER_TIMEOUT,
                             sizeof(timeout_val), &timeout_val);
        if (!WinUsb_ReadPipe(d->winusb, d->ep_in, (PUCHAR)b, m, &s, NULL))
            return -1;
    }
    return (int)s;
}

void platform_usb_close(UsbDevice *d) {
    if (d->winusb) WinUsb_Free(d->winusb);
    if (d->dev_handle && d->dev_handle != INVALID_HANDLE_VALUE) CloseHandle(d->dev_handle);
    memset(d, 0, sizeof(*d));
    d->fd = -1;
}

int platform_usb_control(UsbDevice *d, uint8_t a, uint8_t b, uint16_t c, uint16_t d2, void *e, uint16_t f, int g) {
    return -1; /* WinUSB control transfers need WinUsb_ControlTransfer */
}

int platform_usb_get_device_desc(UsbDevice *d, unsigned char *b, int s) {
    if (!d->winusb) return -1;
    USB_DEVICE_DESCRIPTOR dd;
    ULONG len = 0;
    if (!WinUsb_GetDescriptor(d->winusb, USB_DEVICE_DESCRIPTOR_TYPE, 0, 0x0409,
                               (PUCHAR)&dd, sizeof(dd), &len))
        return -1;
    if (s > (int)sizeof(dd)) s = sizeof(dd);
    memcpy(b, &dd, s);
    return s;
}

int platform_usb_get_serial(UsbDevice *d, char *s, int m) {
    if (d->serial[0]) { strncpy(s, d->serial, m-1); s[m-1]=0; return 0; }
    return -1;
}

int platform_usb_reset(UsbDevice *d) {
    return -1; /* USBDEVFS_RESET not available on Windows */
}
#endif
