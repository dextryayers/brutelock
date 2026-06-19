#include "usb.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#ifdef __linux__
#include <dirent.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/usbdevice_fs.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#define VENDOR_GOOGLE  0x18d1
#define VENDOR_SAMSUNG 0x04e8
#define VENDOR_XIAOMI  0x2717
#define VENDOR_HUAWEI  0x12d1
#define VENDOR_ONEPLUS 0x2a70
#define VENDOR_OPPO    0x22d9
#define VENDOR_VIVO    0x2d95
#define VENDOR_LG      0x1004
#define VENDOR_SONY    0x054c
#define VENDOR_MOTOROLA 0x22b8
#define VENDOR_NOKIA   0x0421
#define VENDOR_GOOGLE2 0x18d2
#define VENDOR_REALME  0x2a71
#define VENDOR_ASUS    0x0b05
#define VENDOR_LENOVO  0x17ef
#define VENDOR_HTC     0x0bb4
#define VENDOR_ACER    0x0502
#define VENDOR_ZTE     0x19d2
#define VENDOR_MEIZU   0x2d02
#define VENDOR_NUBIA   0x19d1
#define VENDOR_TCL     0x1bbb

static int is_android_vid(int vid) {
    switch (vid) {
        case VENDOR_GOOGLE: case VENDOR_SAMSUNG: case VENDOR_XIAOMI:
        case VENDOR_HUAWEI: case VENDOR_ONEPLUS: case VENDOR_OPPO:
        case VENDOR_VIVO: case VENDOR_LG: case VENDOR_SONY:
        case VENDOR_MOTOROLA: case VENDOR_NOKIA: case VENDOR_GOOGLE2:
        case VENDOR_REALME: case VENDOR_ASUS: case VENDOR_LENOVO:
        case VENDOR_HTC: case VENDOR_ACER: case VENDOR_ZTE:
        case VENDOR_MEIZU: case VENDOR_NUBIA: case VENDOR_TCL:
            return 1;
        default: return 0;
    }
}

typedef struct {
    char name[64]; int vid; int pid;
    char serial[128]; char mode[64];
    int adb_iface, fastboot_iface, recovery_iface;
} UsbDeviceInfo;

static UsbDeviceInfo g_devices[16];
static int g_ndev = 0;

static const char* vendor_name(int vid) {
    switch (vid) {
        case VENDOR_GOOGLE: return "Google";
        case VENDOR_SAMSUNG: return "Samsung";
        case VENDOR_XIAOMI: return "Xiaomi";
        case VENDOR_HUAWEI: return "Huawei";
        case VENDOR_ONEPLUS: case VENDOR_REALME: return "OnePlus/Realme";
        case VENDOR_OPPO: return "Oppo";
        case VENDOR_VIVO: return "Vivo";
        case VENDOR_NUBIA: return "Nubia";
        case VENDOR_LG: return "LG";
        case VENDOR_SONY: return "Sony";
        case VENDOR_MOTOROLA: return "Motorola";
        case VENDOR_NOKIA: return "Nokia";
        case VENDOR_ASUS: return "Asus";
        case VENDOR_LENOVO: return "Lenovo";
        case VENDOR_HTC: return "HTC";
        case VENDOR_ACER: return "Acer";
        case VENDOR_ZTE: return "ZTE";
        case VENDOR_MEIZU: return "Meizu";
        case VENDOR_TCL: return "TCL";
        case 0x18d2: return "Google (alt)";
        default: return "Unknown";
    }
}

/* Check USB interface for ADB (0xFF/0x42/0x01) or Fastboot (0xFF/0x42/0x03) */
#ifdef __linux__
static int parse_usb_iface(const char *bus_path, const char *dev_name, UsbDeviceInfo *info) {
    char fp[128];
    snprintf(fp, sizeof(fp), "%s/%s", bus_path, dev_name);
    int fd = open(fp, O_RDWR);
    if (fd < 0) return 0;

    unsigned char buf[1024];
    struct usbdevfs_ctrltransfer ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.bRequestType = 0x80 | 0x00 | 0x00;
    ctrl.bRequest = 6;
    ctrl.wValue = (2 << 8) | 0;
    ctrl.wIndex = 0;
    ctrl.wLength = sizeof(buf);
    ctrl.data = buf;
    ctrl.timeout = 5000;
    if (ioctl(fd, USBDEVFS_CONTROL, &ctrl) < 9) { close(fd); return 0; }

    int ilen = buf[0];
    unsigned char *p = buf + 9;
    int left = ilen - 9;

    while (left >= 3) {
        int len = p[0];
        if (len < 3 || len > left) break;
        if (p[1] == 4 && len >= 9) {
            int cls = p[3], sub = p[4], prot = p[5];
            if (cls == 0xff && sub == 0x42) {
                if (prot == 0x01) info->adb_iface = 1;
                if (prot == 0x03) info->fastboot_iface = 1;
            }
        }
        left -= len; p += len;
    }
    close(fd);
    return 1;
}

static int scan_linux(void) {
    g_ndev = 0;
    DIR *d = opendir("/sys/bus/usb/devices");
    if (!d) return 0;
    struct dirent *de;
    while ((de = readdir(d)) && g_ndev < 16) {
        if (de->d_name[0] == '.') continue;
        char path[512], buf[256];
        int vid = 0, pid = 0;

        snprintf(path, sizeof(path), "/sys/bus/usb/devices/%s/idVendor", de->d_name);
        FILE *f = fopen(path, "r");
        if (f) { if (fgets(buf, sizeof(buf), f)) vid = (int)strtol(buf, NULL, 16); fclose(f); }

        snprintf(path, sizeof(path), "/sys/bus/usb/devices/%s/idProduct", de->d_name);
        f = fopen(path, "r");
        if (f) { if (fgets(buf, sizeof(buf), f)) pid = (int)strtol(buf, NULL, 16); fclose(f); }

        if (!is_android_vid(vid)) continue;

        UsbDeviceInfo *dev = &g_devices[g_ndev];
        snprintf(dev->name, sizeof(dev->name), "%s:0x%04x:0x%04x", de->d_name, vid, pid);
        dev->vid = vid;
        dev->pid = pid;
        dev->adb_iface = 0;
        dev->fastboot_iface = 0;

        /* Try to get serial */
        snprintf(path, sizeof(path), "/sys/bus/usb/devices/%s/serial", de->d_name);
        f = fopen(path, "r");
        if (f) { if (fgets(dev->serial, sizeof(dev->serial), f)) trim_newline(dev->serial); fclose(f); }

        /* Check for ADB/Fastboot interfaces via /dev/bus/usb/ */
        for (int b = 1; b <= 20; b++) {
            char bus_path[64];
            snprintf(bus_path, sizeof(bus_path), "/dev/bus/usb/%03d", b);
            DIR *bdir = opendir(bus_path);
            if (!bdir) continue;
            struct dirent *be;
            while ((be = readdir(bdir))) {
                if (strcmp(be->d_name, de->d_name) == 0) {
                    parse_usb_iface(bus_path, be->d_name, dev);
                    break;
                }
            }
            closedir(bdir);
        }

        if (dev->adb_iface) snprintf(dev->mode, sizeof(dev->mode), "ADB");
        else if (dev->fastboot_iface) snprintf(dev->mode, sizeof(dev->mode), "FASTBOOT");
        else snprintf(dev->mode, sizeof(dev->mode), "CHARGING");

        printf("[USB] %s %s | VID:0x%04x PID:0x%04x | Mode: %s",
               vendor_name(vid), dev->serial, vid, pid, dev->mode);
        if (dev->adb_iface) printf(" (ADB available)");
        if (dev->fastboot_iface) printf(" (FASTBOOT mode)");
        printf("\n");

        g_ndev++;
    }
    closedir(d);
    return g_ndev;
}
#elif __APPLE__
static int scan_macos(void) {
    g_ndev = 0;
    FILE *fp = popen("system_profiler SPUSBDataType 2>/dev/null | grep -E 'Vendor ID|Product ID|Manufacturer|Serial Number'", "r");
    if (!fp) return 0;
    char buf[512];
    while (fgets(buf, sizeof(buf), fp) && g_ndev < 16) {
        if (strstr(buf, "0x")) {
            char *p = strstr(buf, "0x");
            if (p) {
                int vid = (int)strtol(p+2, NULL, 16);
                if (is_android_vid(vid)) {
                    UsbDeviceInfo *dev = &g_devices[g_ndev];
                    dev->vid = vid;
                    snprintf(dev->mode, sizeof(dev->mode), "ADB");
                    snprintf(dev->name, sizeof(dev->name), "macOS device");
                    printf("[USB] %s | VID:0x%04x | Mode: ADB\n", vendor_name(vid), vid);
                    g_ndev++;
                }
            }
        }
    }
    pclose(fp);
    return g_ndev;
}
#elif _WIN32
static int scan_windows(void) {
    g_ndev = 0;
    FILE *fp = popen("wmic path Win32_USBControllerDevice get Dependent 2>nul", "r");
    if (!fp) return 0;
    char buf[512];
    while (fgets(buf, sizeof(buf), fp) && g_ndev < 16) {
        if (strstr(buf, "USB\\VID_")) {
            char *p = strstr(buf, "VID_");
            if (p) {
                int vid = (int)strtol(p+4, NULL, 16);
                if (is_android_vid(vid)) {
                    UsbDeviceInfo *dev = &g_devices[g_ndev];
                    dev->vid = vid;
                    snprintf(dev->mode, sizeof(dev->mode), "ADB");
                    printf("[USB] %s | VID:0x%04x\n", vendor_name(vid), vid);
                    g_ndev++;
                }
            }
        }
    }
    pclose(fp);
    return g_ndev;
}
#else
static int scan_other(void) { return -1; }
#endif

int usb_scan_devices(void) {
#ifdef __linux__
    return scan_linux();
#elif __APPLE__
    return scan_macos();
#elif _WIN32
    return scan_windows();
#else
    return -1;
#endif
}

int usb_wait_for_device(int timeout) {
    printf("[*] Waiting for USB device");
    fflush(stdout);
    int waited = 0;
    while (timeout == 0 || waited < timeout) {
        if (usb_scan_devices() > 0) {
            printf("\n[+] USB device detected.\n");
            return 1;
        }
        printf(".");
        fflush(stdout);
        sleep(2);
        waited += 2;
    }
    printf("\n");
    return 0;
}

int usb_get_device_count(void) { return g_ndev; }

const char* usb_get_device_serial(int idx) {
    if (idx < 0 || idx >= g_ndev) return "";
    return g_devices[idx].serial;
}

const char* usb_get_device_mode(int idx) {
    if (idx < 0 || idx >= g_ndev) return "";
    return g_devices[idx].mode;
}

const char* usb_get_device_vendor(int idx) {
    if (idx < 0 || idx >= g_ndev) return "Unknown";
    return vendor_name(g_devices[idx].vid);
}

int usb_has_adb(int idx) {
    if (idx < 0 || idx >= g_ndev) return 0;
    return g_devices[idx].adb_iface;
}

int usb_has_fastboot(int idx) {
    if (idx < 0 || idx >= g_ndev) return 0;
    return g_devices[idx].fastboot_iface;
}
