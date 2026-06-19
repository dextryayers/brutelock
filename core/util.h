#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RED     "\033[0;31m"
#define GREEN   "\033[0;32m"
#define YELLOW  "\033[1;33m"
#define CYAN    "\033[0;36m"
#define BOLD    "\033[1m"
#define RESET   "\033[0m"

#define MAX_STR 512
#define BIG_STR 2048
#define STATE_FILE "brutepin_state.txt"

typedef struct {
    char vendor[MAX_STR];
    char model[MAX_STR];
    char chipset_vendor[MAX_STR];
    char chipset_name[MAX_STR];
    char platform[MAX_STR];
    char android[MAX_STR];
    char sdk[MAX_STR];
    char security_patch[MAX_STR];
    char serial[MAX_STR];
    char imei[MAX_STR];
    char imei2[MAX_STR];
    char cpu_abi[MAX_STR];
    char cpu_cores[MAX_STR];
    char cpu_maxfreq[MAX_STR];
    char ram_total[MAX_STR];
    char storage_total[MAX_STR];
    char screen_size[MAX_STR];
    char screen_density[MAX_STR];
    char battery_level[MAX_STR];
    char battery_status[MAX_STR];
    char kernel[MAX_STR];
    char ip[MAX_STR];
    char mac[MAX_STR];
    char wifi_ssid[MAX_STR];
    char lock_type[MAX_STR];
    char build_fingerprint[MAX_STR];
    char bootloader[MAX_STR];
    char baseband[MAX_STR];
    char display_id[MAX_STR];
    char gpu[MAX_STR];
    char dpr[MAX_STR];
    char product_name[MAX_STR];
    char device_name[MAX_STR];
    char hardware[MAX_STR];
    int  locked;
    int  pin_len;
    char detected_via[32];  /* "ADB", "Fastboot", "USB", "" */

    char ip_list[BIG_STR];
    char mac_list[BIG_STR];
    char wifi_bssid[64];
    char wifi_freq[16];
    char wifi_rssi[16];
    char gateway[64];
    char dns_list[256];

    char kernel_full[MAX_STR];
    char selinux_mode[16];
    char partitions[BIG_STR];
    char mounts[BIG_STR];
    char cpu_governor[32];
    char cpu_minfreq[32];
    char swap_total[32];
    char mem_details[BIG_STR];
    char kernel_modules[BIG_STR];

    char pin_hash[256];
    char pin_salt[64];
    char pin_type[32];
    char backup_pin[16];
    char locksettings_info[BIG_STR];

    char package_count[16];
    char device_admin_apps[BIG_STR];
    char accessibility_services[BIG_STR];
    char usb_mode[64];
    char power_info[MAX_STR];
    char running_services[BIG_STR];
    char installed_apps_count[16];

    char gpu_renderer[128];
    char gpu_version[64];
    char display_hz[16];
    char display_hdr[16];
    char fingerprint[32];
    char face_unlock[32];
    char camera_info[MAX_STR];
    char sensors[BIG_STR];
    char audio_codecs[MAX_STR];
    char biometrics[256];

    char google_accounts[BIG_STR];
    char frp_status[64];
    char device_owner[256];
    char backup_account[256];

    char imei1[32];
    char imei2_2[32];
    char imsi[32];
    char phone_number[32];
    char network_operator[128];
    char sim_operator[128];
    char network_type[32];
    char signal_strength[16];
    char sim_state[32];
    char volte[16];
    char data_roaming[16];
    char meid[32];
    char cell_id[32];
    char lac[16];

    char boot_mode[32];
    char build_type[32];
    char dev_options[16];
    char oem_unlock[16];
    char crypto_state[64];
    char avb_state[32];
    char usb_config[64];
    char adb_secure[24];
    char selinux_context[128];
    char locale[64];
    char cpu_abilist[256];
    char animation_scale[128];
    char brightness[32];

    char touchscreen[128];
    char input_devices[BIG_STR];
    char nfc_state[32];
    char bt_info[128];
    char sensors_detail[BIG_STR];
    char fm_radio[32];
    char ir_blaster[16];
    char stylus[32];
    char battery_health[32];
    char battery_temp[16];
    char battery_voltage[16];
    char battery_tech[32];

    char verity_mode[32];
    char verified_boot[32];
    char bootloader_unlock[32];
    char last_kmsg[BIG_STR];
    char recent_crashes[BIG_STR];
    char thermal_info[BIG_STR];
    char battery_stats[BIG_STR];
    char proc_stats[BIG_STR];
    char bypass_list[BIG_STR];

    char soc_exact[128];
    char cpu_clusters[256];
    char gpu_freq[32];
    char npu[64];
    char modem_type[64];

    char panel_manuf[64];
    char panel_type[32];
    char hdr_formats[128];
    char color_gamut[64];
    char touch_sampling[24];
    char aod[16];
    char display_feat[256];

    char rear_cams[256];
    char front_cam[128];
    char video_cap[64];
    char flash[32];

    char audio_dsp[64];
    char speaker_cfg[32];
    char mic_num[16];
    char audio_codec[64];
    char audio_feat[128];

    char batt_cap[32];
    char charge_type[64];
    char wireless_charge[32];
    char charge_current[32];

    char wifi_ver[24];
    char bt_ver[16];
    char gps_type[64];
    char nfc_feat[64];
    char usb_ver[16];

    char ram_type[32];
    char storage_type[32];
    char sdcard[24];

    char tee[64];
    char keymaster_ver[16];
    char gatekeeper_ver[16];
    char avb_mode[16];
    char dmverity_status[32];

    char sensor_list[BIG_STR];
} DeviceInfo;

typedef enum {
    CHIP_UNKNOWN = 0,
    CHIP_MEDIATEK,
    CHIP_QUALCOMM,
    CHIP_EXYNOS,
    CHIP_SPREADTRUM,
    CHIP_TENSOR,
    CHIP_KIRIN,
    CHIP_BROADCOM,
    CHIP_ROCKCHIP,
    CHIP_ALLWINNER,
    CHIP_INTEL,
    CHIP_NVIDIA
} ChipsetFamily;

void banner(void);
void save_state(long cur, long tot);
long load_state(long *tot);
void print_progress(long cur, long total, const char *pin, double elapsed);
char* trim_newline(char *s);
void normalize_str(char *dst, const char *src);
int yes_no(const char *prompt);
void loading_spinner(const char *msg, int duration_s);
void loading_dots(const char *msg, int duration_s);
void append_str(char *dst, size_t size, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
