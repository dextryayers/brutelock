#define _GNU_SOURCE
#include "fastboot.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <ctype.h>
/* Fastboot uses ASCII commands over USB bulk endpoints */
/* Response prefix (4 bytes): OKAY, FAIL, DATA, INFO */
#define FB_RESP_OKAY  "OKAY"
#define FB_RESP_FAIL  "FAIL"
#define FB_RESP_DATA  "DATA"
#define FB_RESP_INFO  "INFO"
#define FB_TIMEOUT    10000  /* 10s default timeout */

/* ── Global state ── */
static struct {
    UsbDevice dev;
    int       connected;
    int       version;       /* fastboot version (1 or 2) */
    char      status[1024];  /* human-readable status */
    pthread_mutex_t lock;
} g_fb = { .dev.fd = -1, .version = 1, .lock = PTHREAD_MUTEX_INITIALIZER };

/* ── Low-level USB I/O ── */

/* Send raw bytes to bulk OUT endpoint */
static int fb_send_raw(const void *buf, int len) {
    if (g_fb.dev.fd < 0) return -1;
    int ret = platform_usb_write(&g_fb.dev, g_fb.dev.ep_out, buf, len, FB_TIMEOUT);
    return (ret == len) ? 0 : -1;
}

/* Receive raw bytes from bulk IN endpoint */
static int fb_recv_raw(void *buf, int maxlen) {
    if (g_fb.dev.fd < 0) return -1;
    return platform_usb_read(&g_fb.dev, g_fb.dev.ep_in, buf, maxlen, FB_TIMEOUT);
}

/* ── Protocol helpers ── */

/* Compare 4-byte response prefix */
static int resp_is(const char *resp, const char *prefix) {
    return resp[0]==prefix[0] && resp[1]==prefix[1] &&
           resp[2]==prefix[2] && resp[3]==prefix[3];
}

/* Read one response packet (4-byte status + optional message up to 256 bytes).
   Returns 0 on OKAY, -1 on FAIL, -2 on IO error.
   Message (without status prefix) is stored in out/out_sz. */
static int fb_read_response(char *out, int out_sz) {
    char raw[260];
    int r = fb_recv_raw(raw, sizeof(raw));
    if (r < 4) return -2;

    int msglen = r - 4;
    if (msglen > out_sz - 1) msglen = out_sz - 1;
    if (msglen > 0) {
        memcpy(out, raw + 4, msglen);
        out[msglen] = 0;
    } else {
        out[0] = 0;
    }

    if (resp_is(raw, FB_RESP_OKAY))  return 0;
    if (resp_is(raw, FB_RESP_FAIL))  return -1;
    if (resp_is(raw, FB_RESP_INFO))  return 1;   /* informational, continue reading */
    if (resp_is(raw, FB_RESP_DATA)) {
        /* DATA response: next 8 bytes = hex data length */
        char hexlen[9] = {0};
        if (r >= 12) memcpy(hexlen, raw+4, 8);
        else { /* try reading more */
            int r2 = fb_recv_raw(raw, sizeof(raw));
            if (r2 >= 8) memcpy(hexlen, raw, 8);
        }
        return 2; /* caller should handle data */
    }
    return -2; /* unknown */
}

/* Send a command (ASCII string) and receive final response.
   Handles multiple INFO lines. Returns final status. */
int fastboot_command(const char *cmd, char *out, int out_sz) {
    if (g_fb.dev.fd < 0) { snprintf(out, out_sz, "no device"); return -2; }

    pthread_mutex_lock(&g_fb.lock);

    /* Fastboot command is just ASCII string, no terminator needed */
    int cmdlen = strlen(cmd);
    if (fb_send_raw(cmd, cmdlen) < 0) {
        snprintf(g_fb.status, sizeof(g_fb.status), "send failed: %s", strerror(errno));
        pthread_mutex_unlock(&g_fb.lock);
        return -2;
    }

    /* Read response(s); INFO lines are intermediate, final is OKAY/FAIL */
    int status = -2;
    out[0] = 0;
    char line[260];

    while (1) {
        int r = fb_read_response(line, sizeof(line));
        if (r == -2) { status = -2; break; }
        if (r == 1) {
            /* INFO line — append to output if there's room */
            int cur = strlen(out);
            if (cur > 0 && cur < out_sz - 80) {
                out[cur] = '\n';
                cur++;
            }
            int sl = strlen(line);
            if (sl > 0 && cur + sl < out_sz - 1) {
                memcpy(out + cur, line, sl);
                out[cur + sl] = 0;
            }
            continue; /* read next */
        }
        if (r == 2) {
            /* DATA — we don't expect data from simple commands */
            status = 2; strncpy(out, line, out_sz-1); break;
        }
        /* r == 0 or -1 */
        status = r;
        strncpy(out, line, out_sz-1);
        out[out_sz-1] = 0;
        break;
    }

    pthread_mutex_unlock(&g_fb.lock);
    return status;
}

int fastboot_command_info(const char *cmd, char *out, int out_sz) {
    return fastboot_command(cmd, out, out_sz);
}

/* ── Variable getters ── */

int fastboot_getvar(const char *var, char *out, int out_sz) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "getvar:%s", var);
    return fastboot_command(cmd, out, out_sz);
}

int fastboot_getvar_all(FastbootInfo *info) {
    if (!info) return -1;
    memset(info, 0, sizeof(*info));
    if (g_fb.dev.fd < 0) return -1;

    char buf[4096];
    int r = fastboot_command("getvar:all", buf, sizeof(buf));
    if (r < 0) return -1;

    /* Parse "name: value" lines from output */
    char *line = buf;
    char *next;
    while (line && *line) {
        next = strchr(line, '\n');
        if (next) { *next = 0; next++; }

        char *colon = strchr(line, ':');
        if (colon) {
            *colon = 0;
            char *val = colon + 1;
            while (*val == ' ') val++;

            if      (strcmp(line, "product") == 0)          strncpy(info->product,       val, sizeof(info->product)-1);
            else if (strcmp(line, "serialno") == 0)         strncpy(info->serial,        val, sizeof(info->serial)-1);
            else if (strcmp(line, "variant") == 0)          strncpy(info->variant,       val, sizeof(info->variant)-1);
            else if (strcmp(line, "battery-voltage") == 0)  strncpy(info->battery_voltage, val, sizeof(info->battery_voltage)-1);
            else if (strcmp(line, "battery-soc-ok") == 0)   strncpy(info->battery_soc,   val, sizeof(info->battery_soc)-1);
            else if (strcmp(line, "unlocked") == 0)         strncpy(info->unlocked,      val, sizeof(info->unlocked)-1);
            else if (strcmp(line, "version") == 0)          strncpy(info->version,       val, sizeof(info->version)-1);
            else if (strcmp(line, "version-bootloader") == 0) strncpy(info->version,     val, sizeof(info->version)-1);
            else if (strcmp(line, "max-download-size") == 0) strncpy(info->max_download, val, sizeof(info->max_download)-1);
            else if (strcmp(line, "soc-id") == 0)           strncpy(info->soc,           val, sizeof(info->soc)-1);
            else if (strcmp(line, "mid") == 0)              strncpy(info->mid,           val, sizeof(info->mid)-1);
            else if (strcmp(line, "cid") == 0)              strncpy(info->cid,           val, sizeof(info->cid)-1);
            else if (strcmp(line, "secure") == 0)           strncpy(info->secure,        val, sizeof(info->secure)-1);
            else if (strcmp(line, "off-mode-charge") == 0)  strncpy(info->off_mode_charge, val, sizeof(info->off_mode_charge)-1);
            else if (strcmp(line, "slot-count") == 0)       strncpy(info->slot_count,    val, sizeof(info->slot_count)-1);
            else if (strcmp(line, "current-slot") == 0)     strncpy(info->current_slot,  val, sizeof(info->current_slot)-1);
            else if (strcmp(line, "has-slot") == 0)         strncpy(info->has_slot,      val, sizeof(info->has_slot)-1);
            else if (strcmp(line, "is-userspace") == 0)     strncpy(info->is_userspace,  val, sizeof(info->is_userspace)-1);
            else if (strcmp(line, "hw-revision") == 0)      strncpy(info->hw_revision,   val, sizeof(info->hw_revision)-1);
            else if (strcmp(line, "board-serial") == 0)     strncpy(info->board_serial,  val, sizeof(info->board_serial)-1);
            else if (strcmp(line, "display-panel") == 0)    strncpy(info->display_panel, val, sizeof(info->display_panel)-1);
            else if (strcmp(line, "fingerprint") == 0)      strncpy(info->fingerprint,   val, sizeof(info->fingerprint)-1);
        }

        line = next;
    }

    info->present = 1;
    return 0;
}

/* ── Operations ── */

int fastboot_reboot(void) {
    char buf[64];
    int r = fastboot_command("reboot", buf, sizeof(buf));
    if (r == 0) snprintf(g_fb.status, sizeof(g_fb.status), "reboot sent");
    else        snprintf(g_fb.status, sizeof(g_fb.status), "reboot failed: %s", buf);
    return r;
}

int fastboot_reboot_bootloader(void) {
    char buf[64];
    int r = fastboot_command("reboot-bootloader", buf, sizeof(buf));
    if (r == 0) snprintf(g_fb.status, sizeof(g_fb.status), "reboot-bootloader sent");
    else        snprintf(g_fb.status, sizeof(g_fb.status), "reboot-bootloader failed: %s", buf);
    return r;
}

int fastboot_continue(void) {
    char buf[64];
    int r = fastboot_command("continue", buf, sizeof(buf));
    if (r == 0) snprintf(g_fb.status, sizeof(g_fb.status), "continue booting");
    else        snprintf(g_fb.status, sizeof(g_fb.status), "continue failed: %s", buf);
    return r;
}

int fastboot_flash(const char *partition, const unsigned char *data, int len) {
    if (g_fb.dev.fd < 0) return -2;
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "flash:%s", partition);
    char out[64];

    /* Send flash command */
    int r = fastboot_command(cmd, out, sizeof(out));
    if (r != 2) return r;  /* should get DATA response */

    /* DATA response: send data in chunks */
    int sent = 0;
    while (sent < len) {
        int chunk = (len - sent > 4096) ? 4096 : (len - sent);
        if (fb_send_raw(data + sent, chunk) < 0) break;
        sent += chunk;
    }
    if (sent < len) return -2;

    /* Read final status */
    usleep(500000);
    char final[64];
    r = fb_read_response(final, sizeof(final));
    return (r == 0) ? 0 : -1;
}

int fastboot_erase(const char *partition) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "erase:%s", partition);
    char out[64];
    return fastboot_command(cmd, out, sizeof(out));
}

int fastboot_boot(const unsigned char *kernel_data, int len) {
    char out[64];
    int r = fastboot_command("boot", out, sizeof(out));
    if (r != 2) return r;
    int sent = 0;
    while (sent < len) {
        int chunk = (len - sent > 4096) ? 4096 : (len - sent);
        if (fb_send_raw(kernel_data + sent, chunk) < 0) break;
        sent += chunk;
    }
    return (sent == len) ? 0 : -2;
}

int fastboot_oem_unlock(void) {
    char out[256];
    /* Try multiple commands for different OEMs */
    int r = fastboot_command("oem unlock", out, sizeof(out));
    if (r == 0) { snprintf(g_fb.status, sizeof(g_fb.status), "OEM unlock successful"); return 0; }

    r = fastboot_command("flashing unlock", out, sizeof(out));
    if (r == 0) { snprintf(g_fb.status, sizeof(g_fb.status), "Flashing unlock successful"); return 0; }

    snprintf(g_fb.status, sizeof(g_fb.status), "unlock failed: %s", out);
    return r;
}

int fastboot_flashing_unlock(void) {
    char out[256];
    int r = fastboot_command("flashing unlock", out, sizeof(out));
    if (r == 0) snprintf(g_fb.status, sizeof(g_fb.status), "flashing unlock OK");
    else        snprintf(g_fb.status, sizeof(g_fb.status), "flashing unlock: %s", out);
    return r;
}

int fastboot_oem_device_info(char *out, int out_sz) {
    return fastboot_command("oem device-info", out, out_sz);
}

/* ── Detection ── */

int fastboot_detect(FastbootInfo *info) {
    if (info) memset(info, 0, sizeof(*info));

    UsbDevice tmp;
    if (!platform_usb_find_by_class(&tmp, 0xFF, 0x42, 0x03)) return 0;

    int found = 1;
    if (info) {
        info->present = 1;
        if (tmp.serial[0]) strncpy(info->serial, tmp.serial, sizeof(info->serial)-1);
    }
    platform_usb_close(&tmp);
    return found;
}

/* ── Init / connect / close ── */

int fastboot_init(void) {
    if (g_fb.dev.fd >= 0 && g_fb.connected) return 1;

    if (g_fb.dev.fd >= 0) platform_usb_close(&g_fb.dev);
    memset(&g_fb.dev, 0, sizeof(g_fb.dev));
    g_fb.dev.fd = -1;
    g_fb.connected = 0;

    if (!platform_usb_find_fastboot(&g_fb.dev)) {
        snprintf(g_fb.status, sizeof(g_fb.status), "no fastboot device found");
        return 0;
    }

    /* Test connection by reading a variable */
    char ver[64];
    int r = fastboot_getvar("version", ver, sizeof(ver));
    if (r == 0) {
        g_fb.version = (strncmp(ver, "0.", 2) == 0) ? 1 : 2;
        snprintf(g_fb.status, sizeof(g_fb.status), "Fastboot v%s connected", ver);
    } else {
        snprintf(g_fb.status, sizeof(g_fb.status), "fastboot device (version check failed)");
    }

    g_fb.connected = 1;
    return 1;
}

int fastboot_check(void) {
    if (g_fb.dev.fd < 0) return 0;
    /* Quick test: send getvar version and expect OKAY */
    char ver[64];
    return fastboot_getvar("version", ver, sizeof(ver)) == 0;
}

void fastboot_close(void) {
    platform_usb_close(&g_fb.dev);
    memset(&g_fb, 0, sizeof(g_fb));
    g_fb.dev.fd = -1;
    g_fb.version = 1;
}

const char* fastboot_status_str(void) {
    return g_fb.status;
}

void fastboot_get_info(char *buf, int sz) {
    FastbootInfo info;
    if (fastboot_getvar_all(&info) < 0) {
        snprintf(buf, sz, "  [Fastboot] Could not read variables");
        return;
    }

    int pos = 0;
    pos += snprintf(buf + pos, sz - pos,
        "  Product  : %s\n", info.product[0] ? info.product : "?");
    pos += snprintf(buf + pos, sz - pos,
        "  Serial   : %s\n", info.serial[0] ? info.serial : "?");
    if (info.variant[0])
        pos += snprintf(buf + pos, sz - pos, "  Variant  : %s\n", info.variant);
    if (info.version[0])
        pos += snprintf(buf + pos, sz - pos, "  Version  : %s\n", info.version);
    if (info.unlocked[0])
        pos += snprintf(buf + pos, sz - pos, "  Unlocked : %s\n", info.unlocked);
    if (info.secure[0])
        pos += snprintf(buf + pos, sz - pos, "  Secure   : %s\n", info.secure);
    if (info.mid[0])
        pos += snprintf(buf + pos, sz - pos, "  MID      : %s\n", info.mid);
    if (info.cid[0])
        pos += snprintf(buf + pos, sz - pos, "  CID      : %s\n", info.cid);
    if (info.soc[0])
        pos += snprintf(buf + pos, sz - pos, "  SoC ID   : %s\n", info.soc);
    if (info.battery_voltage[0])
        pos += snprintf(buf + pos, sz - pos, "  Battery  : %s mV\n", info.battery_voltage);
    if (info.max_download[0])
        pos += snprintf(buf + pos, sz - pos, "  Max DL   : %s bytes\n", info.max_download);
    if (info.current_slot[0])
        pos += snprintf(buf + pos, sz - pos, "  Slot     : %s\n", info.current_slot);
    if (info.fingerprint[0])
        pos += snprintf(buf + pos, sz - pos, "  FP       : %s\n", info.fingerprint);
    if (info.hw_revision[0])
        pos += snprintf(buf + pos, sz - pos, "  HW Rev   : %s\n", info.hw_revision);
    if (info.display_panel[0])
        pos += snprintf(buf + pos, sz - pos, "  Panel    : %s\n", info.display_panel);
}
