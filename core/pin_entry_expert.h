#ifndef PIN_ENTRY_EXPERT_H
#define PIN_ENTRY_EXPERT_H

#include <string>
#include <vector>
#include "touch_fb_exploit.h"
#include "gesture_synthesis.h"
#include "hw_control_expert.h"

typedef enum {
    INPUT_METHOD_TOUCH,
    INPUT_METHOD_SENDEVENT,
    INPUT_METHOD_KEYEVENT,
    INPUT_METHOD_AOA_HID,
    INPUT_METHOD_GPIO_BUTTON,
    INPUT_METHOD_FRAMEBUFFER,
    INPUT_METHOD_MAX
} PinInputMethod;

typedef struct {
    int grid_x;
    int grid_y;
    int key_w;
    int key_h;
    int screen_w;
    int screen_h;
    int dot_count;
    std::string lock_type;
} PinKeyboardLayout;

class PinEntryExpert {
public:
    PinEntryExpert();
    ~PinEntryExpert();

    int init();
    void set_method(PinInputMethod method) { method_ = method; }
    PinInputMethod get_method() const { return method_; }

    int detect_keyboard_layout(PinKeyboardLayout *layout);
    int enter_pin(const std::string &pin, PinKeyboardLayout *layout);
    int enter_pin_sequence(const std::vector<std::string> &pins, PinKeyboardLayout *layout);
    int enter_pattern(const std::vector<int> &dots, PinKeyboardLayout *layout);
    int enter_pin_turbo(const std::string &pin);
    int enter_pin_multitap(const std::string &pin);

    int wake_screen();
    int swipe_unlock();
    int press_ok();
    int press_emergency_call();
    int is_unlocked();
    int detect_lockout();

    int set_speed(int inter_digit_us) { inter_digit_us_ = inter_digit_us; return 1; }
    int set_grid(int x, int y, int w, int h);
    void set_screen_size(int w, int h);

    int get_attempts() const { return attempts_; }
    int get_successes() const { return successes_; }
    void reset_stats() { attempts_ = 0; successes_ = 0; }

    std::string get_last_error() const { return last_error_; }
    void print_layout(const PinKeyboardLayout *layout) const;

    HWControlExpert *get_hw_control() { return &hw_; }
    GestureSynthesis *get_gesture() { return &gs_; }

    int enter_pin_touch(const std::string &pin, const PinKeyboardLayout *layout);
    int enter_pin_sendevent(const std::string &pin, const PinKeyboardLayout *layout);
    int enter_pin_keyevent(const std::string &pin);
    int enter_pin_gpio(const std::string &pin);
    int swipe_unlock_touch();

private:
    PinInputMethod method_;
    HWControlExpert hw_;
    GestureSynthesis gs_;
    TouchDevice td_;
    int touch_found_;
    int inter_digit_us_;
    int grid_x_, grid_y_, key_w_, key_h_;
    int screen_w_, screen_h_;
    int attempts_;
    int successes_;
    std::string last_error_;

    int swipe_unlock_sendevent();
    int detect_grid_via_probe(const PinKeyboardLayout *layout);
};

#endif
