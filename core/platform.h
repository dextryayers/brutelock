#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>

#if defined(_WIN32) || defined(_WIN64)
  #define OS_WIN 1
  #define OS_POSIX 0
#elif defined(__APPLE__) || defined(__MACH__)
  #define OS_WIN 0
  #define OS_POSIX 1
  #define OS_MAC 1
#else
  #define OS_WIN 0
  #define OS_POSIX 1
  #define OS_MAC 0
#endif

#if OS_WIN
  #include <windows.h>
  #include <winusb.h>
  #include <setupapi.h>
  #define popen _popen
  #define pclose _pclose
  #define sleep(x) Sleep((x)*1000)
  #define strncasecmp _strnicmp
  #define strcasecmp _stricmp
  #define PATH_SEP "\\"
  #define snprintf _snprintf
  #define usleep(x) Sleep((x)/1000)
  static inline int getch_win(void) {
      DWORD m, n; HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
      GetConsoleMode(h, &m); SetConsoleMode(h, m & ~(ENABLE_LINE_INPUT|ENABLE_ECHO_INPUT));
      char c; DWORD r; ReadConsoleA(h, &c, 1, &r, NULL);
      SetConsoleMode(h, m); return (int)(unsigned char)c;
  }
  #define getch() getch_win()
#else
  #include <stdio.h>
  #include <unistd.h>
  #include <termios.h>
  #include <sys/ioctl.h>
  #define PATH_SEP "/"
  static inline int getch_posix(void) {
      struct termios old, new_tio; int ch;
      tcgetattr(0, &old); new_tio = old;
      new_tio.c_lflag &= ~(ICANON|ECHO); tcsetattr(0, TCSANOW, &new_tio);
      ch = getchar();
      tcsetattr(0, TCSANOW, &old); return ch;
  }
  #define getch() getch_posix()
#endif

/* USB device handle */
typedef struct {
    int fd;       /* POSIX: file desc; Win: HANDLE; Mac: flag */
    int ep_in, ep_out;
    int iface_num, has_iface;
    int vid, pid;       /* vendor/product ID from device descriptor */
    int bus, dev_num;   /* bus and device number (Linux) */
    char serial[128];   /* cached serial */
#if OS_WIN
    HANDLE dev_handle;
    WINUSB_INTERFACE_HANDLE winusb;
    UCHAR pipe_in, pipe_out;
#endif
#if OS_MAC
    void *mac_dev;      /* IOUSBDeviceInterface ** */
    void *mac_intf;     /* IOUSBInterfaceInterface ** */
#endif
} UsbDevice;

/* Protocol interface types */
#define USB_IFACE_ADB      0  /* 0xFF/0x42/0x01 */
#define USB_IFACE_FASTBOOT 1  /* 0xFF/0x42/0x03 */
#define USB_IFACE_SIDELOAD 2  /* 0xFF/0x42/0x02 (recovery sideload) */

/* Platform USB init / tx / rx */
int platform_usb_find_adb(UsbDevice *dev);
int platform_usb_find_fastboot(UsbDevice *dev);
int platform_usb_find_by_class(UsbDevice *dev, int cls, int sub, int prot);
int platform_usb_write(UsbDevice *dev, int ep, const void *buf, int len, int timeout_ms);
int platform_usb_read(UsbDevice *dev, int ep, void *buf, int maxlen, int timeout_ms);
void platform_usb_close(UsbDevice *dev);

/* USB control transfer */
int platform_usb_control(UsbDevice *dev, uint8_t bmReqType, uint8_t bRequest,
                         uint16_t wValue, uint16_t wIndex,
                         void *data, uint16_t wLength, int timeout_ms);

/* Device descriptor read */
int platform_usb_get_device_desc(UsbDevice *dev, unsigned char *buf, int bufsz);

/* Get serial from USB device descriptor (iSerialNumber) */
int platform_usb_get_serial(UsbDevice *dev, char *serial, int maxlen);

/* Reset device (USBDEVFS_RESET on Linux) */
int platform_usb_reset(UsbDevice *dev);

#endif
