#ifndef CHIP_H
#define CHIP_H

#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

void detect_chip(DeviceInfo *info);
void print_chip_info(const DeviceInfo *info);

#ifdef __cplusplus
}
#endif

#endif
