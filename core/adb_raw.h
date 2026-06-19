#ifndef ADB_RAW_H
#define ADB_RAW_H

#ifdef __cplusplus
extern "C" {
#endif

char* adb_get_input_devs(void);
int   adb_find_input_dev(void);
int   adb_reliable_event_dev(void);
int   adb_raw_touch(int x, int y);
int   adb_raw_touch_slot(int slot, int x, int y);
int   adb_raw_swipe(int x1, int y1, int x2, int y2, int steps);
int   adb_raw_key(int code);
int   adb_raw_multitap(const int xy[][2], int num_points);

#ifdef __cplusplus
}
#endif

#endif
