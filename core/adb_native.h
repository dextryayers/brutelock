#ifndef ADB_NATIVE_H
#define ADB_NATIVE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Connection lifecycle */
int  adb_native_init(void);
int  adb_native_check(void);
int  adb_native_state(void);
void adb_native_close(void);

/* Shell */
int  adb_native_shell(const char *cmd, char *out, int out_sz);
int  adb_native_shell_noret(const char *cmd);
int  adb_native_getprop(const char *prop, char *out, int out_sz);

/* Input simulation */
int  adb_native_tap(int x, int y);
int  adb_native_swipe(int x1, int y1, int x2, int y2);
int  adb_native_keyevent(int code);
int  adb_native_text(const char *text);

/* Unlock check */
int  adb_native_is_unlocked(void);

/* RSa auth management */
void adb_native_set_keypath(const char *path);
int  adb_native_has_auth(void);

/* ── Low-level ADB service operations (for sync, etc.) ── */
int  adb_native_service_open(const char *service);
int  adb_native_service_write(const void *data, int len);
int  adb_native_service_read(void *data, int maxlen);
void adb_native_service_close(void);

/* ── Advanced ADB operations ── */
int  adb_get_device_fd(void);
int  adb_get_in_ep(void);
int  adb_get_out_ep(void);
int  adb_native_detect_endpoints(void);
int  adb_native_write_raw(const unsigned char *data, int len);
int  adb_native_shell_batch(const char **cmds, int ncmds, char *out, int out_sz);
int  adb_native_shell_timeout(const char *cmd, char *out, int out_sz, int timeout_ms);
int  adb_native_input_text_fast(const char *text);
int  adb_native_usb_control(int request, int value, int index, unsigned char *data, int len);

#ifdef __cplusplus
}
#endif

#endif
