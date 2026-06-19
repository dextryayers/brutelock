#ifndef BRUTE_ADV_H
#define BRUTE_ADV_H

#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

int brute_adv(int digits, int delay_ms, long start, long end, int resume, int skip_confirm);
int brute_adv_phase(const DeviceInfo *info, int digits, int delay_ms);

#ifdef __cplusplus
}
#endif

#endif
