#define _GNU_SOURCE
#include "adb_raw.h"
#include "adb.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define EV_ABS      3
#define EV_KEY      1
#define EV_SYN      0
#define SYN_REPORT  0
#define BTN_TOUCH   330
#define BTN_TOOL_FINGER 325
#define ABS_MT_POSITION_X 53
#define ABS_MT_POSITION_Y 54
#define ABS_MT_TRACKING_ID 57
#define ABS_MT_SLOT       47
#define ABS_MT_TOUCH_MAJOR 48
#define ABS_MT_WIDTH_MAJOR 50
#define ABS_MT_PRESSURE   58

char* adb_get_input_devs(void) {
    return adb_shell("adb shell ls /dev/input/event* 2>/dev/null | head -10");
}

int adb_find_input_dev(void) {
    char *out = adb_shell("adb shell getevent -p 2>/dev/null | grep -B1 'ABS_MT_POSITION' | head -1 | grep -oE 'event[0-9]+'");
    trim_newline(out);
    if (strlen(out) == 0) {
        out = adb_shell("adb shell ls /dev/input/event* 2>/dev/null | head -1");
        trim_newline(out);
    }
    if (strlen(out) > 0) {
        char *ev = strstr(out, "event");
        if (ev) return atoi(ev + 5);
    }
    return 0;
}

int adb_reliable_event_dev(void) {
    /* Try multiple input devices, return touchscreen event */
    char *out = adb_shell("adb shell getevent -p 2>/dev/null | grep -B2 '0035' | grep -oE 'event[0-9]+' | head -1");
    trim_newline(out);
    if (strlen(out) > 0) {
        char *ev = strstr(out, "event");
        if (ev) return atoi(ev + 5);
    }
    return adb_find_input_dev();
}

static int send_ev(int dev, int type, int code, int val) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "adb shell sendevent /dev/input/event%d %d %d %d 2>/dev/null",
             dev, type, code, val);
    adb_shell_noret(cmd);
    return 0;
}

static int send_syn(int dev) {
    return send_ev(dev, EV_SYN, SYN_REPORT, 0);
}

int adb_raw_touch(int x, int y) {
    int dev = adb_reliable_event_dev();
    if (dev < 0) return -1;

    /* Multi-touch protocol: tracking ID + position + pressure + release */
    send_ev(dev, EV_ABS, ABS_MT_TRACKING_ID, 1);
    send_ev(dev, EV_ABS, ABS_MT_POSITION_X, x);
    send_ev(dev, EV_ABS, ABS_MT_POSITION_Y, y);
    send_ev(dev, EV_ABS, ABS_MT_PRESSURE, 50);
    send_ev(dev, EV_ABS, ABS_MT_TOUCH_MAJOR, 5);
    send_ev(dev, EV_KEY, BTN_TOUCH, 1);
    send_ev(dev, EV_KEY, BTN_TOOL_FINGER, 1);
    send_syn(dev);

    usleep(80000); /* Hold 80ms */

    send_ev(dev, EV_KEY, BTN_TOUCH, 0);
    send_ev(dev, EV_KEY, BTN_TOOL_FINGER, 0);
    send_ev(dev, EV_ABS, ABS_MT_TRACKING_ID, -1);
    send_syn(dev);

    return 0;
}

int adb_raw_touch_slot(int slot, int x, int y) {
    int dev = adb_reliable_event_dev();
    if (dev < 0) return -1;

    send_ev(dev, EV_ABS, ABS_MT_SLOT, slot);
    send_ev(dev, EV_ABS, ABS_MT_TRACKING_ID, slot + 1);
    send_ev(dev, EV_ABS, ABS_MT_POSITION_X, x);
    send_ev(dev, EV_ABS, ABS_MT_POSITION_Y, y);
    send_ev(dev, EV_ABS, ABS_MT_PRESSURE, 50);
    send_ev(dev, EV_KEY, BTN_TOUCH, 1);
    send_syn(dev);

    usleep(60000);

    send_ev(dev, EV_ABS, ABS_MT_SLOT, slot);
    send_ev(dev, EV_ABS, ABS_MT_TRACKING_ID, -1);
    send_ev(dev, EV_KEY, BTN_TOUCH, 0);
    send_syn(dev);

    return 0;
}

int adb_raw_swipe(int x1, int y1, int x2, int y2, int steps) {
    int dev = adb_reliable_event_dev();
    if (dev < 0) return -1;
    if (steps < 5) steps = 15;

    send_ev(dev, EV_ABS, ABS_MT_TRACKING_ID, 2);
    send_ev(dev, EV_KEY, BTN_TOUCH, 1);

    for (int i = 0; i <= steps; i++) {
        int x = x1 + (x2 - x1) * i / steps;
        int y = y1 + (y2 - y1) * i / steps;
        send_ev(dev, EV_ABS, ABS_MT_POSITION_X, x);
        send_ev(dev, EV_ABS, ABS_MT_POSITION_Y, y);
        send_ev(dev, EV_ABS, ABS_MT_PRESSURE, 50);
        send_syn(dev);
        usleep(8000);
    }

    usleep(30000);
    send_ev(dev, EV_KEY, BTN_TOUCH, 0);
    send_ev(dev, EV_ABS, ABS_MT_TRACKING_ID, -1);
    send_syn(dev);
    return 0;
}

int adb_raw_key(int code) {
    int dev = adb_reliable_event_dev();
    if (dev < 0) return -1;
    send_ev(dev, EV_KEY, code, 1);
    send_syn(dev);
    usleep(50000);
    send_ev(dev, EV_KEY, code, 0);
    send_syn(dev);
    return 0;
}

int adb_raw_multitap(const int xy[][2], int num_points) {
    int dev = adb_reliable_event_dev();
    if (dev < 0) return -1;

    for (int p = 0; p < num_points; p++) {
        send_ev(dev, EV_ABS, ABS_MT_SLOT, p);
        send_ev(dev, EV_ABS, ABS_MT_TRACKING_ID, p + 10);
        send_ev(dev, EV_ABS, ABS_MT_POSITION_X, xy[p][0]);
        send_ev(dev, EV_ABS, ABS_MT_POSITION_Y, xy[p][1]);
        send_ev(dev, EV_ABS, ABS_MT_PRESSURE, 50);
    }
    send_ev(dev, EV_KEY, BTN_TOUCH, 1);
    send_syn(dev);

    usleep(50000);

    for (int p = 0; p < num_points; p++) {
        send_ev(dev, EV_ABS, ABS_MT_SLOT, p);
        send_ev(dev, EV_ABS, ABS_MT_TRACKING_ID, -1);
    }
    send_ev(dev, EV_KEY, BTN_TOUCH, 0);
    send_syn(dev);
    return 0;
}
