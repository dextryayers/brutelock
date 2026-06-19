#ifndef DB_PARSER_H
#define DB_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

int  db_parse_locksettings(const char *path, char *hash, int hash_sz, int *pin_len);
int  db_extract_sp(const char *path, char *out, int out_sz);

#ifdef __cplusplus
}
#endif

#endif
