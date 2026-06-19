#include "db_parser.h"
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>

static const uint8_t MAGIC_SP[] = {'s', 'p', '_'};
static const uint8_t MAGIC_SALT[] = {'S', 'A', 'L', 'T'};

static bool find_in_buf(const uint8_t *buf, size_t len,
                        const uint8_t *pattern, size_t plen,
                        size_t &offset, size_t &end) {
    for (size_t i = 0; i < len - plen; i++) {
        if (memcmp(buf + i, pattern, plen) == 0) {
            offset = i;
            end = i + plen;
            while (end < len && buf[end] != '\n' && buf[end] != 0) end++;
            return true;
        }
    }
    return false;
}

extern "C" int db_parse_locksettings(const char *path, char *hash, int hash_sz, int *pin_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize <= 0) { fclose(f); return 0; }

    std::vector<uint8_t> buf(fsize);
    fread(buf.data(), 1, fsize, f);
    fclose(f);

    size_t off, end;
    hash[0] = 0;
    if (pin_len) *pin_len = 0;

    if (find_in_buf(buf.data(), fsize, MAGIC_SP, 3, off, end)) {
        size_t n = end - off;
        if (n < (size_t)hash_sz) {
            memcpy(hash, buf.data() + off, n);
            hash[n] = 0;
        }
    }

    if (find_in_buf(buf.data(), fsize, MAGIC_SALT, 4, off, end)) {
        size_t n = end - off;
        char salt_str[256];
        n = (n < 255) ? n : 255;
        memcpy(salt_str, buf.data() + off, n);
        salt_str[n] = 0;
        if (strlen(hash) > 0) {
            snprintf(hash + strlen(hash), hash_sz - strlen(hash), "|SALT:%s", salt_str);
        }
    }

    const char *types[] = {"password_type=", "lockscreen.password_type="};
    for (auto t : types) {
        const uint8_t *p = (const uint8_t*)t;
        size_t plen = strlen(t);
        if (find_in_buf(buf.data(), fsize, p, plen, off, end)) {
            char val[64];
            size_t n = end - off - plen;
            n = (n < 63) ? n : 63;
            memcpy(val, buf.data() + off + plen, n);
            val[n] = 0;
            int ptype = atoi(val);
            if (pin_len) {
                if (ptype == 65536) *pin_len = 4;
                else if (ptype == 131072 || ptype == 196608) *pin_len = 6;
                else if (ptype == 262144) *pin_len = 8;
                else *pin_len = 4;
            }
            break;
        }
    }

    return strlen(hash) > 0 ? 1 : 0;
}

extern "C" int db_extract_sp(const char *path, char *out, int out_sz) {
    return db_parse_locksettings(path, out, out_sz, NULL);
}
