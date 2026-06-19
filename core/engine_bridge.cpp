#include "engine_bridge.h"
#include "hardware_probe.h"
#include "data_forensics.h"
#include "exploit_chain.h"
#include "neural_cracker.h"
#include "security_bypass.h"
#include "auto_pwn_chain.h"
#include "hardware_scanner_deep.h"
#include <cstdio>
#include <cstring>

int hwprobe_run(void) {
    HardwareProbe probe;
    HardwareProfile profile;
    if (probe.probe_all(profile)) {
        printf("%s", profile.to_pretty().c_str());
        printf("\n--- JSON ---\n%s\n", profile.to_json().c_str());
        return 0;
    }
    printf("[-] Hardware probe failed.\n");
    return 1;
}

int forensics_extract(void) {
    DataForensics df;
    DeviceData data = df.extract_all();
    printf("%s", data.to_json().c_str());

    /* Save gatekeeper hashes if available */
    if (!data.gatekeeper_password_key.empty() || !data.gatekeeper_pattern_key.empty()) {
        printf("\n[*] Gatekeeper keys found! Saving to hash.txt\n");
        FILE *f = fopen("hash.txt", "w");
        if (f) {
            if (!data.gatekeeper_password_key.empty()) {
                fprintf(f, "password:%s\n", data.gatekeeper_password_key.c_str());
                printf("  password.key: %s\n", data.gatekeeper_password_key.c_str());
            }
            if (!data.gatekeeper_pattern_key.empty()) {
                fprintf(f, "pattern:%s\n", data.gatekeeper_pattern_key.c_str());
                printf("  pattern.key: %s\n", data.gatekeeper_pattern_key.c_str());
            }
            fclose(f);
            printf("[+] Saved to hash.txt\n");
        }
    }
    return 0;
}

int chain_run(void) {
    ExploitChain chain;
    chain.set_device_info("auto", "auto", 30, true, true);
    chain.auto_build_chain();
    ExploitResult result = chain.execute();
    printf("%s", chain.generate_report().c_str());
    return result.overall_success ? 0 : 1;
}

int neural_gen_pins(int digits, int count, char *out, int out_sz) {
    NeuralCracker nc;
    nc.set_digits(digits);
    nc.train_from_common_pins();
    nc.train_from_patterns();

    std::vector<PinScore> candidates = nc.generate_candidates(count, digits);
    int pos = 0;
    for (size_t i = 0; i < candidates.size() && pos < out_sz - 1; i++) {
        pos += snprintf(out + pos, out_sz - pos, "%s (%.2f) [%s]\n",
                        candidates[i].pin.c_str(), candidates[i].score,
                        candidates[i].source_name.c_str());
    }
    return candidates.size();
}

int secbypass_run(void) {
    SecurityBypass sb;
    BypassResult result = sb.full_bypass_all();
    printf("%s", sb.generate_report().c_str());
    return result.success ? 0 : 1;
}

int auto_pwn_run(void) {
    AutoPwnChain chain;
    if (!chain.is_device_ready()) {
        printf("[-] Device not ready.\n");
        return 1;
    }
    return chain.run_full_chain() ? 0 : 1;
}

int auto_pwn_scan_full(char *out, int out_sz) {
    HardwareScannerDeep scanner;
    DeepHWInfo hwinfo;
    DeviceInfo devinfo;
    memset(&devinfo, 0, sizeof(devinfo));

    scanner.scan_all(&hwinfo);
    scanner.fill_device_info(&devinfo);
    scanner.print_deep_info(&hwinfo);

    if (out && out_sz > 0) {
        int pos = 0;
        pos += snprintf(out + pos, out_sz - pos,
            "SoC: %s %s\nCPU: %s %s cores %s\nGPU: %s %s\n"
            "RAM: %s %s\nStorage: %s %s\n"
            "Display: %s %s %sHz %s\n"
            "Battery: %s %s %s\n"
            "WiFi: %s BT: %s NFC: %s\n"
            "Fingerprint: %s Face: %s\n"
            "SELinux: %s Encryption: %s\n"
            "TrustZone: %s\n",
            hwinfo.soc_vendor.c_str(), hwinfo.soc_model.c_str(),
            hwinfo.cpu_arch.c_str(), hwinfo.cpu_cores.c_str(), hwinfo.cpu_max_freq.c_str(),
            hwinfo.gpu_vendor.c_str(), hwinfo.gpu_model.c_str(),
            hwinfo.ram_total.c_str(), hwinfo.ram_type.c_str(),
            hwinfo.storage_total.c_str(), hwinfo.storage_type.c_str(),
            hwinfo.display_resolution.c_str(), hwinfo.display_density.c_str(),
            hwinfo.display_refresh.c_str(), hwinfo.display_hdr.c_str(),
            hwinfo.battery_level.c_str(), hwinfo.battery_status.c_str(), hwinfo.battery_health.c_str(),
            hwinfo.wifi_mac.c_str(), hwinfo.bt_mac.c_str(), hwinfo.nfc_state.c_str(),
            hwinfo.fingerprint_sensor.c_str(), hwinfo.face_unlock.c_str(),
            hwinfo.selinux_mode.c_str(), hwinfo.encryption_state.c_str(),
            hwinfo.tz_type.c_str());
    }
    return 0;
}
