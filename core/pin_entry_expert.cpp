#include "pin_entry_expert.h"
#include "adb_native.h"
#include "adb.h"
#include "input_precision.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

PinEntryExpert::PinEntryExpert()
    : method_(INPUT_METHOD_TOUCH), touch_found_(0),
      inter_digit_us_(80000), attempts_(0), successes_(0) {
    memset(&td_, 0, sizeof(td_));
    td_.fd = -1;
    grid_x_ = grid_y_ = 0;
    key_w_ = key_h_ = 0;
    screen_w_ = 1080;
    screen_h_ = 1920;
}

PinEntryExpert::~PinEntryExpert() {
    if (td_.fd >= 0) tfb_close_touch(&td_);
}

int PinEntryExpert::init() {
    hw_.init();
    hw_.scan_all_components();
    gs_.set_screen_resolution(screen_w_, screen_h_);

    if (hw_.is_component_available(HW_TOUCH)) {
        char path[64] = "/dev/input/event0";
        for (int i = 0; i < 32; i++) {
            snprintf(path, sizeof(path), "/dev/input/event%d", i);
            TouchDevice test;
            memset(&test, 0, sizeof(test));
            test.fd = -1;
            if (tfb_open_touch(&test, path)) {
                if (test.has_touch) {
                    td_ = test;
                    touch_found_ = 1;
                    break;
                }
                tfb_close_touch(&test);
            }
        }
    }

    if (touch_found_) {
        method_ = INPUT_METHOD_TOUCH;
    } else {
        method_ = INPUT_METHOD_SENDEVENT;
    }
    return 1;
}

int PinEntryExpert::detect_keyboard_layout(PinKeyboardLayout *layout) {
    layout->grid_x = 0;
    layout->grid_y = 0;
    layout->key_w = 0;
    layout->key_h = 0;
    layout->screen_w = screen_w_;
    layout->screen_h = screen_h_;
    layout->dot_count = 0;
    layout->lock_type.clear();

    int w = layout->screen_w;
    int h = layout->screen_h;

    layout->key_w = w / 5;
    layout->key_h = layout->key_w;
    layout->grid_x = (w - layout->key_w * 3) / 2;
    layout->grid_y = (h - layout->key_h * 4) / 2 + layout->key_h;

    char buf[1024];
    if (adb_native_shell("settings get secure lockscreen.password_type 2>/dev/null",
                         buf, sizeof(buf)) == 0 && strlen(buf) > 0) {
        trim_newline(buf);
        int pt = atoi(buf);
        if (pt == 262144) {
            layout->lock_type = "pattern";
            layout->dot_count = 9;
        } else if (pt == 65536 || pt == 327680) {
            layout->lock_type = "pin";
            layout->key_w = layout->screen_w / 5;
            layout->key_h = layout->key_w;
            layout->grid_x = (layout->screen_w - layout->key_w * 3) / 2;
            layout->grid_y = (layout->screen_h - layout->key_h * 4) / 2 + layout->key_h;
        } else if (pt == 131072) {
            layout->lock_type = "password";
        }
    }

    if (adb_native_shell("dumpsys window 2>/dev/null | grep -i 'mCurrentFocus' | head -1",
                         buf, sizeof(buf)) == 0) {
        if (strstr(buf, "PinEntry") || strstr(buf, "PinView"))
            layout->lock_type = "pin";
        else if (strstr(buf, "PatternEntry"))
            layout->lock_type = "pattern";
    }

    this->grid_x_ = layout->grid_x;
    this->grid_y_ = layout->grid_y;
    this->key_w_ = layout->key_w;
    this->key_h_ = layout->key_h;
    this->screen_w_ = layout->screen_w;
    this->screen_h_ = layout->screen_h;
    gs_.set_screen_resolution(layout->screen_w, layout->screen_h);

    return 1;
}

int PinEntryExpert::enter_pin(const std::string &pin, PinKeyboardLayout *layout) {
    attempts_++;
    switch (method_) {
        case INPUT_METHOD_TOUCH:
            if (enter_pin_touch(pin, layout)) { successes_++; return 1; }
            break;
        case INPUT_METHOD_SENDEVENT:
            if (enter_pin_sendevent(pin, layout)) { successes_++; return 1; }
            break;
        case INPUT_METHOD_KEYEVENT:
            if (enter_pin_keyevent(pin)) { successes_++; return 1; }
            break;
        case INPUT_METHOD_GPIO_BUTTON:
            if (enter_pin_gpio(pin)) { successes_++; return 1; }
            break;
        default:
            break;
    }
    if (enter_pin_touch(pin, layout)) { successes_++; return 1; }
    if (enter_pin_sendevent(pin, layout)) { successes_++; return 1; }
    if (enter_pin_keyevent(pin)) { successes_++; return 1; }
    last_error_ = std::string("All input methods failed");
    return 0;
}

int PinEntryExpert::enter_pin_sequence(const std::vector<std::string> &pins, PinKeyboardLayout *layout) {
    for (const auto &pin : pins) {
        if (enter_pin(pin, layout)) return 1;
    }
    return 0;
}

int PinEntryExpert::enter_pattern(const std::vector<int> &dots, PinKeyboardLayout *layout) {
    Gesture g = gs_.create_pattern_unlock(dots);
    (void)layout;
    if (touch_found_ && td_.fd >= 0) {
        for (const auto &pt : g.points) {
            if (pt.tracking_id < 0) {
                tfb_send_touch_event(&td_, pt.slot, pt.x, pt.y, 0, -1);
            } else {
                tfb_send_touch_event(&td_, pt.slot, pt.x, pt.y, pt.pressure, pt.tracking_id);
            }
            usleep(10000);
        }
        return 1;
    }
    return 0;
}

int PinEntryExpert::enter_pin_turbo(const std::string &pin) {
    PinKeyboardLayout layout;
    detect_keyboard_layout(&layout);
    for (char c : pin) {
        int digit = c - '0';
        tfb_digit_press(&td_, digit, layout.grid_x, layout.grid_y, layout.key_w, layout.key_h);
        usleep(inter_digit_us_);
    }
    return 1;
}

int PinEntryExpert::enter_pin_multitap(const std::string &pin) {
    PinKeyboardLayout layout;
    detect_keyboard_layout(&layout);
    std::vector<int> digits;
    for (char c : pin) digits.push_back(c - '0');
    return tfb_multi_tap(&td_, digits.data(), (int)digits.size(),
                          &layout.grid_x, &layout.grid_y,
                          layout.key_w, layout.key_h);
}

int PinEntryExpert::enter_pin_touch(const std::string &pin, const PinKeyboardLayout *layout) {
    if (!touch_found_ || td_.fd < 0) return 0;
    Gesture g = gs_.create_pin_entry(pin, layout->grid_x, layout->grid_y,
                                      layout->key_w, layout->key_h);
    for (const auto &pt : g.points) {
        tfb_send_touch_event(&td_, pt.slot, pt.x, pt.y, pt.pressure, pt.tracking_id);
        usleep(5000);
    }
    usleep(100000);
    return 1;
}

int PinEntryExpert::enter_pin_sendevent(const std::string &pin, const PinKeyboardLayout *layout) {
    (void)layout;
    return prec_enter_pin_turbo(pin.c_str());
}

int PinEntryExpert::enter_pin_keyevent(const std::string &pin) {
    for (char c : pin) {
        prec_keyevent_digit(c - '0');
        usleep(inter_digit_us_);
    }
    return 1;
}

int PinEntryExpert::enter_pin_gpio(const std::string &pin) {
    (void)pin;
    return 0;
}

int PinEntryExpert::wake_screen() {
    if (hw_.is_component_available(HW_GPIO)) {
        if (hw_.gpio_press_power()) {
            usleep(500000);
            return 1;
        }
    }
    char buf[256];
    if (adb_native_shell("input keyevent 26", buf, sizeof(buf)) == 0) {
        usleep(500000);
        return 1;
    }
    return 0;
}

int PinEntryExpert::swipe_unlock() {
    return swipe_unlock_touch() || swipe_unlock_sendevent();
}

int PinEntryExpert::swipe_unlock_touch() {
    if (!touch_found_ || td_.fd < 0) return 0;
    Gesture g = gs_.create_swipe(screen_w_/2, screen_h_-100,
                                  screen_w_/2, screen_h_/3, 30, 2000);
    for (const auto &pt : g.points) {
        tfb_send_touch_event(&td_, pt.slot, pt.x, pt.y, pt.pressure, pt.tracking_id);
        usleep(1000);
    }
    usleep(300000);
    return 1;
}

int PinEntryExpert::swipe_unlock_sendevent() {
    prec_wake_and_swipe();
    return 1;
}

int PinEntryExpert::press_ok() {
    if (touch_found_ && td_.fd >= 0) {
        int ok_x = screen_w_ / 2;
        int ok_y = screen_h_ * 3 / 4;
        tfb_tap_screen(&td_, ok_x, ok_y);
        return 1;
    }
    if (adb_native_shell_noret("input keyevent KEYCODE_ENTER") == 0) return 1;
    if (adb_native_shell_noret("input keyevent 66") == 0) return 1;
    return 0;
}

int PinEntryExpert::press_emergency_call() {
    if (touch_found_ && td_.fd >= 0) {
        tfb_tap_screen(&td_, screen_w_ / 2, screen_h_ - 100);
        return 1;
    }
    return 0;
}

int PinEntryExpert::is_unlocked() {
    char buf[4096];
    if (adb_native_shell("dumpsys window 2>/dev/null | grep -i 'mKeyguardShowing' | head -1",
                         buf, sizeof(buf)) == 0) {
        if (strstr(buf, "false")) return 1;
    }
    if (adb_native_shell("dumpsys window 2>/dev/null | grep -i 'mCurrentFocus' | head -1",
                         buf, sizeof(buf)) == 0) {
        if (strstr(buf, "Launcher") || strstr(buf, "HomeScreen"))
            return 1;
    }
    return adb_is_unlocked();
}

int PinEntryExpert::detect_lockout() {
    char buf[4096];
    if (adb_native_shell("settings get secure lockscreen.lockoutattemptdeadline 2>/dev/null",
                         buf, sizeof(buf)) == 0 && strlen(buf) > 0) {
        trim_newline(buf);
        long deadline = atol(buf);
        if (deadline > 0) {
            long now = time(NULL) * 1000;
            int remaining = (int)((deadline - now) / 1000);
            if (remaining > 0) {
                if (remaining > 600) remaining = 600;
                printf(YELLOW "\n[!] LOCKOUT detected! %ds remaining\n" RESET, remaining);
                fflush(stdout);
                for (int w = 0; w < remaining; w++) {
                    printf("."); fflush(stdout);
                    sleep(1);
                }
                printf("\n");
                fflush(stdout);
                return remaining;
            }
        }
    }
    return 0;
}

int PinEntryExpert::set_grid(int x, int y, int w, int h) {
    grid_x_ = x; grid_y_ = y; key_w_ = w; key_h_ = h;
    return 1;
}

void PinEntryExpert::set_screen_size(int w, int h) {
    screen_w_ = w; screen_h_ = h;
    gs_.set_screen_resolution(w, h);
}

int PinEntryExpert::detect_grid_via_probe(const PinKeyboardLayout *layout) {
    (void)layout;
    grid_x_ = screen_w_ / 6;
    grid_y_ = screen_h_ / 3;
    key_w_ = (screen_w_ - grid_x_ * 2) / 3;
    key_h_ = key_w_;
    return 1;
}

void PinEntryExpert::print_layout(const PinKeyboardLayout *layout) const {
    printf("\n" BOLD "=== PIN Keyboard Layout ===" RESET "\n");
    printf("  Screen: %dx%d\n", layout->screen_w, layout->screen_h);
    printf("  Grid: (%d, %d) keys %dx%d\n",
           layout->grid_x, layout->grid_y, layout->key_w, layout->key_h);
    printf("  Lock Type: %s\n", layout->lock_type.c_str());
    printf("  Dot Count: %d\n", layout->dot_count);
    printf("  Input Method: ");
    switch (method_) {
        case INPUT_METHOD_TOUCH: printf("Touch (/dev/input)\n"); break;
        case INPUT_METHOD_SENDEVENT: printf("sendevent\n"); break;
        case INPUT_METHOD_KEYEVENT: printf("keyevent\n"); break;
        case INPUT_METHOD_AOA_HID: printf("AOA HID\n"); break;
        case INPUT_METHOD_GPIO_BUTTON: printf("GPIO Button\n"); break;
        case INPUT_METHOD_FRAMEBUFFER: printf("Framebuffer\n"); break;
        default: printf("Unknown\n");
    }
    printf("  Inter-digit delay: %d us\n", inter_digit_us_);
}
