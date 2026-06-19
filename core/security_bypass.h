#ifndef SECURITY_BYPASS_H
#define SECURITY_BYPASS_H

#include <string>
#include <vector>
#include <map>
#include <functional>

enum class BypassState {
    INIT,
    CONNECTING,
    ADB_CHECK,
    FASTBOOT_CHECK,
    RECOVERY_CHECK,
    ADB_BYPASS_ATTEMPT,
    LOCKSCREEN_BYPASS,
    RECOVERY_MOUNT,
    KEY_EXTRACTION,
    KERNEL_EXPLOIT,
    DATA_ACCESS,
    PIN_CRACKING,
    SUCCESS,
    FAILED
};

struct BypassChannel {
    std::string name;
    bool available;
    bool authorized;
    std::string auth_method;
    int priority;
    std::string status;
};

struct BypassResult {
    bool success;
    std::string method_used;
    std::string detail;
    double time_seconds;
    int total_attempts;
    bool adb_gained;
    bool root_gained;
    bool data_accessed;
    bool pin_found;
    std::string found_pin;
    bool data_intact;               /* True = no data loss */
    BypassState final_state;
};

class SecurityBypass {
public:
    SecurityBypass();
    ~SecurityBypass();

    /* Main entry point */
    BypassResult full_bypass_all();
    BypassResult full_bypass_all_with_chain();

    /* Bypass: ADB authorization */
    bool bypass_adb_auth();
    bool try_adb_keys_all();
    bool try_adb_restore_attack();
    bool try_adb_downgrade();
    bool try_adb_engineering();
    bool try_adb_recovery_adb();
    bool try_adb_fastboot_adb();

    /* Bypass: Lock screen */
    bool bypass_lock_screen();
    bool try_ls_emergency_call();
    bool try_ls_camera();
    bool try_ls_accessibility();
    bool try_ls_power_menu();
    bool try_ls_notifications();
    bool try_ls_assistant();
    bool try_ls_activity_inject();
    bool try_ls_overlay();
    bool try_ls_safe_mode();
    bool try_ls_cve_exploit();

    /* Bypass: Fastboot */
    bool bypass_fastboot_lock();
    bool try_fb_oem_unlock();
    bool try_fb_flashing_unlock();
    bool try_fb_boot_unlock_image();

    /* Bypass: Recovery */
    bool bypass_via_recovery();
    bool try_rec_mount_system();
    bool try_rec_delete_lock_file();
    bool try_rec_push_adb_keys();
    bool try_rec_extract_data();

    /* Bypass: Kernel */
    bool bypass_kernel_root();
    bool try_kernel_cve_exploit();
    bool try_kernel_dirty_pipe();
    bool try_kernel_dirty_pipe_full();
    bool try_kernel_overlayfs();
    bool try_kernel_qualcomm_exploit();
    bool try_kernel_mediatek_exploit();
    bool exec_kernel_dirty_pipe();
    bool exec_kernel_overlayfs();
    bool exec_bootloader_unlock();
    bool exec_hid_bruteforce();
    bool exec_recovery_exploit();
    bool exec_usb_gadget_exploit();
    bool exec_usb_fuzzing();
    bool exec_engineering_mode();

    /* Bypass: Hardware */
    bool bypass_hardware_security();
    bool try_hw_trustzone_exploit();
    bool try_hw_secure_element_exploit();
    bool try_hw_usb_direct();
    bool try_usb_direct_adb_rsa();

    /* FRP Bypass */
    bool try_frp_bypass();

    /* Auto-detect best exploit path */
    std::string auto_detect_exploit_path();

    /* PIN Cracking */
    bool crack_pin(int digits);
    bool try_pin_list(const std::vector<std::string> &pins);

    /* State checks */
    bool is_device_detected() const;
    bool is_adb_authorized() const;
    bool is_fastboot_mode() const;
    bool is_recovery_mode() const;
    bool is_locked() const;
    bool is_rooted() const;
    bool is_bootloader_unlocked() const;

    /* Result */
    BypassResult get_result() const { return m_result; }
    std::string generate_report() const;

    /* Device info */
    void set_device_info(const std::string &vendor, const std::string &model,
                         const std::string &serial, int sdk);

private:
    BypassState m_state;
    BypassResult m_result;
    std::vector<BypassChannel> m_channels;

    std::string m_vendor, m_model, m_serial;
    int m_sdk;

    bool adb_shell_noret(const std::string &cmd);
    std::string adb_shell(const std::string &cmd);
    bool fastboot_cmd(const std::string &cmd);

    void update_state(BypassState s);
    void add_channel(const std::string &name, int priority);
    void update_channel(const std::string &name, bool available, const std::string &status);

    std::vector<std::string> generate_adb_keys_inject();
    bool inject_key_to_recovery(const std::string &pubkey);
    bool wait_for_adb(int timeout_sec);
    bool wait_for_fastboot(int timeout_sec);

    /* Lock file removal (safe, no data loss) */
    bool safe_clear_lock();
    bool backup_lock_files();
    bool restore_lock_files();

    /* Parallel execution helpers */
    struct ParallelTask {
        std::string name;
        std::function<bool()> func;
        bool result;
        bool completed;
    };
    bool run_parallel_tasks(std::vector<ParallelTask> &tasks, int timeout_sec);
};

#endif
