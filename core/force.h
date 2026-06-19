#ifndef FORCE_H
#define FORCE_H

#ifdef __cplusplus
extern "C" {
#endif

int force_connect(int offer_recovery);
int force_try_recovery(void);
int force_try_fastboot(void);
int force_is_recovery(void);
int force_is_fastboot(void);

#ifdef __cplusplus
}
#endif

#endif
