#ifndef PREDICTOR_H
#define PREDICTOR_H

#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

int  predictor_generate(const DeviceInfo *info, int digits, char pins[][16], int max);
void predictor_phase_common(const DeviceInfo *info, int digits, char pins[][16], int *count, int max);
void predictor_phase_date(const DeviceInfo *info, int digits, char pins[][16], int *count, int max);
void predictor_phase_device(const DeviceInfo *info, int digits, char pins[][16], int *count, int max);

#ifdef __cplusplus
}
#endif

#endif
