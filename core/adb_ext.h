#ifndef ADB_EXT_H
#define ADB_EXT_H

int  adb_find_device(void);
int  adb_force_recovery(void);
int  adb_force_fastboot(void);
int  adb_is_fastboot(void);
char* adb_fastboot_cmd(const char *cmd);
int  adb_authorize_loop(int timeout);
void adb_list_devices(void);

#endif
