#ifndef CPU_H
#define CPU_H

#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

void detect_cpu(DeviceInfo *info);
void detect_cpu_deep(DeviceInfo *info);
void print_cpu_info(const DeviceInfo *info);

#ifdef __cplusplus
}
#endif

#endif
