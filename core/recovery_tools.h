#ifndef RECOVERY_TOOLS_H
#define RECOVERY_TOOLS_H

#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

int  recovery_check(void);
int  recovery_pull_db(const char *dest);
int  recovery_list_data(void);
char* recovery_getprop(const char *prop);
int  recovery_parse_db(const char *db_path, DeviceInfo *info);
int  recovery_extract_hash(const char *db_path, char *hash, int hash_sz);

#ifdef __cplusplus
}
#endif

#endif
