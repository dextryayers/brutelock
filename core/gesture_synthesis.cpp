#include "gesture_synthesis.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cmath>
#include <algorithm>

GestureSynthesis::GestureSynthesis() : screen_w_(1080), screen_h_(1920), rng_state_(42) {}
GestureSynthesis::~GestureSynthesis() {}

int GestureSynthesis::random_int(int min, int max) {
    rng_state_ = (rng_state_ * 1103515245 + 12345) & 0x7fffffff;
    return min + (rng_state_ % (max - min + 1));
}

float GestureSynthesis::random_float() {
    rng_state_ = (rng_state_ * 1103515245 + 12345) & 0x7fffffff;
    return (float)rng_state_ / 2147483648.0f;
}

int GestureSynthesis::bezier_point(int p0, int p1, int p2, int p3, float t) {
    float u = 1.0f - t;
    return (int)(u*u*u*p0 + 3*u*u*t*p1 + 3*u*t*t*p2 + t*t*t*p3);
}

Gesture GestureSynthesis::create_tap(int x, int y, int duration_us) {
    Gesture g;
    g.gesture_type = "tap";
    g.num_fingers = 1;
    TouchPoint p;
    p.x = x; p.y = y; p.pressure = 50;
    p.slot = 0; p.tracking_id = 1; p.time_us = 0;
    g.points.push_back(p);
    p.tracking_id = -1; p.time_us = duration_us;
    g.points.push_back(p);
    g.total_duration_us = duration_us;
    return g;
}

Gesture GestureSynthesis::create_double_tap(int x, int y, int interval_us) {
    Gesture g;
    g.gesture_type = "double_tap";
    g.num_fingers = 1;
    TouchPoint p;
    p.x = x; p.y = y; p.pressure = 50;
    p.slot = 0; p.tracking_id = 1; p.time_us = 0;
    g.points.push_back(p);
    p.tracking_id = -1; p.time_us = 80000;
    g.points.push_back(p);
    p.tracking_id = 2; p.time_us = interval_us;
    g.points.push_back(p);
    p.tracking_id = -1; p.time_us = interval_us + 80000;
    g.points.push_back(p);
    g.total_duration_us = interval_us + 80000;
    return g;
}

Gesture GestureSynthesis::create_long_press(int x, int y, int duration_us) {
    Gesture g;
    g.gesture_type = "long_press";
    g.num_fingers = 1;
    TouchPoint p;
    p.x = x; p.y = y; p.pressure = 80;
    p.slot = 0; p.tracking_id = 1; p.time_us = 0;
    g.points.push_back(p);
    /* Keep pressure high throughout */
    for (int t = 100000; t < duration_us; t += 100000) {
        p.time_us = t;
        p.pressure = 80 + random_int(-10, 10);
        g.points.push_back(p);
    }
    p.tracking_id = -1; p.time_us = duration_us; p.pressure = 0;
    g.points.push_back(p);
    g.total_duration_us = duration_us;
    return g;
}

Gesture GestureSynthesis::create_swipe(int x1, int y1, int x2, int y2, int steps, int step_us) {
    Gesture g;
    g.gesture_type = "swipe";
    g.num_fingers = 1;
    TouchPoint p;
    p.slot = 0; p.tracking_id = 1;

    for (int i = 0; i < steps; i++) {
        float t = (float)i / (steps - 1);
        /* Add slight bezier curve for realism */
        float ease = t < 0.5f ? 2*t*t : -1 + (4-2*t)*t;
        p.x = (int)(x1 + (x2 - x1) * ease + random_int(-2, 2));
        p.y = (int)(y1 + (y2 - y1) * ease + random_int(-2, 2));
        p.pressure = 40 + random_int(0, 20);
        p.time_us = i * step_us;
        g.points.push_back(p);
    }
    p.tracking_id = -1;
    p.time_us = steps * step_us;
    g.points.push_back(p);
    g.total_duration_us = steps * step_us;
    return g;
}

Gesture GestureSynthesis::create_swipe_curve(int x1, int y1, int x2, int y2, float curvature, int steps) {
    Gesture g;
    g.gesture_type = "swipe_curve";
    g.num_fingers = 1;

    int cx = (int)((x1 + x2) / 2.0f + curvature);
    int cy = (int)((y1 + y2) / 2.0f - curvature * 0.5f);

    TouchPoint p;
    p.slot = 0; p.tracking_id = 1;

    for (int i = 0; i < steps; i++) {
        float t = (float)i / (steps - 1);
        p.x = bezier_point(x1, cx, cx, x2, t);
        p.y = bezier_point(y1, cy, cy, y2, t);
        p.pressure = 50;
        p.time_us = i * 3000;
        g.points.push_back(p);
    }
    p.tracking_id = -1;
    p.time_us = steps * 3000;
    g.total_duration_us = steps * 3000;
    g.points.push_back(p);
    return g;
}

Gesture GestureSynthesis::create_pinch(int cx, int cy, int start_dist, int end_dist, int steps) {
    Gesture g;
    g.gesture_type = "pinch";
    g.num_fingers = 2;

    for (int i = 0; i < steps; i++) {
        float t = (float)i / (steps - 1);
        int dist = start_dist + (int)((end_dist - start_dist) * t);
        /* Finger 1 */
        TouchPoint p1;
        p1.slot = 0; p1.tracking_id = 1;
        p1.x = cx - dist/2 + random_int(-1, 1);
        p1.y = cy + random_int(-1, 1);
        p1.pressure = 50;
        p1.time_us = i * 3000;
        g.points.push_back(p1);
        /* Finger 2 */
        TouchPoint p2;
        p2.slot = 1; p2.tracking_id = 2;
        p2.x = cx + dist/2 + random_int(-1, 1);
        p2.y = cy + random_int(-1, 1);
        p2.pressure = 50;
        p2.time_us = i * 3000;
        g.points.push_back(p2);
    }
    g.total_duration_us = steps * 3000;
    return g;
}

Gesture GestureSynthesis::create_spread(int cx, int cy, int start_dist, int end_dist, int steps) {
    return create_pinch(cx, cy, start_dist, end_dist, steps);
}

Gesture GestureSynthesis::create_pattern_unlock(const std::vector<int> &dots, int dot_size) {
    Gesture g;
    g.gesture_type = "pattern";
    g.num_fingers = 1;

    /* 3x3 grid, center-to-center distance = dot_size*2 */
    int grid_spacing = dot_size * 2;

    TouchPoint p;
    p.slot = 0; p.tracking_id = 1;
    int step_us = 80000;

    for (size_t i = 0; i < dots.size(); i++) {
        int d = dots[i] - 1;
        int col = d % 3;
        int row = d / 3;
        p.x = grid_spacing + col * grid_spacing + random_int(-2, 2);
        p.y = grid_spacing + row * grid_spacing + random_int(-2, 2);
        p.pressure = 60 + random_int(-10, 10);
        p.time_us = (int)(i * step_us);
        g.points.push_back(p);
    }
    p.tracking_id = -1;
    p.time_us = (int)(dots.size() * step_us + 50000);
    g.points.push_back(p);
    g.total_duration_us = (int)(dots.size() * step_us + 50000);
    return g;
}

Gesture GestureSynthesis::create_digit_press(int digit, int grid_x, int grid_y, int key_w, int key_h) {
    Gesture g;
    g.gesture_type = "digit";
    g.num_fingers = 1;

    int col = (digit - 1) % 3;
    int row = (digit - 1) / 3;
    if (digit == 0) { col = 1; row = 3; }
    /* Add small random offset for realism */
    int cx = grid_x + col * key_w + key_w / 2 + random_int(-3, 3);
    int cy = grid_y + row * key_h + key_h / 2 + random_int(-3, 3);

    TouchPoint p;
    p.slot = 0; p.tracking_id = 1;
    p.x = cx; p.y = cy;
    p.pressure = 70;
    p.time_us = 0;
    g.points.push_back(p);

    p.tracking_id = -1;
    p.pressure = 0;
    p.time_us = 60000;
    g.points.push_back(p);
    g.total_duration_us = 60000;
    return g;
}

Gesture GestureSynthesis::create_pin_entry(const std::string &pin, int grid_x, int grid_y, int key_w, int key_h) {
    Gesture g;
    g.gesture_type = "pin_entry";
    g.num_fingers = 1;

    int total_us = 0;
    for (size_t i = 0; i < pin.length(); i++) {
        int digit = pin[i] - '0';
        Gesture dg = create_digit_press(digit, grid_x, grid_y, key_w, key_h);
        for (auto &pt : dg.points) {
            pt.time_us += total_us;
            g.points.push_back(pt);
        }
        total_us += 60000 + random_int(20000, 80000);
    }
    g.total_duration_us = total_us;
    return g;
}

Gesture GestureSynthesis::create_custom_path(const std::vector<std::pair<int,int>> &path, int step_us) {
    Gesture g;
    g.gesture_type = "custom";
    g.num_fingers = 1;
    TouchPoint p;
    p.slot = 0; p.tracking_id = 1;

    for (size_t i = 0; i < path.size(); i++) {
        p.x = path[i].first + random_int(-1, 1);
        p.y = path[i].second + random_int(-1, 1);
        p.pressure = 50;
        p.time_us = (int)(i * step_us);
        g.points.push_back(p);
    }
    p.tracking_id = -1;
    p.time_us = (int)(path.size() * step_us);
    g.points.push_back(p);
    g.total_duration_us = (int)(path.size() * step_us);
    return g;
}

void GestureSynthesis::add_human_delay(Gesture &g, int min_us, int max_us) {
    for (auto &pt : g.points)
        pt.time_us += random_int(min_us, max_us);
    g.total_duration_us += random_int(min_us, max_us);
}

void GestureSynthesis::add_jitter(Gesture &g, int max_jitter) {
    for (auto &pt : g.points) {
        pt.x += random_int(-max_jitter, max_jitter);
        pt.y += random_int(-max_jitter, max_jitter);
    }
}

void GestureSynthesis::apply_acceleration(Gesture &g, float accel) {
    for (auto &pt : g.points)
        pt.time_us = (int)(pt.time_us / accel);
    g.total_duration_us = (int)(g.total_duration_us / accel);
}

std::string GestureSynthesis::gesture_to_string(const Gesture &g) const {
    char buf[4096];
    int pos = 0;
    pos += snprintf(buf + pos, sizeof(buf) - pos,
                    "Gesture: %s | %zu points | %d fingers | %d us\n",
                    g.gesture_type.c_str(), g.points.size(),
                    g.num_fingers, g.total_duration_us);
    for (const auto &pt : g.points) {
        pos += snprintf(buf + pos, sizeof(buf) - pos,
                        "  t=%d slot=%d id=%d (%d,%d) p=%d\n",
                        pt.time_us, pt.slot, pt.tracking_id,
                        pt.x, pt.y, pt.pressure);
        if (pos >= (int)sizeof(buf) - 100) break;
    }
    return std::string(buf);
}

void GestureSynthesis::print_gesture(const Gesture &g) const {
    printf("%s", gesture_to_string(g).c_str());
}
