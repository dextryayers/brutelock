#define _GNU_SOURCE
#include "adb_native.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <linux/usb/ch9.h>
#include <linux/usbdevice_fs.h>

/* ── ADB protocol constants ── */
#define ADB_VERSION   0x01000001
#define ADB_MAXDATA   4096

#define A_CNXN 0x4e584e43
#define A_OPEN 0x4e45504f
#define A_OKAY 0x59414b4f
#define A_WRTE 0x45545257
#define A_CLSE 0x45534c43
#define A_AUTH 0x48545541
#define A_STLS 0x534c5453

#define ADB_AUTH_TOKEN       1
#define ADB_AUTH_SIGNATURE   2
#define ADB_AUTH_RSAPUBLICKEY 3

typedef struct __attribute__((packed)) {
    uint32_t command;
    uint32_t arg0;
    uint32_t arg1;
    uint32_t data_length;
    uint32_t data_crc32;
    uint32_t magic;
} AdbPacket;

/* ── global state ── */
static struct {
    UsbDevice dev;
    int connected;
    int authorized;
    int local_id;
    char ident[256];
    char key_path[512];
    pthread_mutex_t lock;
} g_adb = { .lock = PTHREAD_MUTEX_INITIALIZER };

/* ── CRC32 ── */
static uint32_t adb_crc32(const uint8_t *d, int len) {
    uint32_t c = 0xFFFFFFFF;
    for (int i = 0; i < len; i++) {
        c ^= d[i];
        for (int j = 0; j < 8; j++)
            c = (c >> 1) ^ (c & 1 ? 0xEDB88320 : 0);
    }
    return ~c;
}

/* ── USB I/O ── */
static int adb_send(uint32_t cmd, uint32_t a0, uint32_t a1,
                    const void *data, int dlen) {
    uint8_t *pkt = malloc(sizeof(AdbPacket) + dlen + 1);
    if (!pkt) return -1;
    AdbPacket *h = (AdbPacket*)pkt;
    h->command = cmd; h->arg0 = a0; h->arg1 = a1;
    h->data_length = dlen;
    h->data_crc32 = dlen ? adb_crc32(data, dlen) : 0;
    h->magic = cmd ^ 0xFFFFFFFF;
    if (dlen && data) memcpy(pkt + sizeof(AdbPacket), data, dlen);
    int ret = platform_usb_write(&g_adb.dev, g_adb.dev.ep_out,
                                  pkt, sizeof(AdbPacket) + dlen, 5000);
    free(pkt);
    return ret < 0 ? -1 : 0;
}

static int adb_recv(uint8_t *payload, int maxpl) {
    uint8_t buf[sizeof(AdbPacket) + maxpl];
    int ret = platform_usb_read(&g_adb.dev, g_adb.dev.ep_in, buf,
                                 sizeof(buf), 10000);
    if (ret < (int)sizeof(AdbPacket)) return -1;
    AdbPacket *h = (AdbPacket*)buf;
    int dlen = h->data_length;
    if (dlen > maxpl) dlen = maxpl;
    if (dlen && payload) memcpy(payload, buf + sizeof(AdbPacket), dlen);
    if (h->command == A_AUTH) return -2;
    if (h->command == A_WRTE) return 1;
    if (h->command == A_CLSE) return 2;
    return 0;
}

/* ── RSA key management (simple embedded approach) ── */
/* We generate a key at first run, store in key_path (~/.android/adbkey) */
/* For simplicity, we use OpenSSL if available, else fallback */

static int rsa_sign(const uint8_t *token, int token_len,
                    uint8_t *sig, int *sig_len) {
    /* Use pre-generated embedded keypair for simplicity */
    /* In production, generate and store in ~/.android/adbkey */
    /* For now, we just copy the token as a placeholder signature */
    /* Real implementation would use RSA_sign() from OpenSSL */
    if (*sig_len < token_len) return -1;
    memcpy(sig, token, token_len);
    *sig_len = token_len;
    return 0;
}

static int rsa_get_public_key(char *buf, int bufsz) {
    char rsa_path[640];
    if (g_adb.key_path[0]) {
        snprintf(rsa_path, sizeof(rsa_path), "%s.pub", g_adb.key_path);
        FILE *f = fopen(rsa_path, "r");
        if (f) {
            size_t n = fread(buf, 1, bufsz-1, f);
            buf[n] = 0;
            fclose(f);
            return 0;
        }
    }
    /* Fallback: embedded minimal public key */
    snprintf(buf, bufsz, "BrutePin::auto-generated-key "
             "QAAAAbrutepin@android 0 0\n");
    return 0;
}

void adb_native_set_keypath(const char *path) {
    if (path) strncpy(g_adb.key_path, path, sizeof(g_adb.key_path)-1);
}

int adb_native_has_auth(void) {
    return g_adb.authorized;
}

/* ── Init / connect ── */
int adb_native_init(void) {
    if (g_adb.dev.fd >= 0 && g_adb.connected) return 1;

    /* Close any stale connection */
    if (g_adb.dev.fd >= 0) platform_usb_close(&g_adb.dev);
    memset(&g_adb.dev, 0, sizeof(g_adb.dev));
    g_adb.dev.fd = -1;
    g_adb.connected = 0;
    g_adb.authorized = 0;

    /* Find ADB USB device */
    if (!platform_usb_find_adb(&g_adb.dev)) return 0;

    g_adb.local_id = 1;

    /* Send CNXN (connect) */
    char id[256];
    snprintf(id, sizeof(id), "BrutePin::host::native");
    if (adb_send(A_CNXN, ADB_VERSION, ADB_MAXDATA, id, strlen(id) + 1) < 0) {
        platform_usb_close(&g_adb.dev);
        return 0;
    }

    /* Receive response */
    uint8_t payload[ADB_MAXDATA];
    int r = adb_recv(payload, sizeof(payload));

    if (r == -2) {
        /* A_AUTH received – device needs authorization */
        /* Extract token from payload */
        uint8_t *token = payload;
        int tlen = 0;
        /* Find token length – auth payload format: 20 bytes header? */
        /* Real implementation: parse A_AUTH packet properly */
        tlen = 20; /* typical token size */

        uint8_t sig[256];
        int siglen = sizeof(sig);
        if (rsa_sign(token, tlen, sig, &siglen) == 0) {
            /* Send signature response */
            adb_send(A_AUTH, ADB_AUTH_SIGNATURE, 0, sig, siglen);
            usleep(100000);
            r = adb_recv(payload, sizeof(payload));
        }

        if (r == -2) {
            /* Still unauthorized – send public key */
            char pubkey[2048];
            if (rsa_get_public_key(pubkey, sizeof(pubkey)) == 0) {
                adb_send(A_AUTH, ADB_AUTH_RSAPUBLICKEY, 0,
                         pubkey, strlen(pubkey) + 1);
                usleep(200000);
                r = adb_recv(payload, sizeof(payload));
            }
        }

        if (r < 0) {
            /* Authorization failed — device still unauthorized */
            g_adb.authorized = 0;
            g_adb.connected = 1; /* we can still detect it */
            return 1;
        }
        g_adb.authorized = 1;
    }

    if (r == 0) {
        /* Got CNXN response – authorized */
        g_adb.authorized = 1;
    }

    g_adb.connected = 1;
    return 1;
}

int adb_native_check(void) {
    return g_adb.dev.fd >= 0 && g_adb.connected;
}

int adb_native_state(void) {
    if (g_adb.dev.fd < 0) return 0;
    if (!g_adb.authorized) return 1; /* unauthorized */
    return 2; /* device */
}

void adb_native_close(void) {
    platform_usb_close(&g_adb.dev);
    memset(&g_adb, 0, sizeof(g_adb));
    g_adb.dev.fd = -1;
}

/* ── Shell ── */
int adb_native_shell(const char *cmd, char *out, int out_sz) {
    if (g_adb.dev.fd < 0 || !g_adb.authorized) return -1;
    pthread_mutex_lock(&g_adb.lock);

    int lid = g_adb.local_id++;
    char svc[256];
    int svclen = snprintf(svc, sizeof(svc), "shell:%s", cmd);
    if (svclen >= (int)sizeof(svc)) svclen = sizeof(svc) - 1;

    if (adb_send(A_OPEN, lid, 0, svc, svclen + 1) < 0) {
        pthread_mutex_unlock(&g_adb.lock);
        return -1;
    }

    out[0] = 0;
    int pos = 0;
    while (1) {
        uint8_t buf[sizeof(AdbPacket) + ADB_MAXDATA];
        int ret = platform_usb_read(&g_adb.dev, g_adb.dev.ep_in, buf,
                                     sizeof(buf), 10000);
        if (ret < (int)sizeof(AdbPacket)) break;
        AdbPacket *h = (AdbPacket*)buf;
        int dlen = h->data_length;
        if (dlen > ADB_MAXDATA) dlen = ADB_MAXDATA;
        if (h->command == A_CLSE) break;
        if (h->command == A_WRTE && dlen > 0) {
            if (pos + dlen < out_sz - 1) {
                memcpy(out + pos, buf + sizeof(AdbPacket), dlen);
                pos += dlen;
                out[pos] = 0;
            }
        }
        adb_send(A_OKAY, 0, lid, NULL, 0);
    }

    pthread_mutex_unlock(&g_adb.lock);
    return pos > 0 ? 0 : -1;
}

int adb_native_shell_noret(const char *cmd) {
    char buf[128];
    return adb_native_shell(cmd, buf, sizeof(buf));
}

int adb_native_getprop(const char *prop, char *out, int out_sz) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "getprop %s", prop);
    return adb_native_shell(cmd, out, out_sz);
}

/* ── Input ── */
int adb_native_tap(int x, int y) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "input tap %d %d", x, y);
    return adb_native_shell_noret(cmd);
}

int adb_native_swipe(int x1, int y1, int x2, int y2) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "input swipe %d %d %d %d", x1, y1, x2, y2);
    return adb_native_shell_noret(cmd);
}

int adb_native_keyevent(int code) {
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "input keyevent %d", code);
    return adb_native_shell_noret(cmd);
}

int adb_native_text(const char *text) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "input text '%s'", text);
    return adb_native_shell_noret(cmd);
}

/* ── Unlock check ── */
int adb_native_is_unlocked(void) {
    char out[4096];
    if (adb_native_shell("dumpsys window 2>/dev/null | grep -i 'mKeyguardShowing' | head -1",
                         out, sizeof(out)) == 0) {
        if (strstr(out, "mKeyguardShowing=false")) return 1;
    }
    return 0;
}

/* ── Low-level service operations ── */

static int g_svc_id = 0;      /* local_id for the service */
static int g_svc_open = 0;

int adb_native_service_open(const char *service) {
    if (g_adb.dev.fd < 0 || !g_adb.authorized) return -1;
    pthread_mutex_lock(&g_adb.lock);

    g_svc_id = g_adb.local_id++;
    if (adb_send(A_OPEN, g_svc_id, 0, service, strlen(service) + 1) < 0) {
        pthread_mutex_unlock(&g_adb.lock);
        return -1;
    }

    /* Receive OKAY */
    uint8_t pl[ADB_MAXDATA];
    int r = adb_recv(pl, sizeof(pl));
    if (r < 0) {
        pthread_mutex_unlock(&g_adb.lock);
        return -1;
    }

    g_svc_open = 1;
    pthread_mutex_unlock(&g_adb.lock);
    return g_svc_id;
}

int adb_native_service_write(const void *data, int len) {
    if (!g_svc_open) return -1;
    pthread_mutex_lock(&g_adb.lock);
    int r = adb_send(A_WRTE, g_svc_id, 0, data, len);
    if (r < 0) { pthread_mutex_unlock(&g_adb.lock); return -1; }
    /* Wait for OKAY */
    uint8_t pl[ADB_MAXDATA];
    adb_recv(pl, sizeof(pl));
    pthread_mutex_unlock(&g_adb.lock);
    return len;
}

int adb_native_service_read(void *data, int maxlen) {
    if (!g_svc_open) return -1;
    pthread_mutex_lock(&g_adb.lock);
    uint8_t pl[ADB_MAXDATA];
    int r = adb_recv(pl, sizeof(pl));
    if (r < 0) { pthread_mutex_unlock(&g_adb.lock); return -1; }
    int dlen = r == 1 ? strlen((char*)pl) : 0;
    if (dlen > maxlen) dlen = maxlen;
    if (dlen > 0) memcpy(data, pl, dlen);
    adb_send(A_OKAY, 0, g_svc_id, NULL, 0);
    pthread_mutex_unlock(&g_adb.lock);
    return dlen;
}

void adb_native_service_close(void) {
    if (!g_svc_open) return;
    pthread_mutex_lock(&g_adb.lock);
    adb_send(A_CLSE, g_svc_id, 0, NULL, 0);
    g_svc_open = 0;
    pthread_mutex_unlock(&g_adb.lock);
}

/* ── Endpoint accessors ── */
int adb_get_device_fd(void) {
    return g_adb.dev.fd;
}

int adb_get_in_ep(void) {
    return g_adb.dev.ep_in;
}

int adb_get_out_ep(void) {
    return g_adb.dev.ep_out;
}

/* ── USB endpoint auto-detection from descriptors ── */
int adb_native_detect_endpoints(void) {
    if (g_adb.dev.fd < 0) return -1;
    unsigned char buf[1024];
    struct usbdevfs_ctrltransfer ctrl = {
        .bRequestType = USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
        .bRequest = USB_REQ_GET_DESCRIPTOR,
        .wValue = (USB_DT_CONFIG << 8) | 0,
        .wIndex = 0,
        .wLength = sizeof(buf),
        .data = buf,
        .timeout = 5000
    };
    int ret = ioctl(g_adb.dev.fd, USBDEVFS_CONTROL, &ctrl);
    if (ret < 12) return -1;
    g_adb.dev.ep_in = 0;
    g_adb.dev.ep_out = 0;
    int pos = buf[0];
    while (pos + 1 < ret && pos < (int)sizeof(buf)) {
        int dlen = buf[pos];
        int dtype = buf[pos + 1];
        if (dlen < 2) break;
        if (dtype == USB_DT_INTERFACE && dlen >= 9) {
            if (buf[pos+5] == 0xFF && buf[pos+6] == 0x42 && buf[pos+7] == 0x01) {
                pos += dlen;
                while (pos + 1 < ret && pos < (int)sizeof(buf)) {
                    int edlen = buf[pos];
                    int etype = buf[pos + 1];
                    if (edlen < 2 || etype == USB_DT_INTERFACE) break;
                    if (etype == USB_DT_ENDPOINT && edlen >= 7) {
                        int ep_addr = buf[pos+2];
                        int ep_attr = buf[pos+3];
                        if ((ep_attr & 0x03) == USB_ENDPOINT_XFER_BULK) {
                            if (ep_addr & USB_ENDPOINT_DIR_MASK)
                                g_adb.dev.ep_in = ep_addr & 0x0F;
                            else
                                g_adb.dev.ep_out = ep_addr & 0x0F;
                        }
                    }
                    pos += edlen;
                }
                break;
            }
        }
        if (pos + dlen >= ret) break;
        pos += dlen;
    }
    return (g_adb.dev.ep_in && g_adb.dev.ep_out) ? 0 : -1;
}

/* ── Raw ADB USB bulk write (no ADB framing) ── */
int adb_native_write_raw(const unsigned char *data, int len) {
    if (g_adb.dev.fd < 0) return -1;
    pthread_mutex_lock(&g_adb.lock);
    int ret = platform_usb_write(&g_adb.dev, g_adb.dev.ep_out, data, len, 5000);
    pthread_mutex_unlock(&g_adb.lock);
    return ret;
}

/* ── Batch shell: send multiple commands in a single shell session ── */
int adb_native_shell_batch(const char **cmds, int ncmds, char *out, int out_sz) {
    if (g_adb.dev.fd < 0 || !g_adb.authorized || !cmds || ncmds < 1) return -1;
    pthread_mutex_lock(&g_adb.lock);

    char combined[ADB_MAXDATA];
    int cpos = 0;
    for (int i = 0; i < ncmds; i++) {
        int slen = strlen(cmds[i]);
        if (cpos + slen + 3 > (int)sizeof(combined)) break;
        if (cpos > 0) { combined[cpos++] = ';'; combined[cpos++] = ' '; }
        memcpy(combined + cpos, cmds[i], slen);
        cpos += slen;
    }
    if (cpos == 0) { pthread_mutex_unlock(&g_adb.lock); return -1; }
    combined[cpos] = 0;

    char svc[288];
    int svclen = snprintf(svc, sizeof(svc), "shell:%s", combined);
    int lid = g_adb.local_id++;

    if (adb_send(A_OPEN, lid, 0, svc, svclen + 1) < 0) {
        pthread_mutex_unlock(&g_adb.lock);
        return -1;
    }

    out[0] = 0;
    int outpos = 0;
    while (1) {
        uint8_t buf[sizeof(AdbPacket) + ADB_MAXDATA];
        int ret = platform_usb_read(&g_adb.dev, g_adb.dev.ep_in, buf,
                                     sizeof(buf), 10000);
        if (ret < (int)sizeof(AdbPacket)) break;
        AdbPacket *h = (AdbPacket*)buf;
        int dlen = h->data_length;
        if (dlen > ADB_MAXDATA) dlen = ADB_MAXDATA;
        if (h->command == A_CLSE) break;
        if (h->command == A_WRTE && dlen > 0) {
            if (outpos + dlen < out_sz - 1) {
                memcpy(out + outpos, buf + sizeof(AdbPacket), dlen);
                outpos += dlen;
                out[outpos] = 0;
            }
        }
        adb_send(A_OKAY, 0, lid, NULL, 0);
    }

    pthread_mutex_unlock(&g_adb.lock);
    return outpos > 0 ? 0 : -1;
}

/* ── ADB shell with poll() timeout ── */
int adb_native_shell_timeout(const char *cmd, char *out, int out_sz, int timeout_ms) {
    if (g_adb.dev.fd < 0 || !g_adb.authorized) return -1;
    pthread_mutex_lock(&g_adb.lock);

    int lid = g_adb.local_id++;
    char svc[256];
    int svclen = snprintf(svc, sizeof(svc), "shell:%s", cmd);
    if (svclen >= (int)sizeof(svc)) svclen = sizeof(svc) - 1;

    if (adb_send(A_OPEN, lid, 0, svc, svclen + 1) < 0) {
        pthread_mutex_unlock(&g_adb.lock);
        return -1;
    }

    out[0] = 0;
    int pos = 0;
    while (1) {
        struct pollfd pfd = { .fd = g_adb.dev.fd, .events = POLLIN };
        int pret = poll(&pfd, 1, timeout_ms);
        if (pret <= 0) break;

        uint8_t buf[sizeof(AdbPacket) + ADB_MAXDATA];
        int ret = platform_usb_read(&g_adb.dev, g_adb.dev.ep_in, buf,
                                     sizeof(buf), timeout_ms > 0 ? timeout_ms : 5000);
        if (ret < (int)sizeof(AdbPacket)) break;
        AdbPacket *h = (AdbPacket*)buf;
        int dlen = h->data_length;
        if (dlen > ADB_MAXDATA) dlen = ADB_MAXDATA;
        if (h->command == A_CLSE) break;
        if (h->command == A_WRTE && dlen > 0) {
            if (pos + dlen < out_sz - 1) {
                memcpy(out + pos, buf + sizeof(AdbPacket), dlen);
                pos += dlen;
                out[pos] = 0;
            }
        }
        adb_send(A_OKAY, 0, lid, NULL, 0);
    }

    pthread_mutex_unlock(&g_adb.lock);
    return pos > 0 ? 0 : -1;
}

/* ── Fast input text with pre-escaping ── */
int adb_native_input_text_fast(const char *text) {
    if (!text || g_adb.dev.fd < 0 || !g_adb.authorized) return -1;

    char cmd[ADB_MAXDATA];
    int pos = snprintf(cmd, sizeof(cmd), "input text ");
    if (pos < 0) return -1;

    for (const char *p = text; *p && pos < (int)sizeof(cmd) - 3; p++) {
        unsigned char c = *p;
        if (c == '\'') {
            if (pos + 5 > (int)sizeof(cmd) - 1) break;
            cmd[pos++] = '\'';
            cmd[pos++] = '\\';
            cmd[pos++] = '\'';
            cmd[pos++] = '\'';
        } else if (c == '\\') {
            if (pos + 3 > (int)sizeof(cmd) - 1) break;
            cmd[pos++] = '\\';
            cmd[pos++] = '\\';
        } else if (c == ' ' || c == '"' || c == '$' || c == '`' || c == '!' ||
                   c == '&' || c == '|' || c == ';' || c == '(' || c == ')' ||
                   c == '<' || c == '>' || c == '{' || c == '}' || c == '[' ||
                   c == ']' || c == '*' || c == '?' || c == '~' || c == '%') {
            if (pos + 2 > (int)sizeof(cmd) - 1) break;
            cmd[pos++] = '\\';
            cmd[pos++] = c;
        } else if (c >= 0x20) {
            cmd[pos++] = c;
        }
    }
    cmd[pos] = 0;

    return adb_native_shell_noret(cmd);
}

/* ── USB control transfer on ADB interface ── */
int adb_native_usb_control(int request, int value, int index, unsigned char *data, int len) {
    if (g_adb.dev.fd < 0) return -1;
    pthread_mutex_lock(&g_adb.lock);

    struct usbdevfs_ctrltransfer ctrl = {
        .bRequestType = USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_INTERFACE,
        .bRequest = request & 0xFF,
        .wValue = value & 0xFFFF,
        .wIndex = index & 0xFFFF,
        .wLength = len > 0 ? len : 0,
        .data = data,
        .timeout = 5000
    };
    int ret = ioctl(g_adb.dev.fd, USBDEVFS_CONTROL, &ctrl);

    pthread_mutex_unlock(&g_adb.lock);
    return ret;
}
