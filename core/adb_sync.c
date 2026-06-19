#include "adb_sync.h"
#include "adb_native.h"
#include "util.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include <errno.h>

/* ── SYNC protocol constants ── */
#define SYNC_DATA 0x41544144  /* "DATA" */
#define SYNC_DENT 0x544e4544  /* "DENT" */
#define SYNC_DONE 0x454e4f44  /* "DONE" */
#define SYNC_FAIL 0x4c494146  /* "FAIL" */
#define SYNC_LIST 0x5453494c  /* "LIST" */
#define SYNC_OKAY 0x59414b4f  /* "OKAY" */
#define SYNC_RECV 0x56434552  /* "RECV" */
#define SYNC_SEND 0x444e4553  /* "SEND" */
#define SYNC_STAT 0x54415453  /* "STAT" */

/* Pack 4-byte command */
static void put32(unsigned char *b, uint32_t v) {
    b[0] = v & 0xFF; b[1] = (v>>8) & 0xFF;
    b[2] = (v>>16) & 0xFF; b[3] = (v>>24) & 0xFF;
}

static uint32_t get32(const unsigned char *b) {
    return b[0] | (b[1]<<8) | (b[2]<<16) | (b[3]<<24);
}

/* ── Send a SYNC request and receive response ── */
static int sync_cmd(const char cmd[4], const void *data, int dlen,
                    unsigned char *resp, int rmax) {
    unsigned char buf[8 + dlen];
    memcpy(buf, cmd, 4);
    put32(buf+4, dlen);
    if (dlen > 0 && data) memcpy(buf+8, data, dlen);

    if (adb_native_service_write(buf, 8+dlen) < 0) return -1;
    int r = adb_native_service_read(resp, rmax);
    return r;
}

/* ── List directory ── */
int adb_sync_list(const char *remote_dir, char *output, int out_sz) {
    if (!remote_dir || !output) return -1;

    if (adb_native_service_open("sync:") < 0) return -1;

    unsigned char buf[4096];
    int dlen = strlen(remote_dir) + 1;
    int r = sync_cmd("LIST", remote_dir, dlen, buf, sizeof(buf));
    if (r < 8) { adb_native_service_close(); return -1; }

    int entries = 0;
    int pos = 0;
    while (r >= 8) {
        uint32_t cmd = get32(buf);
        uint32_t len = get32(buf+4);

        if (cmd == SYNC_DONE) break;
        if (cmd == SYNC_FAIL) { adb_native_service_close(); return -1; }

        if (cmd == SYNC_DENT && len >= 16 && len < sizeof(buf) && r >= 8 + (int)len) {
            /* DENT: mode(4) + size(4) + time(4) + name_len(4) + name */
            unsigned int name_len = get32(buf+16);
            if (8+len <= r && pos + name_len + 2 < out_sz) {
                pos += snprintf(output+pos, out_sz-pos, "%.*s\n",
                                name_len, buf+20);
                entries++;
            }
        }

        /* Read next */
        r = adb_native_service_read(buf, sizeof(buf));
    }

    output[pos] = 0;
    adb_native_service_close();
    return entries;
}

/* ── Pull file ── */
int adb_sync_pull(const char *remote_path, unsigned char *data, int maxlen) {
    if (!remote_path || !data) return -1;

    if (adb_native_service_open("sync:") < 0) return -1;

    unsigned char buf[4096];
    int dlen = strlen(remote_path) + 1;
    int r = sync_cmd("RECV", remote_path, dlen, buf, sizeof(buf));
    if (r < 8) { adb_native_service_close(); return -1; }

    int total = 0;
    while (1) {
        if (r < 8) break;
        uint32_t cmd = get32(buf);
        uint32_t len = get32(buf+4);

        if (cmd == SYNC_DONE) break;
        if (cmd == SYNC_FAIL) { total = -1; break; }

        if (cmd == SYNC_DATA && len > 0) {
            /* DATA: copy payload */
            /* Payload is in buf+8, but we read resp into buf starting at 0 */
            /* Actually our read returns full response. Payload is at offset 8 */
            int payload = r - 8;
            if (payload > (int)len) payload = len;
            if (total + payload <= maxlen) {
                memcpy(data + total, buf + 8, payload);
                total += payload;
            }
        }

        r = adb_native_service_read(buf, sizeof(buf));
    }

    adb_native_service_close();
    return total;
}

/* ── Pull to local file ── */
int adb_sync_pull_file(const char *remote_path, const char *local_path) {
    if (!remote_path || !local_path) return -1;

    unsigned char buf[131072]; /* 128K buffer */
    int n = adb_sync_pull(remote_path, buf, sizeof(buf));
    if (n <= 0) return -1;

    FILE *f = fopen(local_path, "wb");
    if (!f) return -1;
    size_t wrote = fwrite(buf, 1, n, f);
    fclose(f);
    return wrote > 0 ? 0 : -1;
}

/* ── Push data ── */
int adb_sync_push_data(const unsigned char *data, int len,
                        const char *remote_path) {
    if (!data || !remote_path || len < 0) return -1;

    if (adb_native_service_open("sync:") < 0) return -1;

    /* SEND: path + "," + octal mode */
    char rpath[1024];
    snprintf(rpath, sizeof(rpath), "%s,0644", remote_path);
    int dlen = strlen(rpath) + 1;
    unsigned char buf[4096];
    int r = sync_cmd("SEND", rpath, dlen, buf, sizeof(buf));
    if (r < 8) { adb_native_service_close(); return -1; }

    /* Send data in chunks via DATA */
    int sent = 0;
    while (sent < len) {
        int chunk = (len - sent > 4096) ? 4096 : (len - sent);
        unsigned char pkt[8 + chunk];
        put32(pkt, SYNC_DATA);
        put32(pkt+4, chunk);
        memcpy(pkt+8, data + sent, chunk);
        if (adb_native_service_write(pkt, 8+chunk) < 0) break;
        sent += chunk;
    }
    if (sent < len) { adb_native_service_close(); return -1; }

    /* Send DONE with timestamp */
    unsigned char done[8];
    put32(done, SYNC_DONE);
    put32(done+4, (unsigned int)time(0));
    adb_native_service_write(done, 8);

    /* Read OKAY/FAIL */
    r = adb_native_service_read(buf, sizeof(buf));
    adb_native_service_close();
    return 0;
}

/* ── Push local file ── */
int adb_sync_push(const char *local_path, const char *remote_path) {
    if (!local_path || !remote_path) return -1;

    FILE *f = fopen(local_path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (len <= 0) { fclose(f); return -1; }

    unsigned char *buf = malloc(len);
    if (!buf) { fclose(f); return -1; }
    size_t n = fread(buf, 1, len, f);
    fclose(f);
    if (n != (size_t)len) { free(buf); return -1; }

    int r = adb_sync_push_data(buf, len, remote_path);
    free(buf);
    return r;
}

/* ── Stat ── */
int adb_sync_stat(const char *remote_path, unsigned int *size,
                   unsigned int *mode) {
    if (!remote_path) return -1;

    if (adb_native_service_open("sync:") < 0) return -1;

    unsigned char buf[64];
    int dlen = strlen(remote_path) + 1;
    int r = sync_cmd("STAT", remote_path, dlen, buf, sizeof(buf));
    if (r < 20) { adb_native_service_close(); return -1; }

    if (get32(buf) != SYNC_STAT) { adb_native_service_close(); return -1; }

    if (mode) *mode = get32(buf+4+4);  /* skip length, then mode */
    if (size) *size = get32(buf+4+4+4); /* skip length, mode */

    adb_native_service_close();
    return 0;
}

/* ── Check if path exists ── */
int adb_sync_exists(const char *remote_path) {
    return adb_sync_stat(remote_path, NULL, NULL) == 0 ? 1 : 0;
}

/* ── Pull locksettings.db from recovery ── */
int adb_sync_pull_locksettings(const char *local_dir) {
    if (!local_dir) local_dir = ".";

    /* Check typical locksettings paths */
    const char *paths[] = {
        "/data/system/locksettings.db",
        "/data/misc/keystore/locksettings.db",
        "/data/system_de/0/locksettings.db",
        "/data/system_ce/0/locksettings.db",
        NULL
    };

    int found = 0;
    for (int i = 0; paths[i]; i++) {
        if (adb_sync_exists(paths[i])) {
            char local[1024];
            /* Extract filename from path */
            const char *fname = strrchr(paths[i], '/');
            if (!fname) fname = paths[i]; else fname++;

            if (strcmp(local_dir, ".") == 0)
                snprintf(local, sizeof(local), "%s", fname);
            else
                snprintf(local, sizeof(local), "%s/%s", local_dir, fname);

            printf("[*] Pulling %s -> %s\n", paths[i], local);
            if (adb_sync_pull_file(paths[i], local) == 0) {
                printf(GREEN "[+] Pulled: %s\n" RESET, local);
                found++;

                /* Also pull WAL and SHM if they exist */
                char wal_path[1024], shm_path[1024];
                snprintf(wal_path, sizeof(wal_path), "%s-wal", paths[i]);
                snprintf(shm_path, sizeof(shm_path), "%s-shm", paths[i]);
                if (adb_sync_exists(wal_path)) {
                    char local_wal[1024];
                    snprintf(local_wal, sizeof(local_wal), "%s/%s-wal", local_dir, fname);
                    adb_sync_pull_file(wal_path, local_wal);
                }
                if (adb_sync_exists(shm_path)) {
                    char local_shm[1024];
                    snprintf(local_shm, sizeof(local_shm), "%s/%s-shm", local_dir, fname);
                    adb_sync_pull_file(shm_path, local_shm);
                }
            } else {
                printf(RED "[-] Failed to pull: %s\n" RESET, paths[i]);
            }
        }
    }
    return found;
}
