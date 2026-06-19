#ifndef HW_CONTROL_EXPERT_H
#define HW_CONTROL_EXPERT_H

#include "kernel_direct.h"
#include "devmem_exploit.h"
#include "gpio_button_exploit.h"
#include "i2c_bus_exploit.h"
#include "spmi_exploit.h"
#include "sysfs_hw_exploit.h"
#include "pinctrl_exploit.h"
#include "touch_fb_exploit.h"
#include <string>
#include <vector>

typedef enum {
    HW_TOUCH,
    HW_DISPLAY,
    HW_BATTERY,
    HW_PMIC,
    HW_GPIO,
    HW_I2C,
    HW_SPMI,
    HW_USB,
    HW_WIFI,
    HW_BLUETOOTH,
    HW_SENSOR,
    HW_CAMERA,
    HW_VIBRATOR,
    HW_BACKLIGHT,
    HW_PINCTRL,
    HW_MEMORY,
    HW_MAX
} HWComponent;

typedef struct {
    HWComponent component;
    std::string name;
    int available;
    int controllable;
    std::string status;
    std::vector<std::string> interfaces;
} HWComponentInfo;

class HWControlExpert {
public:
    HWControlExpert();
    ~HWControlExpert();

    int init();
    int scan_all_components();
    int scan_component(HWComponent comp);
    int control_component(HWComponent comp, const std::string &action, const std::string &param);
    int get_component_info(HWComponent comp, HWComponentInfo *info);

    /* Touch control */
    int touch_tap(int x, int y);
    int touch_swipe(int x1, int y1, int x2, int y2, int steps);
    int touch_press_digit(int digit, int grid_x, int grid_y, int key_w, int key_h);
    int touch_swipe_pattern(const std::vector<int> &x_pts, const std::vector<int> &y_pts);

    /* Display control */
    int display_set_brightness(int percent);
    int display_get_info(int *w, int *h, int *bpp);

    /* GPIO control */
    int gpio_press_power();
    int gpio_press_vol_up();
    int gpio_press_vol_down();
    int gpio_set_value(int gpio, int val);
    int gpio_get_value(int gpio);

    /* I2C access */
    int i2c_read(int bus, int addr, int reg, unsigned char *val);
    int i2c_write(int bus, int addr, int reg, unsigned char val);

    /* PMIC control */
    int pmic_read_reg(int sid, int pid, unsigned short addr, unsigned char *val);
    int pmic_set_regulator(int sid, int pid, unsigned short addr, int enable);

    /* USB control */
    int usb_set_mode(const std::string &mode);
    int usb_reset();

    /* Battery */
    int battery_get_level();
    int battery_get_temp(float *temp);
    int battery_get_status(std::string &status);

    /* General */
    int is_component_available(HWComponent comp) const;
    std::string get_last_error() const { return last_error_; }
    void print_all_components() const;

private:
    KernelDirect kd_;
    DevMemExploit dm_;
    GpioController gc_;
    I2CExploit ie_;
    SPMIExploit se_;
    SysfsHWExploit she_;
    PinCtrlExploit pce_;
    TouchDevice td_;

    HWComponentInfo components_[HW_MAX];
    int initialized_;
    std::string last_error_;
    int touch_found_;

    int scan_touch_component(HWComponentInfo *info);
    int scan_display_component(HWComponentInfo *info);
    int scan_battery_component(HWComponentInfo *info);
    int scan_pmic_component(HWComponentInfo *info);
    int scan_gpio_component(HWComponentInfo *info);
    int scan_i2c_component(HWComponentInfo *info);
    int scan_memory_component(HWComponentInfo *info);
    int scan_pinctrl_component(HWComponentInfo *info);
};

#endif
