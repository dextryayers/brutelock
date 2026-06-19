#ifndef HARDWARE_PROBE_H
#define HARDWARE_PROBE_H

#include <string>
#include <vector>
#include <map>

struct CpuInfo {
    std::string abi, abi2, cores, max_freq, min_freq, governor;
    std::string vendor, model_name, architecture, features;
    std::vector<int> freq_table;
    std::string bogo_mips, cpu_impl_part, hardware;
};

struct MemInfo {
    unsigned long total_kb, free_kb, available_kb, buffers_kb, cached_kb;
    unsigned long swap_total_kb, swap_free_kb;
    std::string type, speed_mhz;
};

struct StorageInfo {
    std::string total, used, free;
    std::string type, fs_type, mount_point, block_size;
    std::map<std::string,std::string> partitions;
};

struct DisplayInfo {
    int width, height, density_dpi;
    float refr_hz;
    std::string panel_name, hdr_types, color_gamut;
    int max_luminance, min_luminance;
};

struct BatteryInfo {
    int level, voltage_uv, temp_c, current_ua, capacity_uah;
    std::string status, health, technology, charger_type;
    int cycle_count;
};

struct GpuInfo {
    std::string renderer, vendor, version, glsl_version;
    std::string freq_max, freq_min;
    std::string memory_size;
};

struct NetworkInfo {
    std::string wifi_chip, bt_chip, nfc_support;
    std::string imei1, imei2, meid, phone_number;
    std::string operator_name, network_type, sim_state;
    std::string mac_wifi, mac_bt, ip_address;
    bool volte_supported, vowifi_supported;
};

struct SensorInfo {
    std::vector<std::string> sensors;
    bool has_fingerprint, has_face_unlock, has_iris;
    bool has_accelerometer, has_gyro, has_magnetometer;
    bool has_proximity, has_light, has_pressure;
    bool has_heart_rate, has_spo2;
};

struct AudioInfo {
    std::string codec, speaker, mic, dsp;
    bool has_3_5mm, has_usb_audio;
    int max_volume_levels;
};

struct CameraInfo {
    std::string rear_cam, front_cam;
    std::string flash_type;
    std::vector<int> rear_resolutions; /* MP */
};

struct KernelInfo {
    std::string version, release, type, arch;
    std::string cmdline, kasan, kpti, selinux_mode;
    bool has_kexec, has_kallsyms, is_debug_build;
    std::string kallsyms_raw;
    int kallsyms_symbol_count;
};

struct SecurityInfo {
    std::string bootloader, baseband, security_patch;
    bool verified_boot, dm_verity, force_encryption;
    bool oem_unlock_allowed, adb_secure;
    bool knox_active, frp_locked;
    std::string trustzone_os_version;
    std::string trustzone_info;
    std::string trustzone_vulns;
    std::string trustzone_tee;
    bool qsee_available;
    bool trusty_available;
    std::string widevine_level;
    int keymaster_version;
    int gatekeeper_version;
    std::string tz_version;
    bool tz_accessible;
    bool dma_dev_mem, dma_dev_kmem, dma_dev_iomem;
    bool dma_attack_surface;
    bool jtag_accessible, swd_accessible;
    bool emmc_flash_mode;
    bool kernel_debug_fs_accessible;
    std::string kallsyms_content;
    std::string selinux_mode;
    std::string selinux_policy_version;
    std::vector<std::string> selinux_contexts;
    bool selinux_enforcing;
};

struct HardwareProfile {
    bool has_adb, has_fastboot, has_recovery;
    bool has_root, has_unlocked_bl;

    std::string device_serial;
    std::string vendor, model, product, device, hardware, platform;
    std::string android_version, sdk, build_id, fingerprint;

    CpuInfo cpu;
    MemInfo mem;
    StorageInfo storage;
    DisplayInfo display;
    BatteryInfo battery;
    GpuInfo gpu;
    NetworkInfo network;
    SensorInfo sensors;
    AudioInfo audio;
    CameraInfo camera;
    KernelInfo kernel;
    SecurityInfo security;

    /* Raw data */
    std::map<std::string,std::string> all_properties;
    std::map<std::string,std::string> all_build_props;
    std::string full_dumpsys;
    std::string proc_cpuinfo, proc_meminfo, proc_version;

    std::string to_json() const;
    std::string to_pretty() const;
};

class HardwareProbe {
public:
    HardwareProbe();
    ~HardwareProbe();

    /* Main entry: probe everything through all available channels */
    bool probe_all(HardwareProfile &profile);

    /* Channel-specific probes */
    bool probe_from_adb(HardwareProfile &profile);
    bool probe_from_fastboot(HardwareProfile &profile);
    bool probe_from_recovery(HardwareProfile &profile);
    bool probe_from_usb_sysfs(HardwareProfile &profile);
    bool probe_from_kernel_proc(HardwareProfile &profile);

    /* Subsystem probes */
    bool probe_cpu(HardwareProfile &profile);
    bool probe_memory(HardwareProfile &profile);
    bool probe_storage(HardwareProfile &profile);
    bool probe_display(HardwareProfile &profile);
    bool probe_battery(HardwareProfile &profile);
    bool probe_gpu(HardwareProfile &profile);
    bool probe_network(HardwareProfile &profile);
    bool probe_sensors(HardwareProfile &profile);
    bool probe_audio(HardwareProfile &profile);
    bool probe_camera(HardwareProfile &profile);
    bool probe_kernel(HardwareProfile &profile);
    bool probe_security(HardwareProfile &profile);

    /* Advanced security probes */
    bool probe_trustzone(HardwareProfile &profile);
    bool probe_dma(HardwareProfile &profile);
    bool probe_debug_features(HardwareProfile &profile);
    bool probe_kernel_kallsyms(HardwareProfile &profile);
    bool probe_se_linux(HardwareProfile &profile);

    /* Property extraction */
    std::string getprop(const std::string &key);

    /* Utility */
    bool has_adb() const;
    bool has_fastboot() const;
    bool has_recovery() const;

    /* Read files from device */
    std::string adb_shell(const std::string &cmd);
    std::string fastboot_getvar(const std::string &var);

private:
    bool m_adb, m_fastboot, m_recovery;
    std::map<std::string,std::string> m_prop_cache;
};

#endif
