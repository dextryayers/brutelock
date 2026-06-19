#ifndef PINCRACK_H
#define PINCRACK_H

#include "util.h"

int brute_smart(DeviceInfo *info, int digits, int delay_ms);
int brute_pattern(DeviceInfo *info, int delay_ms);
int check_lockout(DeviceInfo *info);

#endif
