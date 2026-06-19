#include "hardware_scanner_deep.h"
#include "touch_fb_exploit.h"
#include "adb_native.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>

HardwareScannerDeep::HardwareScannerDeep() {
    dm_init(&dm_);
    se_init(&se_);
    she_init(&she_);
}

HardwareScannerDeep::~HardwareScannerDeep() {
    dm_close(&dm_);
    se_close(&se_);
}

std::string HardwareScannerDeep::read_sysfs(const char *path) {
    char buf[4096] = {0};
    int f = open(path, O_RDONLY);
    if (f < 0) return "";
    int n = read(f, buf, sizeof(buf)-1);
    close(f);
    if (n > 0) {
        buf[n] = 0;
        char *nl = strchr(buf, '\n');
        if (nl) *nl = 0;
    }
    return std::string(buf);
}

std::string HardwareScannerDeep::read_prop(const char *prop) {
    char buf[4096];
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "getprop %s 2>/dev/null", prop);
    if (adb_native_shell(cmd, buf, sizeof(buf)) != 0) return "";
    trim_newline(buf);
    return std::string(buf);
}

std::string HardwareScannerDeep::read_adb(const char *cmd) {
    char buf[16384];
    if (adb_native_shell(cmd, buf, sizeof(buf)) != 0) return "";
    trim_newline(buf);
    return std::string(buf);
}

int HardwareScannerDeep::read_int_sysfs(const char *path) {
    std::string s = read_sysfs(path);
    return s.empty() ? 0 : atoi(s.c_str());
}

int HardwareScannerDeep::scan_all(DeepHWInfo *info) {
    DeepHWInfo tmp;
    she_scan_all(&she_);
    dm_open_mem(&dm_);
    dm_scan_iomem(&dm_);
    se_scan_spmi_devices(&se_);

    scan_soc(&tmp);
    scan_cpu(&tmp);
    scan_gpu(&tmp);
    scan_memory(&tmp);
    scan_storage(&tmp);
    scan_display(&tmp);
    scan_battery(&tmp);
    scan_telephony(&tmp);
    scan_connectivity(&tmp);
    scan_sensors(&tmp);
    scan_biometrics(&tmp);
    scan_camera(&tmp);
    scan_security(&tmp);

    *info = tmp;
    hw_info_ = tmp;
    return 1;
}

int HardwareScannerDeep::scan_soc(DeepHWInfo *info) {
    SocInfo si;
    soc_engine_.detect_soc(&si);
    info->soc_vendor = si.manufacturer;
    info->soc_model = si.model.empty() ? si.name : si.model;

    if (info->soc_vendor.empty())
        info->soc_vendor = read_prop("ro.chipset.vendor");
    if (info->soc_vendor.empty())
        info->soc_vendor = read_prop("ro.hardware.chipset");

    if (info->soc_model.empty())
        info->soc_model = read_sysfs("/sys/devices/soc0/machine");
    if (info->soc_model.empty())
        info->soc_model = read_sysfs("/sys/devices/soc0/soc_id");

    info->soc_revision = read_sysfs("/sys/devices/soc0/revision");

    /* Detect SoC from /proc/cpuinfo */
    std::string cpuinfo = read_adb("cat /proc/cpuinfo 2>/dev/null | grep -i 'Hardware\\|Processor\\|model name' | head -3");
    if (info->soc_model.empty() && !cpuinfo.empty()) {
        /* Extract Hardware line */
        size_t pos = cpuinfo.find("Hardware");
        if (pos != std::string::npos) {
            pos = cpuinfo.find(": ", pos);
            if (pos != std::string::npos) {
                info->soc_model = cpuinfo.substr(pos + 2);
                pos = info->soc_model.find('\n');
                if (pos != std::string::npos) info->soc_model = info->soc_model.substr(0, pos);
            }
        }
    }

    /* Detect SoC type from compatible DT */
    std::string compatible = read_sysfs("/sys/firmware/devicetree/base/compatible");
    if (!compatible.empty() && info->soc_vendor.empty()) {
        if (compatible.find("qcom") != std::string::npos) info->soc_vendor = "Qualcomm";
        else if (compatible.find("mediatek") != std::string::npos) info->soc_vendor = "MediaTek";
        else if (compatible.find("samsung") != std::string::npos) info->soc_vendor = "Samsung";
        else if (compatible.find("hisilicon") != std::string::npos) info->soc_vendor = "HiSilicon";
        else if (compatible.find("amlogic") != std::string::npos) info->soc_vendor = "Amlogic";
        else if (compatible.find("rockchip") != std::string::npos) info->soc_vendor = "Rockchip";
        else if (compatible.find("allwinner") != std::string::npos) info->soc_vendor = "Allwinner";
        else if (compatible.find("broadcom") != std::string::npos) info->soc_vendor = "Broadcom";
        else info->soc_vendor = compatible.substr(0, compatible.find(','));
    }
    return 1;
}

int HardwareScannerDeep::scan_cpu(DeepHWInfo *info) {
    info->cpu_arch = read_prop("ro.product.cpu.abi");
    if (info->cpu_arch.empty())
        info->cpu_arch = read_prop("ro.product.cpu.abi2");
    if (info->cpu_arch.empty())
        info->cpu_arch = read_sysfs("/sys/devices/system/cpu/cpu0/cpuinfo_min_freq");

    /* Read CPU cores */
    info->cpu_cores = read_sysfs("/sys/devices/system/cpu/present");
    if (!info->cpu_cores.empty()) {
        /* Parse "0-7" or "0,1,2,3" */
        if (info->cpu_cores[0] == '0') {
            size_t dash = info->cpu_cores.find('-');
            if (dash != std::string::npos) {
                int max = atoi(info->cpu_cores.substr(dash + 1).c_str());
                info->cpu_cores = std::to_string(max + 1);
            }
        }
    }

    /* CPU max frequency */
    info->cpu_max_freq = read_sysfs("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq");
    if (!info->cpu_max_freq.empty()) {
        int khz = atoi(info->cpu_max_freq.c_str());
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f GHz", khz / 1000000.0f);
        info->cpu_max_freq = buf;
    }

    info->cpu_min_freq = read_sysfs("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_min_freq");
    if (!info->cpu_min_freq.empty()) {
        int khz = atoi(info->cpu_min_freq.c_str());
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f GHz", khz / 1000000.0f);
        info->cpu_min_freq = buf;
    }

    info->cpu_governor = read_sysfs("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");

    /* CPU ABI details */
    std::string abi_list = read_prop("ro.product.cpu.abilist");
    if (!abi_list.empty()) info->cpu_abi = abi_list;
    else info->cpu_abi = info->cpu_arch;

    return 1;
}

int HardwareScannerDeep::scan_gpu(DeepHWInfo *info) {
    info->gpu_model = read_sysfs("/sys/class/kgsl/kgsl-3d0/gpu_model");
    if (info->gpu_model.empty())
        info->gpu_model = read_sysfs("/sys/class/kgsl/kgsl-3d0/devfreq/available_frequencies");

    info->gpu_freq = read_sysfs("/sys/class/kgsl/kgsl-3d0/gpuclk");
    if (info->gpu_freq.empty())
        info->gpu_freq = read_sysfs("/sys/class/kgsl/kgsl-3d0/devfreq/cur_freq");

    /* Try Mali */
    if (info->gpu_model.empty()) {
        DIR *d = opendir("/sys/class/misc");
        if (d) {
            struct dirent *e;
            while ((e = readdir(d))) {
                if (strstr(e->d_name, "mali")) {
                    info->gpu_vendor = "ARM";
                    info->gpu_model = "Mali";
                    break;
                }
            }
            closedir(d);
        }
    }

    if (info->gpu_model.empty())
        info->gpu_model = read_prop("ro.hardware.gpu");
    if (info->gpu_model.empty())
        info->gpu_model = read_prop("debug.gpu.model");

    if (info->gpu_vendor.empty()) {
        std::string m = info->gpu_model;
        if (m.find("Adreno") != std::string::npos) info->gpu_vendor = "Qualcomm";
        else if (m.find("Mali") != std::string::npos) info->gpu_vendor = "ARM";
        else if (m.find("PowerVR") != std::string::npos) info->gpu_vendor = "Imagination";
        else if (m.find("Xclipse") != std::string::npos) info->gpu_vendor = "Samsung/AMD";
    }

    return 1;
}

int HardwareScannerDeep::scan_memory(DeepHWInfo *info) {
    /* RAM total via proc */
    std::string meminfo = read_adb("cat /proc/meminfo 2>/dev/null | grep 'MemTotal' | head -1");
    if (!meminfo.empty()) {
        int kb = 0;
        sscanf(meminfo.c_str(), "MemTotal: %d kB", &kb);
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f GB", kb / 1048576.0f);
        info->ram_total = buf;
    }

    info->ram_type = read_sysfs("/sys/devices/system/edac/mc/mc0/ce_count");
    if (info->ram_type.empty())
        info->ram_type = "LPDDR"; /* Default for mobile */

    info->ram_freq = read_sysfs("/sys/class/memory/memory0/device/clock");
    if (info->ram_freq.empty())
        info->ram_freq = read_prop("ro.boot.ddrsize");

    return 1;
}

int HardwareScannerDeep::scan_storage(DeepHWInfo *info) {
    std::string df = read_adb("df /data 2>/dev/null | tail -1");
    if (!df.empty()) {
        unsigned long total = 0;
        sscanf(df.c_str(), "%*s %lu", &total);
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f GB", total / 1048576.0f);
        info->storage_total = buf;
    }

    info->storage_type = read_sysfs("/sys/block/mmcblk0/device/type");
    if (info->storage_type.empty())
        info->storage_type = read_sysfs("/sys/block/sda/queue/rotational");
    if (info->storage_type == "0") info->storage_type = "UFS";
    else if (info->storage_type.empty()) info->storage_type = "eMMC";

    /* Swap */
    std::string swap = read_adb("cat /proc/meminfo | grep 'SwapTotal' | head -1");
    if (!swap.empty()) {
        int kb = 0;
        sscanf(swap.c_str(), "SwapTotal: %d kB", &kb);
        if (kb > 0) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.1f GB", kb / 1048576.0f);
            info->swap_total = buf;
        }
    }

    return 1;
}

int HardwareScannerDeep::scan_display(DeepHWInfo *info) {
    FrameBuffer fb;
    if (tfb_open_fb(&fb, 0)) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%dx%d", fb.width, fb.height);
        info->display_resolution = buf;
        tfb_close(&fb);
    }

    if (info->display_resolution.empty()) {
        std::string size = read_adb("wm size 2>/dev/null | head -1");
        if (!size.empty()) {
            size_t pos = size.find("Physical size: ");
            if (pos != std::string::npos) {
                info->display_resolution = size.substr(pos + 15);
            }
        }
    }

    info->display_density = read_adb("wm density 2>/dev/null | head -1");
    if (!info->display_density.empty()) {
        size_t pos = info->display_density.find("Physical density: ");
        if (pos != std::string::npos)
            info->display_density = info->display_density.substr(pos + 18);
        pos = info->display_density.find(": ");
        if (pos != std::string::npos)
            info->display_density = info->display_density.substr(pos + 2);
    }

    /* Refresh rate */
    info->display_refresh = read_sysfs("/sys/class/graphics/fb0/modes");
    if (info->display_refresh.empty())
        info->display_refresh = read_adb("settings get system peak_refresh_rate 2>/dev/null");

    /* Panel info */
    info->display_panel = read_sysfs("/sys/class/graphics/fb0/panel_info");
    if (info->display_panel.empty())
        info->display_panel = read_sysfs("/sys/class/graphics/fb0/panel_name");

    /* HDR */
    info->display_hdr = read_sysfs("/sys/class/graphics/fb0/hdr_capability");
    if (!info->display_hdr.empty() && info->display_hdr.find("1") != std::string::npos)
        info->display_hdr = "HDR10+";

    /* Brightness */
    int bl = read_int_sysfs("/sys/class/backlight/panel0-backlight/brightness");
    int max_bl = read_int_sysfs("/sys/class/backlight/panel0-backlight/max_brightness");
    if (max_bl > 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d%%", bl * 100 / max_bl);
        info->display_brightness = buf;
    }

    return 1;
}

int HardwareScannerDeep::scan_battery(DeepHWInfo *info) {
    int level = she_get_battery_level(&she_);
    if (level >= 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d%%", level);
        info->battery_level = buf;
    }

    float temp;
    if (she_get_battery_temp(&she_, &temp)) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f", temp);
        info->battery_temp = buf;
    }

    char status[64];
    if (she_get_battery_status(&she_, status, sizeof(status)))
        info->battery_status = status;

    info->battery_health = read_sysfs("/sys/class/power_supply/battery/health");
    info->battery_capacity = read_sysfs("/sys/class/power_supply/battery/capacity");
    info->battery_technology = read_sysfs("/sys/class/power_supply/battery/technology");
    info->battery_voltage = read_sysfs("/sys/class/power_supply/battery/voltage_now");
    info->battery_current = read_sysfs("/sys/class/power_supply/battery/current_now");

    if (!info->battery_voltage.empty()) {
        int uv = atoi(info->battery_voltage.c_str());
        char buf[32];
        snprintf(buf, sizeof(buf), "%.3f V", uv / 1000000.0f);
        info->battery_voltage = buf;
    }
    if (!info->battery_current.empty()) {
        int ua = atoi(info->battery_current.c_str());
        char buf[32];
        snprintf(buf, sizeof(buf), "%d mA", abs(ua) / 1000);
        info->battery_current = buf;
    }

    return 1;
}

int HardwareScannerDeep::scan_telephony(DeepHWInfo *info) {
    info->network_type = read_adb("getprop gsm.network.type 2>/dev/null || getprop ro.telephony.default_network 2>/dev/null");
    info->network_operator = read_adb("getprop gsm.operator.alpha 2>/dev/null || getprop gsm.operator.numeric 2>/dev/null");
    info->imei = read_adb("getprop gsm.baseband.imei 2>/dev/null || getprop ril.imei 2>/dev/null");
    info->imei2 = read_adb("getprop ril.imei2 2>/dev/null || getprop gsm.baseband.imei2 2>/dev/null");
    info->meid = read_adb("getprop ril.meid 2>/dev/null");
    info->iccid = read_adb("getprop ril.iccid 2>/dev/null || getprop gsm.sim.iccid 2>/dev/null");
    info->signal_strength = read_adb("dumpsys telephony 2>/dev/null | grep -i 'signalStrength\\|mSignalStrength' | head -1");
    info->sim_state = read_adb("getprop gsm.sim.state 2>/dev/null");

    return 1;
}

int HardwareScannerDeep::scan_connectivity(DeepHWInfo *info) {
    info->wifi_mac = read_sysfs("/sys/class/net/wlan0/address");
    if (info->wifi_mac.empty())
        info->wifi_mac = read_sysfs("/sys/class/net/wlan0/addr");

    info->wifi_state = read_adb("dumpsys wifi 2>/dev/null | grep 'Wi-Fi is' | head -1");
    if (info->wifi_state.empty())
        info->wifi_state = read_sysfs("/sys/class/net/wlan0/operstate");

    info->wifi_ip = read_adb("getprop dhcp.wlan0.ipaddress 2>/dev/null");

    info->bt_mac = read_sysfs("/sys/class/bluetooth/hci0/address");
    if (info->bt_mac.empty())
        info->bt_mac = read_adb("getprop bt.address 2>/dev/null");

    info->bt_state = read_adb("dumpsys bluetooth 2>/dev/null | grep 'state:' | head -1");
    info->nfc_state = read_adb("dumpsys nfc 2>/dev/null | grep 'mState' | head -1");

    return 1;
}

int HardwareScannerDeep::scan_sensors(DeepHWInfo *info) {
    info->sensors.clear();
    info->sensor_values.clear();

    DIR *d = opendir("/sys/class/sensor");
    if (!d) d = opendir("/sys/class/iio");
    if (!d) d = opendir("/sys/bus/iio/devices");
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (e->d_name[0] == '.') continue;
            info->sensors.push_back(e->d_name);
        }
        closedir(d);
    }

    /* Also read from dumpsys */
    std::string sensors = read_adb("dumpsys sensorservice 2>/dev/null | head -30");
    if (!sensors.empty()) {
        info->sensor_values.push_back(sensors);
    }

    if (info->sensors.empty())
        info->sensors.push_back("Accelerometer, Gyroscope, Magnetometer, Light, Proximity");

    return 1;
}

int HardwareScannerDeep::scan_biometrics(DeepHWInfo *info) {
    info->fingerprint_sensor = read_adb("getprop ro.boot.fingerprint 2>/dev/null");
    if (info->fingerprint_sensor.empty())
        info->fingerprint_sensor = read_sysfs("/sys/class/fingerprint/fingerprint/name");

    if (read_adb("dumpsys display 2>/dev/null | grep -i 'face\\|faceunlock' | head -1").size() > 0)
        info->face_unlock = "Available";

    if (read_adb("dumpsys display 2>/dev/null | grep -i 'iris' | head -1").size() > 0)
        info->iris_scanner = "Available";

    /* Check for fingerprint HAL */
    if (info->fingerprint_sensor.empty() &&
        read_adb("ls /vendor/lib64/hw/fingerprint.* 2>/dev/null || ls /system/lib64/hw/fingerprint.* 2>/dev/null || echo NONE").find("NONE") == std::string::npos) {
        info->fingerprint_sensor = "Generic (HAL detected)";
    }

    return 1;
}

int HardwareScannerDeep::scan_camera(DeepHWInfo *info) {
    info->cameras.clear();
    info->camera_resolutions.clear();

    DIR *d = opendir("/sys/class/camera");
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (e->d_name[0] == '.') continue;
            info->cameras.push_back(e->d_name);
        }
        closedir(d);
    }

    if (info->cameras.empty()) {
        std::string cam_list = read_adb("dumpsys media.camera 2>/dev/null | grep '\\- id:' | head -5");
        if (!cam_list.empty()) {
            /* Parse camera IDs */
            size_t pos = 0;
            while ((pos = cam_list.find("- id:", pos)) != std::string::npos) {
                size_t end = cam_list.find('\n', pos);
                info->cameras.push_back(cam_list.substr(pos, end - pos));
                pos = end + 1;
            }
        }
    }

    if (info->cameras.empty())
        info->cameras.push_back("Back, Front");

    return 1;
}

int HardwareScannerDeep::scan_security(DeepHWInfo *info) {
    info->selinux_mode = read_adb("getprop ro.boot.selinux 2>/dev/null || getprop ro.build.selinux 2>/dev/null");
    if (info->selinux_mode.empty()) {
        std::string enforce = read_sysfs("/sys/fs/selinux/enforce");
        if (!enforce.empty())
            info->selinux_mode = (enforce == "1") ? "Enforcing" : "Permissive";
    }

    info->encryption_state = read_adb("getprop ro.crypto.state 2>/dev/null");
    info->verified_boot = read_adb("getprop ro.boot.verifiedbootstate 2>/dev/null");
    info->oem_unlock = read_adb("getprop ro.oem_unlock_supported 2>/dev/null");

    /* TrustZone */
    TrustZoneInfo tzi;
    tz_pwn_.detect_trustzone(&tzi);
    info->tz_type = tzi.name;

    /* Knox (Samsung) */
    info->knox_version = read_adb("getprop ro.boot.knox 2>/dev/null || getprop ro.config.knox 2>/dev/null");

    return 1;
}

void HardwareScannerDeep::print_deep_info(const DeepHWInfo *info) const {
    printf("\n" BOLD "========== DEEP HARDWARE SCAN ==========" RESET "\n");

    printf("\n-- " BOLD "SoC / Chipset" RESET " --\n");
    printf("  Vendor    : %s\n", info->soc_vendor.empty() ? "?" : info->soc_vendor.c_str());
    printf("  Model     : %s\n", info->soc_model.empty() ? "?" : info->soc_model.c_str());
    printf("  Revision  : %s\n", info->soc_revision.empty() ? "?" : info->soc_revision.c_str());

    printf("\n-- " BOLD "CPU" RESET " --\n");
    printf("  Arch      : %s\n", info->cpu_arch.empty() ? "?" : info->cpu_arch.c_str());
    printf("  Cores     : %s\n", info->cpu_cores.empty() ? "?" : info->cpu_cores.c_str());
    printf("  ABI       : %s\n", info->cpu_abi.empty() ? "?" : info->cpu_abi.c_str());
    printf("  Max Freq  : %s\n", info->cpu_max_freq.empty() ? "?" : info->cpu_max_freq.c_str());
    printf("  Min Freq  : %s\n", info->cpu_min_freq.empty() ? "?" : info->cpu_min_freq.c_str());
    printf("  Governor  : %s\n", info->cpu_governor.empty() ? "?" : info->cpu_governor.c_str());

    printf("\n-- " BOLD "GPU" RESET " --\n");
    printf("  Vendor    : %s\n", info->gpu_vendor.empty() ? "?" : info->gpu_vendor.c_str());
    printf("  Model     : %s\n", info->gpu_model.empty() ? "?" : info->gpu_model.c_str());
    printf("  Freq      : %s\n", info->gpu_freq.empty() ? "?" : info->gpu_freq.c_str());

    printf("\n-- " BOLD "Memory" RESET " --\n");
    printf("  RAM       : %s\n", info->ram_total.empty() ? "?" : info->ram_total.c_str());
    printf("  RAM Type  : %s\n", info->ram_type.empty() ? "?" : info->ram_type.c_str());
    printf("  RAM Freq  : %s\n", info->ram_freq.empty() ? "?" : info->ram_freq.c_str());
    printf("  Storage   : %s\n", info->storage_total.empty() ? "?" : info->storage_total.c_str());
    printf("  Stor Type : %s\n", info->storage_type.empty() ? "?" : info->storage_type.c_str());
    printf("  Swap      : %s\n", info->swap_total.empty() ? "?" : info->swap_total.c_str());

    printf("\n-- " BOLD "Display" RESET " --\n");
    printf("  Resolution: %s\n", info->display_resolution.empty() ? "?" : info->display_resolution.c_str());
    printf("  Density   : %s\n", info->display_density.empty() ? "?" : info->display_density.c_str());
    printf("  Refresh   : %s\n", info->display_refresh.empty() ? "?" : info->display_refresh.c_str());
    printf("  HDR       : %s\n", info->display_hdr.empty() ? "?" : info->display_hdr.c_str());
    printf("  Panel     : %s\n", info->display_panel.empty() ? "?" : info->display_panel.c_str());
    printf("  Brightness: %s\n", info->display_brightness.empty() ? "?" : info->display_brightness.c_str());

    printf("\n-- " BOLD "Battery" RESET " --\n");
    printf("  Level     : %s\n", info->battery_level.empty() ? "?" : info->battery_level.c_str());
    printf("  Temp      : %s\n", info->battery_temp.empty() ? "?" : info->battery_temp.c_str());
    printf("  Status    : %s\n", info->battery_status.empty() ? "?" : info->battery_status.c_str());
    printf("  Health    : %s\n", info->battery_health.empty() ? "?" : info->battery_health.c_str());
    printf("  Capacity  : %s\n", info->battery_capacity.empty() ? "?" : info->battery_capacity.c_str());
    printf("  Tech      : %s\n", info->battery_technology.empty() ? "?" : info->battery_technology.c_str());
    printf("  Voltage   : %s\n", info->battery_voltage.empty() ? "?" : info->battery_voltage.c_str());
    printf("  Current   : %s\n", info->battery_current.empty() ? "?" : info->battery_current.c_str());

    printf("\n-- " BOLD "Telephony" RESET " --\n");
    printf("  Network   : %s\n", info->network_type.empty() ? "?" : info->network_type.c_str());
    printf("  Operator  : %s\n", info->network_operator.empty() ? "?" : info->network_operator.c_str());
    printf("  IMEI      : %s\n", info->imei.empty() ? "?" : info->imei.c_str());
    printf("  IMEI2     : %s\n", info->imei2.empty() ? "?" : info->imei2.c_str());
    printf("  MEID      : %s\n", info->meid.empty() ? "?" : info->meid.c_str());
    printf("  ICCID     : %s\n", info->iccid.empty() ? "?" : info->iccid.c_str());
    printf("  Signal    : %s\n", info->signal_strength.empty() ? "?" : info->signal_strength.c_str());
    printf("  SIM State : %s\n", info->sim_state.empty() ? "?" : info->sim_state.c_str());

    printf("\n-- " BOLD "Connectivity" RESET " --\n");
    printf("  WiFi MAC  : %s\n", info->wifi_mac.empty() ? "?" : info->wifi_mac.c_str());
    printf("  WiFi State: %s\n", info->wifi_state.empty() ? "?" : info->wifi_state.c_str());
    printf("  WiFi IP   : %s\n", info->wifi_ip.empty() ? "?" : info->wifi_ip.c_str());
    printf("  BT MAC    : %s\n", info->bt_mac.empty() ? "?" : info->bt_mac.c_str());
    printf("  BT State  : %s\n", info->bt_state.empty() ? "?" : info->bt_state.c_str());
    printf("  NFC State : %s\n", info->nfc_state.empty() ? "?" : info->nfc_state.c_str());

    printf("\n-- " BOLD "Sensors (%zu)" RESET " --\n", info->sensors.size());
    for (const auto &s : info->sensors)
        printf("  - %s\n", s.c_str());

    printf("\n-- " BOLD "Biometrics" RESET " --\n");
    printf("  Fingerprint: %s\n", info->fingerprint_sensor.empty() ? "?" : info->fingerprint_sensor.c_str());
    printf("  Face Unlock: %s\n", info->face_unlock.empty() ? "?" : info->face_unlock.c_str());
    printf("  Iris       : %s\n", info->iris_scanner.empty() ? "?" : info->iris_scanner.c_str());

    printf("\n-- " BOLD "Camera (%zu)" RESET " --\n", info->cameras.size());
    for (const auto &c : info->cameras)
        printf("  - %s\n", c.c_str());

    printf("\n-- " BOLD "Security" RESET " --\n");
    printf("  SELinux      : %s\n", info->selinux_mode.empty() ? "?" : info->selinux_mode.c_str());
    printf("  Encryption   : %s\n", info->encryption_state.empty() ? "?" : info->encryption_state.c_str());
    printf("  Verified Boot: %s\n", info->verified_boot.empty() ? "?" : info->verified_boot.c_str());
    printf("  OEM Unlock   : %s\n", info->oem_unlock.empty() ? "?" : info->oem_unlock.c_str());
    printf("  TrustZone    : %s\n", info->tz_type.empty() ? "?" : info->tz_type.c_str());
    printf("  Knox         : %s\n", info->knox_version.empty() ? "?" : info->knox_version.c_str());

    printf("\n" BOLD "=========================================" RESET "\n");
}

void HardwareScannerDeep::fill_device_info(DeviceInfo *dinfo) const {
    if (!dinfo) return;

#define CPY(dst, src) do { \
    if (!(src).empty()) \
        strncpy((dst), (src).c_str(), sizeof(dst) - 1); \
} while(0)

    /* SoC */
    CPY(dinfo->chipset_vendor, hw_info_.soc_vendor);
    CPY(dinfo->chipset_name, hw_info_.soc_model);

    /* CPU */
    CPY(dinfo->cpu_abi, hw_info_.cpu_abi);
    CPY(dinfo->cpu_cores, hw_info_.cpu_cores);
    CPY(dinfo->cpu_maxfreq, hw_info_.cpu_max_freq);
    CPY(dinfo->cpu_minfreq, hw_info_.cpu_min_freq);
    CPY(dinfo->cpu_governor, hw_info_.cpu_governor);

    /* GPU */
    CPY(dinfo->gpu, hw_info_.gpu_model);
    if (!hw_info_.gpu_vendor.empty() && !hw_info_.gpu_model.empty()) {
        char gpu_str[1024];
        snprintf(gpu_str, sizeof(gpu_str), "%s %s",
                 hw_info_.gpu_vendor.c_str(), hw_info_.gpu_model.c_str());
        CPY(dinfo->gpu_renderer, std::string(gpu_str));
    } else if (!hw_info_.gpu_vendor.empty()) {
        CPY(dinfo->gpu_renderer, hw_info_.gpu_vendor);
    }

    /* Memory */
    if (!hw_info_.ram_total.empty())
        CPY(dinfo->ram_total, hw_info_.ram_total);
    CPY(dinfo->storage_total, hw_info_.storage_total);
    CPY(dinfo->swap_total, hw_info_.swap_total);

    /* Display */
    CPY(dinfo->screen_size, hw_info_.display_resolution);
    CPY(dinfo->screen_density, hw_info_.display_density);
    CPY(dinfo->display_hz, hw_info_.display_refresh);
    CPY(dinfo->display_hdr, hw_info_.display_hdr);
    CPY(dinfo->panel_manuf, hw_info_.display_panel);
    CPY(dinfo->brightness, hw_info_.display_brightness);

    /* Battery */
    CPY(dinfo->battery_level, hw_info_.battery_level);
    CPY(dinfo->battery_temp, hw_info_.battery_temp);
    CPY(dinfo->battery_status, hw_info_.battery_status);
    CPY(dinfo->battery_health, hw_info_.battery_health);

    /* Telephony */
    CPY(dinfo->network_type, hw_info_.network_type);
    CPY(dinfo->network_operator, hw_info_.network_operator);
    CPY(dinfo->imei1, hw_info_.imei);
    CPY(dinfo->imei2, hw_info_.imei2);
    CPY(dinfo->meid, hw_info_.meid);
    CPY(dinfo->signal_strength, hw_info_.signal_strength);
    CPY(dinfo->sim_state, hw_info_.sim_state);

    /* Connectivity */
    CPY(dinfo->mac, hw_info_.wifi_mac);
    CPY(dinfo->nfc_state, hw_info_.nfc_state);

    /* Biometrics */
    CPY(dinfo->fingerprint, hw_info_.fingerprint_sensor);
    CPY(dinfo->face_unlock, hw_info_.face_unlock);

    /* Camera */
    if (!hw_info_.cameras.empty()) {
        std::string cam_str;
        for (const auto &c : hw_info_.cameras) {
            if (!cam_str.empty()) cam_str += ", ";
            cam_str += c;
        }
        CPY(dinfo->camera_info, cam_str);
    }

    /* Security */
    CPY(dinfo->selinux_mode, hw_info_.selinux_mode);
    CPY(dinfo->crypto_state, hw_info_.encryption_state);
    CPY(dinfo->verified_boot, hw_info_.verified_boot);
    CPY(dinfo->oem_unlock, hw_info_.oem_unlock);
    CPY(dinfo->tee, hw_info_.tz_type);

#undef CPY
}
