#ifndef GESTURE_SYNTHESIS_H
#define GESTURE_SYNTHESIS_H

#include <vector>
#include <string>
#include <utility>

typedef struct {
    int x;
    int y;
    int pressure;
    int slot;
    int tracking_id;
    unsigned int time_us;
} TouchPoint;

typedef struct {
    std::vector<TouchPoint> points;
    int total_duration_us;
    int num_fingers;
    std::string gesture_type;
} Gesture;

class GestureSynthesis {
public:
    GestureSynthesis();
    ~GestureSynthesis();

    Gesture create_tap(int x, int y, int duration_us = 80000);
    Gesture create_double_tap(int x, int y, int interval_us = 100000);
    Gesture create_long_press(int x, int y, int duration_us = 500000);
    Gesture create_swipe(int x1, int y1, int x2, int y2, int steps = 50, int step_us = 2000);
    Gesture create_swipe_curve(int x1, int y1, int x2, int y2, float curvature, int steps = 50);
    Gesture create_pinch(int cx, int cy, int start_dist, int end_dist, int steps = 30);
    Gesture create_spread(int cx, int cy, int start_dist, int end_dist, int steps = 30);
    Gesture create_pattern_unlock(const std::vector<int> &dots, int dot_size = 50);
    Gesture create_digit_press(int digit, int grid_x, int grid_y, int key_w, int key_h);
    Gesture create_pin_entry(const std::string &pin, int grid_x, int grid_y, int key_w, int key_h);
    Gesture create_custom_path(const std::vector<std::pair<int,int>> &path, int step_us = 2000);

    void add_human_delay(Gesture &g, int min_us = 30000, int max_us = 120000);
    void add_jitter(Gesture &g, int max_jitter = 3);
    void apply_acceleration(Gesture &g, float accel = 1.2f);
    void set_screen_resolution(int w, int h) { screen_w_ = w; screen_h_ = h; }

    std::string gesture_to_string(const Gesture &g) const;
    void print_gesture(const Gesture &g) const;

private:
    int screen_w_;
    int screen_h_;
    int rng_state_;

    int random_int(int min, int max);
    float random_float();
    int bezier_point(int p0, int p1, int p2, int p3, float t);
};

#endif
