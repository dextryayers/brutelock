#ifndef CONNECTION_MGR_H
#define CONNECTION_MGR_H

#include "util.h"

#define CONN_MAX_DEVICES 16

typedef enum {
    MGR_DISCONNECTED = 0,
    MGR_USB_DETECTED,
    MGR_ADB_AUTHORIZED,
    MGR_ADB_UNAUTHORIZED,
    MGR_FASTBOOT,
    MGR_RECOVERY_ADB,
    MGR_HID_READY,
    MGR_FAILED
} MgrState;

typedef struct {
    MgrState state;
    char state_name[32];
    char serial[64];
    char vendor[64];
    char model[64];
    char product[64];
    char manufacturer[64];
    char android_version[32];
    char sdk_version[16];
    char build_fingerprint[256];
    char bootloader[64];
    char baseband[64];
    char security_patch[32];
    char cpu_abi[64];
    char screen_size[32];
    char ram_total[32];
    char storage_total[32];
    char battery_level[16];
    char kernel_version[128];
    char lock_screen_type[32];
    char connection_log[4096];
    int log_len;
    int bus;
    int dev_num;
    int vid;
    int pid;
    int has_fastboot;
    int has_recovery_adb;
    int tries_adb;
    int tries_fastboot;
    int tries_recovery;
    int tries_usb_hid;
} ConnectionMgr;

void mgr_init(ConnectionMgr *cm);
void mgr_log(ConnectionMgr *cm, const char *fmt, ...);

/* Full auto-connect with fallback chain */
int mgr_auto_connect(ConnectionMgr *cm, int timeout_sec);

/* Individual mode attempts */
int mgr_try_adb(ConnectionMgr *cm);
int mgr_try_fastboot(ConnectionMgr *cm);
int mgr_try_recovery_adb(ConnectionMgr *cm);
int mgr_try_usb_info(ConnectionMgr *cm);
int mgr_try_usb_hid(ConnectionMgr *cm);

/* Info gathering without ADB */
int mgr_gather_usb_info(ConnectionMgr *cm);
int mgr_gather_fastboot_info(ConnectionMgr *cm);

/* Utility */
int mgr_scan_usb_devices(ConnectionMgr *cm);
int mgr_force_recovery(ConnectionMgr *cm);
void mgr_print_status(ConnectionMgr *cm);

#endif
