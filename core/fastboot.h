#ifndef FASTBOOT_H
#define FASTBOOT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Fastboot device info */
typedef struct {
    int      present;
    char     serial[128];
    char     product[256];
    char     variant[128];
    char     battery_voltage[64];
    char     battery_soc[64];      /* state of charge */
    char     unlocked[64];
    char     version[64];
    char     max_download[64];
    char     soc[128];
    char     mid[128];             /* manufacturer ID */
    char     cid[128];             /* carrier/region ID */
    char     secure[64];
    char     off_mode_charge[64];
    char     slot_count[64];
    char     current_slot[64];
    char     has_slot[64];
    char     is_userspace[64];
    char     hw_revision[128];
    char     board_serial[128];
    char     display_panel[128];
    char     fingerprint[256];     /* build fingerprint */
} FastbootInfo;

/* Initialize: find and claim fastboot USB interface */
int fastboot_init(void);

/* Check if fastboot connection is active */
int fastboot_check(void);

/* Low-level: send command, receive full response (status + message).
   Returns: 0=OKAY, -1=FAIL, -2=IO error. Response string stored in out/out_sz. */
int fastboot_command(const char *cmd, char *out, int out_sz);

/* Low-level: send command, receive multiple INFO lines + final status.
   Returns number of INFO lines, final status in out. */
int fastboot_command_info(const char *cmd, char *out, int out_sz);

/* Get a single variable */
int fastboot_getvar(const char *var, char *out, int out_sz);

/* Populate FastbootInfo with all variables */
int fastboot_getvar_all(FastbootInfo *info);

/* Detect fastboot device without claiming (just check USB descriptors) */
int fastboot_detect(FastbootInfo *info);

/* Operations */
int fastboot_reboot(void);
int fastboot_reboot_bootloader(void);
int fastboot_continue(void);
int fastboot_flash(const char *partition, const unsigned char *data, int len);
int fastboot_erase(const char *partition);
int fastboot_boot(const unsigned char *kernel_data, int len);
int fastboot_oem_unlock(void);
int fastboot_flashing_unlock(void);
int fastboot_oem_device_info(char *out, int out_sz);

/* Close connection */
void fastboot_close(void);

/* Human-readable status string */
const char* fastboot_status_str(void);

/* Format device info into buffer for display */
void fastboot_get_info(char *buf, int sz);

#ifdef __cplusplus
}
#endif

#endif
