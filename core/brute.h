#ifndef BRUTE_H
#define BRUTE_H

#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

int brute_pin(int digits, int delay_ms, long start, long end, int resume, int skip_confirm);
int brute_wordlist(const char *path, int delay_ms, long start, long end, int resume);
int brute_smart(DeviceInfo *info, int digits, int delay_ms);

#ifdef __cplusplus
}
#endif

#endif
