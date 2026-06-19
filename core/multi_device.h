#ifndef MULTI_DEVICE_H
#define MULTI_DEVICE_H

#ifdef __cplusplus
extern "C" {
#endif

int  mdev_scan(void);
int  mdev_count(void);
const char* mdev_serial(int idx);
int  mdev_select(int idx);
int  mdev_try_all(int (*fn)(const char *serial));

#ifdef __cplusplus
}
#endif

#endif
