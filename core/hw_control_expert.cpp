#include "hw_control_expert.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

HWControlExpert::HWControlExpert()
    : initialized_(0), touch_found_(0) {
    memset(&td_, 0, sizeof(td_));
    gc_init(&gc_);
    ie_init(&ie_);
    se_init(&se_);
    she_init(&she_);
    pce_init(&pce_);
    kd_init(&kd_);
    dm_init(&dm_);
    for (int i = 0; i < HW_MAX; i++) {
        components_[i].component = (HWComponent)i;
        components_[i].available = 0;
        components_[i].controllable = 0;
    }
}

HWControlExpert::~HWControlExpert() {
    if (td_.fd >= 0) tfb_close_touch(&td_);
    gc_close(&gc_);
    ie_close_all(&ie_);
    se_close(&se_);
    pce_close(&pce_);
    kd_close(&kd_);
    dm_close(&dm_);
}

int HWControlExpert::init() {
    kd_open_mem(&kd_);
    dm_open_mem(&dm_);
    dm_scan_iomem(&dm_);
    gc_scan_sysfs(&gc_);
    ie_scan_buses(&ie_);
    se_scan_spmi_devices(&se_);
    she_scan_all(&she_);
    pce_scan_pinctrl(&pce_);
    initialized_ = 1;
    return 1;
}

int HWControlExpert::scan_all_components() {
    if (!initialized_) init();
    for (int i = 0; i < HW_MAX; i++)
        scan_component((HWComponent)i);
    return 1;
}

int HWControlExpert::scan_component(HWComponent comp) {
    HWComponentInfo *info = &components_[comp];
    info->component = comp;
    info->interfaces.clear();

    switch (comp) {
        case HW_TOUCH: return scan_touch_component(info);
        case HW_DISPLAY: return scan_display_component(info);
        case HW_BATTERY: return scan_battery_component(info);
        case HW_PMIC: return scan_pmic_component(info);
        case HW_GPIO: return scan_gpio_component(info);
        case HW_I2C: return scan_i2c_component(info);
        case HW_PINCTRL: return scan_pinctrl_component(info);
        case HW_MEMORY: return scan_memory_component(info);
        default: return 0;
    }
}

int HWControlExpert::scan_touch_component(HWComponentInfo *info) {
    info->name = "Touch Controller";
    int n = tfb_find_touch_input(&td_, 1);
    if (n > 0) {
        info->available = 1;
        info->controllable = (td_.fd >= 0);
        info->status = td_.name;
        info->interfaces.push_back("/dev/input/event*");
        touch_found_ = 1;
        return 1;
    }
    /* Try I2C touch */
    for (int i = 0; i < ie_.nbuses && i < 2; i++) {
        ie_open_bus(&ie_, ie_.buses[i].bus_num);
        if (ie_scan_devices(&ie_, ie_.buses[i].bus_num) > 0) {
            for (int j = 0; j < ie_.ndevices; j++) {
                if (ie_.devices[j].is_touch) {
                    info->available = 1;
                    info->interfaces.push_back("I2C touch at 0x" +
                        std::to_string(ie_.devices[j].addr));
                    return 1;
                }
            }
        }
    }
    return 0;
}

int HWControlExpert::scan_display_component(HWComponentInfo *info) {
    info->name = "Display Controller";
    FrameBuffer fb;
    if (tfb_open_fb(&fb, 0)) {
        info->available = 1;
        info->controllable = 1;
        char status[128];
        snprintf(status, sizeof(status), "%dx%d %dbpp %s",
                 fb.width, fb.height, fb.bpp, fb.id);
        info->status = status;
        info->interfaces.push_back("/dev/graphics/fb0");
        tfb_close(&fb);
        return 1;
    }
    /* Try sysfs backlight */
    char path[256];
    if (she_find_backlight(&she_, path, sizeof(path))) {
        info->available = 1;
        info->controllable = 1;
        info->interfaces.push_back(path);
        return 1;
    }
    return 0;
}

int HWControlExpert::scan_battery_component(HWComponentInfo *info) {
    info->name = "Battery/Power";
    int level = she_get_battery_level(&she_);
    if (level >= 0) {
        info->available = 1;
        char status[64];
        snprintf(status, sizeof(status), "%d%%", level);
        info->status = status;
        info->interfaces.push_back("/sys/class/power_supply/*");
        return 1;
    }
    return 0;
}

int HWControlExpert::scan_pmic_component(HWComponentInfo *info) {
    info->name = "PMIC (Power Management IC)";
    if (se_.ndevices > 0) {
        info->available = 1;
        info->controllable = 1;
        info->interfaces.push_back("SPMI");
        return 1;
    }
    if (ie_.ndevices > 0) {
        for (int i = 0; i < ie_.ndevices; i++) {
            if (ie_.devices[i].is_pmic) {
                info->available = 1;
                info->controllable = 1;
                info->interfaces.push_back("I2C PMIC");
                return 1;
            }
        }
    }
    return 0;
}

int HWControlExpert::scan_gpio_component(HWComponentInfo *info) {
    info->name = "GPIO Controller";
    if (gc_.npins > 0) {
        info->available = 1;
        info->controllable = 1;
        char status[64];
        snprintf(status, sizeof(status), "%d GPIOs", gc_.npins);
        info->status = status;
        info->interfaces.push_back("/sys/class/gpio/*");
        return 1;
    }
    if (dm_.nregions > 0 && dm_find_region(&dm_, "gpio") >= 0) {
        info->available = 1;
        info->controllable = 1;
        info->interfaces.push_back("dev/mem GPIO registers");
        return 1;
    }
    return 0;
}

int HWControlExpert::scan_i2c_component(HWComponentInfo *info) {
    info->name = "I2C Bus";
    if (ie_.nbuses > 0 && ie_.ndevices > 0) {
        info->available = 1;
        info->controllable = 1;
        char status[64];
        snprintf(status, sizeof(status), "%d buses, %d devices",
                 ie_.nbuses, ie_.ndevices);
        info->status = status;
        info->interfaces.push_back("/dev/i2c-*");
        return 1;
    }
    return 0;
}

int HWControlExpert::scan_pinctrl_component(HWComponentInfo *info) {
    info->name = "Pin Controller";
    if (pce_.npins > 0) {
        info->available = 1;
        info->controllable = 1;
        char status[64];
        snprintf(status, sizeof(status), "%d pins, %d groups",
                 pce_.npins, pce_.ngroups);
        info->status = status;
        info->interfaces.push_back("/sys/kernel/debug/pinctrl");
        return 1;
    }
    return 0;
}

int HWControlExpert::scan_memory_component(HWComponentInfo *info) {
    info->name = "Physical Memory";
    if (dm_.nregions > 0) {
        info->available = 1;
        info->controllable = (dm_.fd_mem >= 0);
        char status[64];
        snprintf(status, sizeof(status), "%d regions via /proc/iomem",
                 dm_.nregions);
        info->status = status;
        info->interfaces.push_back("/dev/mem");
        info->interfaces.push_back("/proc/kcore");
        return 1;
    }
    return 0;
}

int HWControlExpert::control_component(HWComponent comp, const std::string &action, const std::string &param) {
    (void)comp; (void)action; (void)param;
    return 0;
}

int HWControlExpert::get_component_info(HWComponent comp, HWComponentInfo *info) {
    if (comp >= HW_MAX) return 0;
    *info = components_[comp];
    return info->available;
}

int HWControlExpert::touch_tap(int x, int y) {
    if (!touch_found_ || td_.fd < 0) return 0;
    return tfb_tap_screen(&td_, x, y);
}

int HWControlExpert::touch_swipe(int x1, int y1, int x2, int y2, int steps) {
    if (!touch_found_ || td_.fd < 0) return 0;
    return tfb_swipe(&td_, x1, y1, x2, y2, steps, 1000);
}

int HWControlExpert::touch_press_digit(int digit, int grid_x, int grid_y, int key_w, int key_h) {
    if (!touch_found_ || td_.fd < 0) return 0;
    return tfb_digit_press(&td_, digit, grid_x, grid_y, key_w, key_h);
}

int HWControlExpert::touch_swipe_pattern(const std::vector<int> &x_pts, const std::vector<int> &y_pts) {
    if (!touch_found_ || td_.fd < 0) return 0;
    return tfb_swipe_pattern(&td_, (int*)x_pts.data(), (int*)y_pts.data(),
                              (int)x_pts.size(), 1000);
}

int HWControlExpert::display_set_brightness(int percent) {
    return she_set_brightness(&she_, percent);
}

int HWControlExpert::display_get_info(int *w, int *h, int *bpp) {
    FrameBuffer fb;
    if (!tfb_open_fb(&fb, 0)) return 0;
    *w = fb.width; *h = fb.height; *bpp = fb.bpp;
    tfb_close(&fb);
    return 1;
}

int HWControlExpert::gpio_press_power() { return gc_press_power(&gc_); }
int HWControlExpert::gpio_press_vol_up() { return gc_press_vol_up(&gc_); }
int HWControlExpert::gpio_press_vol_down() { return gc_press_vol_down(&gc_); }
int HWControlExpert::gpio_set_value(int gpio, int val) { return gc_set_value(&gc_, gpio, val); }
int HWControlExpert::gpio_get_value(int gpio) { return gc_get_value(&gc_, gpio); }

int HWControlExpert::i2c_read(int bus, int addr, int reg, unsigned char *val) {
    return ie_read_byte(&ie_, bus, addr, reg, val);
}
int HWControlExpert::i2c_write(int bus, int addr, int reg, unsigned char val) {
    return ie_write_byte(&ie_, bus, addr, reg, val);
}

int HWControlExpert::pmic_read_reg(int sid, int pid, unsigned short addr, unsigned char *val) {
    return se_reg_read(&se_, sid, pid, addr, val);
}
int HWControlExpert::pmic_set_regulator(int sid, int pid, unsigned short addr, int enable) {
    return se_pmic_set_regulator(&se_, sid, pid, addr, enable);
}

int HWControlExpert::usb_set_mode(const std::string &mode) {
    return she_toggle_usb_mode(&she_, mode.c_str());
}
int HWControlExpert::usb_reset() {
    return she_toggle_usb_mode(&she_, "0");
}

int HWControlExpert::battery_get_level() {
    return she_get_battery_level(&she_);
}
int HWControlExpert::battery_get_temp(float *temp) {
    return she_get_battery_temp(&she_, temp);
}
int HWControlExpert::battery_get_status(std::string &status) {
    char buf[64];
    if (!she_get_battery_status(&she_, buf, sizeof(buf))) return 0;
    status = buf;
    return 1;
}

int HWControlExpert::is_component_available(HWComponent comp) const {
    if (comp >= HW_MAX) return 0;
    return components_[comp].available;
}

void HWControlExpert::print_all_components() const {
    printf("\n" BOLD "=== Hardware Components ===" RESET "\n");
    for (int i = 0; i < HW_MAX; i++) {
        const HWComponentInfo *info = &components_[i];
        printf("  %s: %s [%s]\n",
               info->name.c_str(),
               info->available ? GREEN "Available" RESET : RED "N/A" RESET,
               info->controllable ? "Controllable" : "Read-only");
        if (info->available) {
            if (!info->status.empty())
                printf("    Status: %s\n", info->status.c_str());
            for (const auto &iface : info->interfaces)
                printf("    Interface: %s\n", iface.c_str());
        }
    }
}
