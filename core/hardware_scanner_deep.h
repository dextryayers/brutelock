#ifndef HARDWARE_SCANNER_DEEP_H
#define HARDWARE_SCANNER_DEEP_H

#include <string>
#include <vector>
#include <string.h>
#include "util.h"
#include "devmem_exploit.h"
#include "sysfs_hw_exploit.h"
#include "soc_exploit_engine.h"
#include "trustzone_pwn.h"

typedef struct {
    /* SoC / Chipset */
    std::string soc_vendor;
    std::string soc_model;
    std::string soc_revision;
    std::string cpu_arch;
    std::string cpu_cores;
    std::string cpu_abi;
    std::string cpu_max_freq;
    std::string cpu_min_freq;
    std::string cpu_governor;
    std::string gpu_vendor;
    std::string gpu_model;
    std::string gpu_freq;

    /* Memory */
    std::string ram_total;
    std::string ram_type;
    std::string ram_freq;
    std::string storage_total;
    std::string storage_type;
    std::string swap_total;

    /* Display */
    std::string display_resolution;
    std::string display_density;
    std::string display_refresh;
    std::string display_hdr;
    std::string display_panel;
    std::string display_brightness;
    std::string display_manufacturer;

    /* Battery */
    std::string battery_level;
    std::string battery_temp;
    std::string battery_status;
    std::string battery_health;
    std::string battery_capacity;
    std::string battery_technology;
    std::string battery_voltage;
    std::string battery_current;

    /* Telephony */
    std::string network_type;
    std::string network_operator;
    std::string imei;
    std::string imei2;
    std::string meid;
    std::string iccid;
    std::string signal_strength;
    std::string sim_state;

    /* Connectivity */
    std::string wifi_mac;
    std::string wifi_state;
    std::string wifi_ip;
    std::string bt_mac;
    std::string bt_state;
    std::string nfc_state;

    /* Sensors */
    std::vector<std::string> sensors;
    std::vector<std::string> sensor_values;

    /* Biometrics */
    std::string fingerprint_sensor;
    std::string face_unlock;
    std::string iris_scanner;

    /* Camera */
    std::vector<std::string> cameras;
    std::vector<std::string> camera_resolutions;

    /* Security */
    std::string selinux_mode;
    std::string encryption_state;
    std::string verified_boot;
    std::string oem_unlock;
    std::string tz_type;
    std::string knox_version;
} DeepHWInfo;

class HardwareScannerDeep {
public:
    HardwareScannerDeep();
    ~HardwareScannerDeep();

    int scan_all(DeepHWInfo *info);
    int scan_soc(DeepHWInfo *info);
    int scan_cpu(DeepHWInfo *info);
    int scan_gpu(DeepHWInfo *info);
    int scan_memory(DeepHWInfo *info);
    int scan_storage(DeepHWInfo *info);
    int scan_display(DeepHWInfo *info);
    int scan_battery(DeepHWInfo *info);
    int scan_telephony(DeepHWInfo *info);
    int scan_connectivity(DeepHWInfo *info);
    int scan_sensors(DeepHWInfo *info);
    int scan_biometrics(DeepHWInfo *info);
    int scan_camera(DeepHWInfo *info);
    int scan_security(DeepHWInfo *info);

    void print_deep_info(const DeepHWInfo *info) const;
    void fill_device_info(DeviceInfo *dinfo) const;
    std::string get_last_error() const { return last_error_; }

    SocExploitEngine *get_soc_engine() { return &soc_engine_; }
    TrustZonePwn *get_tz_pwn() { return &tz_pwn_; }
    DevMemExploit *get_devmem() { return &dm_; }
    SysfsHWExploit *get_sysfs() { return &she_; }

private:
    DeepHWInfo hw_info_;
    SocExploitEngine soc_engine_;
    TrustZonePwn tz_pwn_;
    DevMemExploit dm_;
    SPMIExploit se_;
    SysfsHWExploit she_;
    std::string last_error_;

    std::string read_sysfs(const char *path);
    std::string read_prop(const char *prop);
    std::string read_adb(const char *cmd);
    int read_int_sysfs(const char *path);
};

#endif
