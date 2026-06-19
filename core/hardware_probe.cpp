#include "hardware_probe.h"
extern "C" {
#include "adb_native.h"
#include "adb.h"
#include "fastboot_exploit.h"
#include "recovery_exploit.h"
}
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sstream>
#include <algorithm>
#include <regex>

HardwareProbe::HardwareProbe() : m_adb(false), m_fastboot(false), m_recovery(false) {}
HardwareProbe::~HardwareProbe() {}

std::string HardwareProbe::adb_shell(const std::string &cmd) {
    char buf[65536];
    if (adb_native_shell(cmd.c_str(), buf, sizeof(buf)) == 0)
        return std::string(buf);
    return "";
}

std::string HardwareProbe::fastboot_getvar(const std::string &var) {
    FastbootExploit fe;
    fb_init(&fe);
    if (!fb_connect(&fe)) return "";
    char val[256] = {0};
    fb_getvar(&fe, var.c_str(), val, sizeof(val));
    fb_disconnect(&fe);
    return std::string(val);
}

bool HardwareProbe::has_adb() const { return m_adb; }
bool HardwareProbe::has_fastboot() const { return m_fastboot; }
bool HardwareProbe::has_recovery() const { return m_recovery; }

std::string HardwareProbe::getprop(const std::string &key) {
    if (m_prop_cache.count(key)) return m_prop_cache[key];
    char val[256] = {0};
    if (adb_native_getprop(key.c_str(), val, sizeof(val)) == 0) {
        m_prop_cache[key] = std::string(val);
        return m_prop_cache[key];
    }
    return "";
}

bool HardwareProbe::probe_all(HardwareProfile &profile) {
    profile = HardwareProfile();
    m_adb = (adb_init() && adb_state() >= 2);
    if (m_adb) {
        probe_from_adb(profile);
        probe_cpu(profile);
        probe_memory(profile);
        probe_storage(profile);
        probe_display(profile);
        probe_battery(profile);
        probe_gpu(profile);
        probe_network(profile);
        probe_sensors(profile);
        probe_audio(profile);
        probe_camera(profile);
        probe_kernel(profile);
        probe_security(profile);
        probe_trustzone(profile);
        probe_dma(profile);
        probe_debug_features(profile);
        probe_kernel_kallsyms(profile);
        probe_se_linux(profile);
    }
    probe_from_usb_sysfs(profile);
    FastbootExploit fe;
    fb_init(&fe);
    if (fb_connect(&fe)) { m_fastboot = true; probe_from_fastboot(profile); }
    // Recovery check from ADB
    char buf[64];
    if (m_adb && adb_native_shell("getprop ro.bootmode", buf, sizeof(buf)) == 0) {
        std::string mode(buf);
        m_recovery = (mode.find("recovery") != std::string::npos);
        if (m_recovery) probe_from_recovery(profile);
    }
    probe_from_kernel_proc(profile);
    return true;
}

bool HardwareProbe::probe_from_adb(HardwareProfile &profile) {
    profile.has_adb = m_adb;
    profile.vendor = getprop("ro.product.manufacturer");
    profile.model = getprop("ro.product.model");
    profile.product = getprop("ro.product.name");
    profile.device = getprop("ro.product.device");
    profile.hardware = getprop("ro.hardware");
    profile.platform = getprop("ro.board.platform");
    profile.android_version = getprop("ro.build.version.release");
    profile.sdk = getprop("ro.build.version.sdk");
    profile.build_id = getprop("ro.build.display.id");
    profile.fingerprint = getprop("ro.build.fingerprint");
    profile.device_serial = getprop("ro.serialno");
    if (profile.device_serial.empty()) profile.device_serial = getprop("sys.serialno");
    return true;
}

bool HardwareProbe::probe_from_fastboot(HardwareProfile &profile) {
    FastbootExploit fe;
    fb_init(&fe);
    if (!fb_connect(&fe)) return false;
    char val[256];
    if (fb_getvar(&fe, "product", val, sizeof(val))) profile.product = val;
    if (fb_getvar(&fe, "serialno", val, sizeof(val))) profile.device_serial = val;
    if (fb_getvar(&fe, "version-bootloader", val, sizeof(val))) profile.security.bootloader = val;
    if (fb_getvar(&fe, "version-baseband", val, sizeof(val))) profile.security.baseband = val;
    if (fb_getvar(&fe, "secure", val, sizeof(val))) profile.security.verified_boot = (strcmp(val, "yes") == 0);
    if (fb_getvar(&fe, "unlocked", val, sizeof(val))) profile.security.oem_unlock_allowed = (strcmp(val, "yes") == 0);
    profile.has_unlocked_bl = profile.security.oem_unlock_allowed;
    fb_disconnect(&fe);
    return true;
}

bool HardwareProbe::probe_from_recovery(HardwareProfile &profile) {
    RecoveryExploit re;
    rec_init(&re);
    rec_detect(&re);
    if (!re.rec.available) return false;
    if (re.adb_ready) {
        char buf[1024];
        if (rec_get_build_prop(&re, "ro.build.display.id", buf, sizeof(buf))) {
            profile.all_build_props["recovery_build"] = buf;
        }
    }
    return true;
}

bool HardwareProbe::probe_from_usb_sysfs(HardwareProfile &profile) {
    DIR *d = opendir("/sys/bus/usb/devices");
    if (!d) return false;
    struct dirent *de;
    while ((de = readdir(d))) {
        char path[256], buf[256];
        snprintf(path, sizeof(path), "/sys/bus/usb/devices/%s/manufacturer", de->d_name);
        int fd = open(path, O_RDONLY);
        if (fd >= 0) {
            int n = read(fd, buf, sizeof(buf)-1); close(fd);
            if (n > 0) { buf[n] = 0; std::string s(buf); s.erase(s.find_last_not_of(" \n\r\t")+1);
                if (!s.empty() && profile.vendor.empty()) profile.vendor = s; }
        }
        snprintf(path, sizeof(path), "/sys/bus/usb/devices/%s/product", de->d_name);
        fd = open(path, O_RDONLY);
        if (fd >= 0) {
            int n = read(fd, buf, sizeof(buf)-1); close(fd);
            if (n > 0) { buf[n] = 0; std::string s(buf); s.erase(s.find_last_not_of(" \n\r\t")+1);
                if (!s.empty() && profile.model.empty()) profile.model = s; }
        }
        snprintf(path, sizeof(path), "/sys/bus/usb/devices/%s/serial", de->d_name);
        fd = open(path, O_RDONLY);
        if (fd >= 0) {
            int n = read(fd, buf, sizeof(buf)-1); close(fd);
            if (n > 0) { buf[n] = 0; std::string s(buf); s.erase(s.find_last_not_of(" \n\r\t")+1);
                if (!s.empty() && profile.device_serial.empty()) profile.device_serial = s; }
        }
    }
    closedir(d);
    return true;
}

bool HardwareProbe::probe_from_kernel_proc(HardwareProfile &profile) {
    if (!m_adb) return false;
    profile.proc_cpuinfo = adb_shell("cat /proc/cpuinfo 2>/dev/null");
    profile.proc_meminfo = adb_shell("cat /proc/meminfo 2>/dev/null");
    profile.proc_version = adb_shell("cat /proc/version 2>/dev/null");
    return true;
}

bool HardwareProbe::probe_cpu(HardwareProfile &profile) {
    std::string info = profile.proc_cpuinfo;
    if (info.empty()) info = adb_shell("cat /proc/cpuinfo 2>/dev/null");
    profile.cpu.abi = getprop("ro.product.cpu.abi");
    profile.cpu.abi2 = getprop("ro.product.cpu.abi2");
    profile.cpu.cores = getprop("ro.core_ctl_max_cpus");
    if (profile.cpu.cores.empty()) {
        int n = 0; std::string p = info;
        size_t pos = 0; while ((pos = p.find("processor", pos)) != std::string::npos) { n++; pos++; }
        if (n > 0) profile.cpu.cores = std::to_string(n);
    }
    std::istringstream stream(info); std::string line;
    while (std::getline(stream, line)) {
        if (line.find("model name") != std::string::npos) profile.cpu.model_name = line.substr(line.find(":")+2);
        if (line.find("BogoMIPS") != std::string::npos) profile.cpu.bogo_mips = line.substr(line.find(":")+2);
        if (line.find("Features") != std::string::npos) profile.cpu.features = line.substr(line.find(":")+2);
        if (line.find("CPU implementer") != std::string::npos) profile.cpu.cpu_impl_part = line.substr(line.find(":")+2);
        if (line.find("CPU part") != std::string::npos) { profile.cpu.cpu_impl_part += " part:" + line.substr(line.find(":")+2); }
        if (line.find("Hardware") != std::string::npos) profile.cpu.hardware = line.substr(line.find(":")+2);
    }
    profile.cpu.max_freq = adb_shell("cat /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq 2>/dev/null");
    profile.cpu.min_freq = adb_shell("cat /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_min_freq 2>/dev/null");
    profile.cpu.governor = adb_shell("cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null");
    std::string freqs = adb_shell("cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies 2>/dev/null");
    if (!freqs.empty()) {
        std::istringstream fs(freqs); int f;
        while (fs >> f) profile.cpu.freq_table.push_back(f);
    }
    return true;
}

bool HardwareProbe::probe_memory(HardwareProfile &profile) {
    std::string info = profile.proc_meminfo;
    if (info.empty()) info = adb_shell("cat /proc/meminfo 2>/dev/null");
    std::istringstream stream(info); std::string line;
    while (std::getline(stream, line)) {
        unsigned long v; char name[64];
        if (sscanf(line.c_str(), "%63s %lu kB", name, &v) == 2) {
            if (strcmp(name, "MemTotal:") == 0) profile.mem.total_kb = v;
            else if (strcmp(name, "MemFree:") == 0) profile.mem.free_kb = v;
            else if (strcmp(name, "MemAvailable:") == 0) profile.mem.available_kb = v;
            else if (strcmp(name, "Buffers:") == 0) profile.mem.buffers_kb = v;
            else if (strcmp(name, "Cached:") == 0) profile.mem.cached_kb = v;
            else if (strcmp(name, "SwapTotal:") == 0) profile.mem.swap_total_kb = v;
            else if (strcmp(name, "SwapFree:") == 0) profile.mem.swap_free_kb = v;
        }
    }
    profile.mem.type = getprop("ro.vendor.ram.type");
    profile.mem.speed_mhz = getprop("ro.vendor.ram.speed");
    return true;
}

bool HardwareProbe::probe_storage(HardwareProfile &profile) {
    std::string df = adb_shell("df -h /data 2>/dev/null | tail -1");
    if (!df.empty()) {
        char total[32]={0}, used[32]={0}, free[32]={0};
        sscanf(df.c_str(), "%*s %31s %31s %31s", total, used, free);
        profile.storage.total = total; profile.storage.used = used; profile.storage.free = free;
    }
    std::string mount = adb_shell("mount | grep /data 2>/dev/null");
    if (!mount.empty()) {
        char dev[64]={0}, fstype[32]={0};
        sscanf(mount.c_str(), "%63s %*s %31s", dev, fstype);
        profile.storage.fs_type = fstype;
        profile.storage.type = getprop("ro.vendor.storage.type");
    }
    std::string parts = adb_shell("cat /proc/partitions 2>/dev/null");
    std::istringstream ps(parts); std::string line;
    while (std::getline(ps, line)) {
        int major, minor; unsigned long blocks; char name[64];
        if (sscanf(line.c_str(), "%d %d %lu %63s", &major, &minor, &blocks, name) == 4) {
            profile.storage.partitions[name] = std::to_string(blocks) + " blocks";
        }
    }
    return true;
}

bool HardwareProbe::probe_display(HardwareProfile &profile) {
    std::string size = adb_shell("wm size 2>/dev/null");
    if (!size.empty()) {
        sscanf(size.c_str(), "Physical size: %dx%d", &profile.display.width, &profile.display.height);
    }
    std::string density = adb_shell("wm density 2>/dev/null");
    if (!density.empty()) {
        sscanf(density.c_str(), "Physical density: %d", &profile.display.density_dpi);
    }
    std::string hz = getprop("ro.vendor.display.refresh_rate");
    if (!hz.empty()) profile.display.refr_hz = std::stof(hz);
    profile.display.panel_name = getprop("ro.vendor.panel.name");
    profile.display.hdr_types = getprop("ro.vendor.display.hdr");
    std::string lum = getprop("ro.vendor.display.max_luminance");
    if (!lum.empty()) profile.display.max_luminance = std::stoi(lum);
    return true;
}

bool HardwareProbe::probe_battery(HardwareProfile &profile) {
    std::string ds = adb_shell("dumpsys battery 2>/dev/null");
    if (!ds.empty()) {
        sscanf(ds.c_str(), "%*[^l]level: %d", &profile.battery.level);
        sscanf(ds.c_str(), "%*[^t]temperature: %d", &profile.battery.temp_c);
        sscanf(ds.c_str(), "%*[^v]voltage: %d", &profile.battery.voltage_uv);
        if (sscanf(ds.c_str(), "%*[^s]AC powered: %*s%*[^s]USB powered: %*s%*[^s]Wireless powered: %*s") >= 0) {}
        std::istringstream stream(ds); std::string line;
        while (std::getline(stream, line)) {
            if (line.find("AC powered") != std::string::npos) profile.battery.charger_type = "AC";
            if (line.find("USB powered") != std::string::npos) profile.battery.charger_type = "USB";
            if (line.find("Wireless powered") != std::string::npos) profile.battery.charger_type = "Wireless";
            if (line.find("technology") != std::string::npos) profile.battery.technology = line.substr(line.find(":")+2);
        }
    }
    std::string cycle = adb_shell("cat /sys/class/power_supply/battery/cycle_count 2>/dev/null || cat /sys/class/power_supply/bms/cycle_count 2>/dev/null");
    if (!cycle.empty()) profile.battery.cycle_count = std::stoi(cycle);
    profile.battery.capacity_uah = 0;
    std::string cc = getprop("ro.vendor.battery.capacity");
    if (!cc.empty()) profile.battery.capacity_uah = std::stoi(cc) * 1000;
    return true;
}

bool HardwareProbe::probe_gpu(HardwareProfile &profile) {
    profile.gpu.renderer = getprop("ro.hardware.egl");
    if (profile.gpu.renderer.empty()) profile.gpu.renderer = getprop("ro.vendor.gpu");
    profile.gpu.vendor = getprop("ro.vendor.gpu.vendor");
    profile.gpu.version = getprop("ro.vendor.gpu.version");
    profile.gpu.glsl_version = getprop("ro.vendor.gpu.glsl");
    profile.gpu.freq_max = adb_shell("cat /sys/class/kgsl/kgsl-3d0/max_gpuclk 2>/dev/null || cat /sys/devices/platform/kgsl/kgsl-3d0/max_gpuclk 2>/dev/null");
    profile.gpu.memory_size = adb_shell("cat /sys/class/kgsl/kgsl-3d0/gpubusy 2>/dev/null");
    return true;
}

bool HardwareProbe::probe_network(HardwareProfile &profile) {
    profile.network.wifi_chip = getprop("ro.vendor.wifi.version");
    profile.network.bt_chip = getprop("ro.vendor.bt.version");
    std::string nfc = getprop("ro.vendor.nfc.support");
    profile.network.nfc_support = nfc;
    profile.network.imei1 = getprop("ril.imei");
    if (profile.network.imei1.empty()) profile.network.imei1 = getprop("persist.radio.imei");
    std::string imei2 = getprop("ril.imei2");
    profile.network.imei2 = imei2.empty() ? getprop("persist.radio.imei2") : imei2;
    profile.network.phone_number = getprop("gsm.operator.alpha");
    if (profile.network.phone_number.empty()) profile.network.phone_number = getprop("persist.radio.subscriber_number");
    profile.network.operator_name = getprop("gsm.operator.alpha");
    profile.network.sim_state = getprop("gsm.sim.state");
    profile.network.mac_wifi = adb_shell("cat /sys/class/net/wlan0/address 2>/dev/null");
    profile.network.mac_bt = adb_shell("cat /sys/class/bluetooth/hci0/address 2>/dev/null");
    std::string ip = adb_shell("ip addr show wlan0 2>/dev/null | grep 'inet ' | awk '{print $2}'");
    if (!ip.empty()) profile.network.ip_address = ip.substr(0, ip.find("/"));
    profile.network.volte_supported = (getprop("ro.vendor.volte") == "true");
    return true;
}

bool HardwareProbe::probe_sensors(HardwareProfile &profile) {
    std::string list = adb_shell("dumpsys sensorservice 2>/dev/null | grep -i 'Sensor\\|Accelerometer\\|Gyro\\|Magnet' | head -20");
    std::istringstream ss(list); std::string line;
    while (std::getline(ss, line)) profile.sensors.sensors.push_back(line);
    profile.sensors.has_fingerprint = !getprop("ro.vendor.fingerprint.support").empty();
    profile.sensors.has_face_unlock = !getprop("ro.vendor.face.unlock").empty();
    std::string hw_sensors = getprop("ro.hardware.sensors");
    profile.sensors.has_accelerometer = (hw_sensors.find("accel") != std::string::npos);
    profile.sensors.has_gyro = (hw_sensors.find("gyro") != std::string::npos);
    profile.sensors.has_magnetometer = (hw_sensors.find("mag") != std::string::npos);
    return true;
}

bool HardwareProbe::probe_audio(HardwareProfile &profile) {
    profile.audio.codec = getprop("ro.vendor.audio.codec");
    profile.audio.speaker = getprop("ro.vendor.audio.speaker");
    profile.audio.dsp = getprop("ro.vendor.audio.dsp");
    std::string mic = getprop("ro.vendor.audio.mic");
    profile.audio.has_3_5mm = (adb_shell("ls /dev/headset 2>/dev/null; ls /dev/audio/headset 2>/dev/null").length() > 0);
    return true;
}

bool HardwareProbe::probe_camera(HardwareProfile &profile) {
    profile.camera.rear_cam = getprop("ro.vendor.camera.rear");
    if (profile.camera.rear_cam.empty()) {
        std::string count = getprop("ro.vendor.camera.count");
        if (!count.empty()) profile.camera.rear_cam = "Rear: " + count;
    }
    profile.camera.front_cam = getprop("ro.vendor.camera.front");
    profile.camera.flash_type = getprop("ro.vendor.camera.flash");
    std::string res = adb_shell("dumpsys media.camera 2>/dev/null | grep -i 'resolution\\|MP\\|megapixel' | head -5");
    if (!res.empty()) {
        std::istringstream rs(res); std::string line;
        while (std::getline(rs, line)) {
            int mp = 0; if (sscanf(line.c_str(), "%*[^0-9]%d", &mp) == 1) profile.camera.rear_resolutions.push_back(mp);
        }
    }
    return true;
}

bool HardwareProbe::probe_kernel(HardwareProfile &profile) {
    profile.kernel.version = getprop("ro.build.version.release");
    std::string ver = adb_shell("uname -r 2>/dev/null");
    profile.kernel.release = ver;
    std::string arch = adb_shell("uname -m 2>/dev/null");
    profile.kernel.arch = arch;
    profile.kernel.selinux_mode = adb_shell("getenforce 2>/dev/null");
    std::string cmdline = adb_shell("cat /proc/cmdline 2>/dev/null");
    profile.kernel.cmdline = cmdline;
    profile.kernel.kasan = (cmdline.find("kasan") != std::string::npos);
    profile.kernel.kpti = (cmdline.find("kpti") != std::string::npos);
    profile.kernel.is_debug_build = (getprop("ro.debuggable") == "1");
    return true;
}

bool HardwareProbe::probe_security(HardwareProfile &profile) {
    profile.security.security_patch = getprop("ro.build.version.security_patch");
    profile.security.bootloader = getprop("ro.boot.bootloader");
    profile.security.baseband = getprop("gsm.version.baseband");
    profile.security.verified_boot = (getprop("ro.boot.verifiedbootstate") == "green");
    profile.security.dm_verity = (getprop("ro.boot.veritymode") == "enforcing");
    profile.security.adb_secure = (getprop("ro.adb.secure") == "1");
    profile.security.frp_locked = (getprop("ro.boot.vbmeta.device_state") == "locked");
    profile.security.knox_active = (adb_shell("ls /system/priv-app/Knox*/ 2>/dev/null").length() > 0);
    std::string tz = adb_shell("cat /proc/tz_version 2>/dev/null || cat /sys/kernel/security/tz_version 2>/dev/null");
    if (!tz.empty()) profile.security.trustzone_os_version = tz;
    return true;
}

std::string HardwareProfile::to_json() const {
    std::ostringstream j;
    j << "{\"vendor\":\"" << vendor << "\",\"model\":\"" << model
      << "\",\"product\":\"" << product << "\",\"device\":\"" << device
      << "\",\"hardware\":\"" << hardware << "\",\"platform\":\"" << platform
      << "\",\"android\":\"" << android_version << "\",\"sdk\":\"" << sdk
      << "\",\"build_id\":\"" << build_id << "\",\"serial\":\"" << device_serial
      << "\",\"fingerprint\":\"" << fingerprint
      << "\",\"cpu_cores\":\"" << cpu.cores << "\",\"cpu_abi\":\"" << cpu.abi
      << "\",\"cpu_maxfreq\":\"" << cpu.max_freq << "\",\"cpu_minfreq\":\"" << cpu.min_freq
      << "\",\"cpu_governor\":\"" << cpu.governor << "\",\"cpu_model\":\"" << cpu.model_name
      << "\",\"mem_total_kb\":" << mem.total_kb
      << ",\"mem_available_kb\":" << mem.available_kb
      << ",\"swap_total_kb\":" << mem.swap_total_kb
      << ",\"storage_type\":\"" << storage.type
      << "\",\"display\":\"" << display.width << "x" << display.height
      << "\",\"density\":" << display.density_dpi
      << ",\"refresh_hz\":" << display.refr_hz
      << ",\"gpu\":\"" << gpu.renderer << "\",\"gpu_vendor\":\"" << gpu.vendor
      << "\",\"battery_level\":" << battery.level
      << ",\"battery_temp\":" << battery.temp_c
      << ",\"imei1\":\"" << network.imei1 << "\",\"imei2\":\"" << network.imei2
      << "\",\"phone\":\"" << network.phone_number
      << "\",\"wifi_chip\":\"" << network.wifi_chip << "\",\"bt_chip\":\"" << network.bt_chip
      << "\",\"nfc\":\"" << network.nfc_support
      << "\",\"fingerprint\":" << (sensors.has_fingerprint ? "true" : "false")
      << ",\"face_unlock\":" << (sensors.has_face_unlock ? "true" : "false")
      << ",\"audio_codec\":\"" << audio.codec
      << "\",\"camera_rear\":\"" << camera.rear_cam << "\",\"camera_front\":\"" << camera.front_cam
      << "\",\"security_patch\":\"" << security.security_patch
      << "\",\"bootloader\":\"" << security.bootloader
      << "\",\"baseband\":\"" << security.baseband
      << "\",\"verified_boot\":" << (security.verified_boot ? "true" : "false")
      << ",\"adb_secure\":" << (security.adb_secure ? "true" : "false")
      << ",\"frp_locked\":" << (security.frp_locked ? "true" : "false")
      << ",\"knox\":" << (security.knox_active ? "true" : "false")
      << ",\"kernel\":\"" << kernel.release << "\",\"selinux\":\"" << kernel.selinux_mode
      << "\"}";
    return j.str();
}

bool HardwareProbe::probe_trustzone(HardwareProfile &profile) {
    profile.security.trustzone_tee = "Unknown";
    profile.security.qsee_available = false;
    profile.security.trusty_available = false;
    profile.security.keymaster_version = 0;
    profile.security.gatekeeper_version = 0;
    profile.security.widevine_level = "Unknown";
    profile.security.tz_version = "";

    if (m_adb) {
        std::string r;
        r = adb_shell("ls /dev/trustzone 2>/dev/null && echo TZOK || echo TZNO");
        if (r.find("TZOK") != std::string::npos) profile.security.trustzone_tee = "TrustZone (generic)";
        r = adb_shell("ls /dev/qsee 2>/dev/null || ls /dev/qseecom 2>/dev/null && echo QSEE || echo QNO");
        if (r.find("QSEE") != std::string::npos) { profile.security.trustzone_tee = "QSEE (Qualcomm)"; profile.security.qsee_available = true; }
        r = adb_shell("ls /dev/trusty* 2>/dev/null && echo TRUSTY || echo TNO");
        if (r.find("TRUSTY") != std::string::npos) { profile.security.trusty_available = true; profile.security.trustzone_tee = "Trusty TEE"; }
        r = adb_shell("getprop ro.boot.trustzone 2>/dev/null");
        if (!r.empty()) { r.erase(r.find_last_not_of(" \n\r\t")+1); profile.security.tz_version = r; }
        r = adb_shell("getprop ro.boot.keymaster 2>/dev/null");
        if (!r.empty()) { r.erase(r.find_last_not_of(" \n\r\t")+1); if (!r.empty()) profile.security.keymaster_version = atoi(r.c_str()); }
        r = adb_shell("getprop ro.boot.gatekeeper 2>/dev/null");
        if (!r.empty()) { r.erase(r.find_last_not_of(" \n\r\t")+1); if (!r.empty()) profile.security.gatekeeper_version = atoi(r.c_str()); }
        r = adb_shell("dumpsys drm 2>/dev/null | grep 'Level' | head -1");
        if (!r.empty()) { r.erase(r.find_last_not_of(" \n\r\t")+1); profile.security.widevine_level = r; }
    }

    if (m_fastboot) {
        std::string r;
        r = fastboot_getvar("trustzone");
        if (!r.empty()) profile.security.tz_version = r;
        r = fastboot_getvar("keymaster");
        if (!r.empty()) profile.security.keymaster_version = atoi(r.c_str());
    }

    return !profile.security.trustzone_tee.empty();
}

bool HardwareProbe::probe_dma(HardwareProfile &profile) {
    if (!m_adb) return false;
    std::string dev_mem = adb_shell("ls -la /dev/mem 2>/dev/null");
    profile.security.dma_dev_mem = !dev_mem.empty() && (dev_mem.find("crw") != std::string::npos);
    std::string dev_kmem = adb_shell("ls -la /dev/kmem 2>/dev/null");
    profile.security.dma_dev_kmem = !dev_kmem.empty() && (dev_kmem.find("crw") != std::string::npos);
    std::string dev_iomem = adb_shell("ls -la /dev/iomem 2>/dev/null");
    profile.security.dma_dev_iomem = !dev_iomem.empty();
    std::string rw_dma = adb_shell("cat /dev/mem 2>/dev/null | head -c 1; echo 2>/dev/null");
    if (!rw_dma.empty()) profile.security.dma_attack_surface = true;
    rw_dma = adb_shell("cat /dev/kmem 2>/dev/null | head -c 1; echo 2>/dev/null");
    if (!rw_dma.empty()) profile.security.dma_attack_surface = true;
    std::string dma_zones = adb_shell("ls /sys/kernel/debug/dma* 2>/dev/null");
    if (!dma_zones.empty()) profile.security.dma_attack_surface = true;
    return profile.security.dma_dev_mem || profile.security.dma_dev_kmem || profile.security.dma_dev_iomem;
}

bool HardwareProbe::probe_debug_features(HardwareProfile &profile) {
    if (!m_adb) return false;
    std::string cr20 = adb_shell("cat /proc/cpuinfo 2>/dev/null | grep -i 'CR-20\\|debug\\|jtag'");
    if (!cr20.empty()) profile.security.jtag_accessible = true;
    std::string debug_fs = adb_shell("ls /sys/kernel/debug/ 2>/dev/null | head -20");
    profile.security.kernel_debug_fs_accessible = !debug_fs.empty();
    if (!debug_fs.empty()) {
        std::string swd = adb_shell("ls /sys/kernel/debug/swd* /sys/kernel/debug/jtag* 2>/dev/null");
        if (!swd.empty()) profile.security.swd_accessible = true;
    }
    std::string emmc = adb_shell("ls -la /dev/block/mmcblk0boot* /dev/block/bootdevice/by-name/ 2>/dev/null | head -10");
    if (!emmc.empty()) profile.security.emmc_flash_mode = true;
    std::string jtag_dev = adb_shell("ls /dev/jtag* 2>/dev/null");
    if (!jtag_dev.empty()) profile.security.jtag_accessible = true;
    return profile.security.jtag_accessible || profile.security.kernel_debug_fs_accessible;
}

bool HardwareProbe::probe_kernel_kallsyms(HardwareProfile &profile) {
    if (!m_adb) return false;
    std::string kallsyms = adb_shell("cat /proc/kallsyms 2>/dev/null");
    if (!kallsyms.empty()) {
        profile.kernel.kallsyms_raw = kallsyms;
        profile.kernel.has_kallsyms = true;
        int count = 0;
        std::istringstream ks(kallsyms); std::string line;
        while (std::getline(ks, line)) count++;
        profile.kernel.kallsyms_symbol_count = count;
        profile.security.kallsyms_content = kallsyms.substr(0, 4096);
        return true;
    }
    profile.kernel.has_kallsyms = false;
    return false;
}

bool HardwareProbe::probe_se_linux(HardwareProfile &profile) {
    if (!m_adb) return false;
    std::string mode = adb_shell("getenforce 2>/dev/null");
    if (!mode.empty()) {
        profile.kernel.selinux_mode = mode;
        profile.security.selinux_mode = mode;
        profile.security.selinux_enforcing = (mode.find("Enforcing") != std::string::npos ||
                                               mode.find("enforcing") != std::string::npos);
    }
    std::string policy_ver = adb_shell("cat /selinux/policy_version 2>/dev/null || cat /sys/fs/selinux/policyversion 2>/dev/null");
    if (!policy_ver.empty()) profile.security.selinux_policy_version = policy_ver;
    std::string contexts = adb_shell("ls -Z /system/bin/ /system/app/ 2>/dev/null | head -30");
    if (!contexts.empty()) {
        std::istringstream cs(contexts); std::string line;
        while (std::getline(cs, line)) {
            if (line.find(':') != std::string::npos && line.find(':') != line.rfind(':'))
                profile.security.selinux_contexts.push_back(line);
        }
    }
    std::string sepolicy = adb_shell("ls -la /sepolicy 2>/dev/null");
    if (!sepolicy.empty()) profile.security.selinux_policy_version += " sepolicy:present";
    return !mode.empty();
}

std::string HardwareProfile::to_pretty() const {
    std::ostringstream p;
    p << "=== HARDWARE PROFILE ===\n";
    p << "Vendor      : " << vendor << "\nModel       : " << model << "\n";
    p << "Product     : " << product << "\nDevice      : " << device << "\n";
    p << "Hardware    : " << hardware << "\nPlatform    : " << platform << "\n";
    p << "Android     : " << android_version << " (SDK " << sdk << ")\n";
    p << "Build ID    : " << build_id << "\nSerial      : " << device_serial << "\n";
    p << "\n-- CPU --\n  " << cpu.cores << " cores, " << cpu.model_name;
    p << "\n  ABI: " << cpu.abi << " | Max: " << cpu.max_freq << " kHz | Gov: " << cpu.governor;
    p << "\n-- Memory --\n  Total: " << (mem.total_kb/1024) << " MB | Avail: " << (mem.available_kb/1024) << " MB";
    p << "\n  Swap: " << (mem.swap_total_kb/1024) << " MB";
    p << "\n-- Storage --\n  " << storage.fs_type << " | Total: " << storage.total << " | Used: " << storage.used;
    p << "\n-- Display --\n  " << display.width << "x" << display.height << " @" << display.refr_hz << "Hz";
    p << "\n  Density: " << display.density_dpi << " dpi | Panel: " << display.panel_name;
    p << "\n-- GPU --\n  " << gpu.renderer << " (" << gpu.vendor << ")";
    p << "\n-- Battery --\n  Level: " << battery.level << "% | Temp: " << (battery.temp_c/10) << "." << (battery.temp_c%10) << "C";
    p << "\n  Charger: " << battery.charger_type << " | Cycles: " << battery.cycle_count;
    p << "\n-- Network --\n  IMEI1: " << network.imei1 << "\n  IMEI2: " << network.imei2;
    p << "\n  Phone: " << network.phone_number;
    p << "\n  Wi-Fi: " << network.wifi_chip << " | BT: " << network.bt_chip << " | NFC: " << network.nfc_support;
    p << "\n-- Sensors --\n  FP: " << (sensors.has_fingerprint ? "Yes" : "No");
    p << "\n  Face: " << (sensors.has_face_unlock ? "Yes" : "No");
    p << "\n  Accel: " << (sensors.has_accelerometer ? "Yes" : "No");
    p << "\n  Gyro: " << (sensors.has_gyro ? "Yes" : "No");
    p << "\n-- Audio --\n  Codec: " << audio.codec << " | Speaker: " << audio.speaker;
    p << "\n-- Camera --\n  Rear: " << camera.rear_cam << " | Front: " << camera.front_cam;
    p << "\n-- Kernel --\n  " << kernel.release << " | Selinux: " << kernel.selinux_mode;
    p << "\n-- Security --\n  Patch: " << security.security_patch << " | BL: " << security.bootloader;
    p << "\n  Verified: " << (security.verified_boot ? "Yes" : "No");
    p << "\n  ADB Secure: " << (security.adb_secure ? "Yes" : "No");
    p << "\n  FRP: " << (security.frp_locked ? "Locked" : "Unlocked");
    p << "\n  Knox: " << (security.knox_active ? "Active" : "Inactive");
    p << "\n-- TrustZone --\n";
    p << "  TEE: " << security.trustzone_tee << "\n";
    p << "  QSEE: " << (security.qsee_available ? "Yes" : "No") << "\n";
    p << "  Trusty: " << (security.trusty_available ? "Yes" : "No") << "\n";
    p << "  Widevine: " << security.widevine_level << "\n";
    p << "  Keymaster: v" << security.keymaster_version << "\n";
    p << "  Gatekeeper: " << (security.gatekeeper_version > 0 ? "v" + std::to_string(security.gatekeeper_version) : "None") << "\n";
    p << "  TZ Version: " << security.tz_version << "\n";
    p << "\n";
    return p.str();
}

