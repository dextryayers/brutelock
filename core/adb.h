#ifndef ADB_H
#define ADB_H

#ifdef __cplusplus
extern "C" {
#endif

int  adb_init(void);
int  adb_check(void);
int  adb_state(void);
int  adb_wait(int timeout);
int  adb_wait_any(int timeout);
void adb_recover(void);
char* adb_shell(const char *fmt, ...);
void adb_shell_noret(const char *fmt, ...);
char* adb_getprop(const char *prop);
int  adb_shell_works(void);
int  adb_enter_pin(const char *pin);
int  adb_enter_pin_sakti(const char *pin);
int  adb_is_unlocked(void);
void adb_lock_screen_info(char *out, int out_sz);
int  adb_screen_is_on(void);
void adb_lock_type_detect(char *out, int out_sz);
void adb_screen_on(void);
void adb_wakeup(void);
void adb_swipe(void);
void adb_swipe_smart(void);
int  adb_get_screen_w(void);
int  adb_get_screen_h(void);
void adb_digit_coords(char digit, int *x, int *y);
void adb_reboot(void);
void adb_reboot_recovery(void);
void adb_reboot_bootloader(void);

#ifdef __cplusplus
}
#endif

#endif
