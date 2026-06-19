#ifndef ENGINE_CONNECT_H
#define ENGINE_CONNECT_H

#include "util.h"
#include "adb.h"
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/*  Connection state machine */
typedef enum {
    CONN_DISCONNECTED = 0,
    CONN_USB_DETECTED,
    CONN_ADB_INIT,
    CONN_UNAUTHORIZED,
    CONN_AUTHORIZED,
    CONN_READY,
    CONN_LOST
} ConnState;

typedef struct {
    ConnState state;
    int adb_state;
    int retry_count;
    int screen_width;
    int screen_height;
    char vendor[64];
    char model[128];
    char serial[64];
    char lock_type[32];
    int locked;
    pthread_mutex_t lock;
} ConnInfo;

/*  Initialize connection engine */
void conn_init(ConnInfo *ci);

/*  Scan USB, auto-detect Android device, return 1 on USB found */
int conn_scan_usb(ConnInfo *ci);

/*  Wait for USB device with timeout (seconds), return 1 when found */
int conn_wait_usb(ConnInfo *ci, int timeout_s);

/*  Initialize ADB (native USB), return 1 on success */
int conn_init_adb(ConnInfo *ci);

/*  Full auto-connect: scan USB → init ADB → wait → return state */
int conn_auto(ConnInfo *ci, int timeout_s);

/*  Check and update connection state */
void conn_poll(ConnInfo *ci);

/*  Get display-ready connection status string */
void conn_status_str(ConnInfo *ci, char *out, int out_sz);

/*  Close ADB and reset */
void conn_close(ConnInfo *ci);

#ifdef __cplusplus
}
#endif
#endif
