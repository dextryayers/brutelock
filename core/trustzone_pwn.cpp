#include "trustzone_pwn.h"
#include "adb_native.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

TrustZonePwn::TrustZonePwn() {
    memset(&tz_info_, 0, sizeof(tz_info_));
    dm_init(&dm_);
    se_init(&se_);
}

TrustZonePwn::~TrustZonePwn() {
    dm_close(&dm_);
    se_close(&se_);
}

int TrustZonePwn::detect_trustzone(TrustZoneInfo *info) {
    memset(info, 0, sizeof(*info));
    char buf[4096];

    /* Check for QSEE (Qualcomm Secure Execution Environment) */
    if (detect_qsee(info)) goto done;
    if (detect_trusty(info)) goto done;
    if (detect_teegris(info)) goto done;
    if (detect_optee(info)) goto done;

    /* Generic detection */
    if (adb_native_shell("ls -la /dev/ | grep -iE 'tz|qsee|trust|kinibi|gud|mobicore|ote' 2>/dev/null",
                         buf, sizeof(buf)) == 0 && strlen(buf) > 0) {
        info->type = TEE_UNKNOWN;
        info->name = "Generic TEE";
        info->is_secure = 1;
        info->interfaces.push_back("Unknown TEE device");
        goto done;
    }

    info->type = TEE_UNKNOWN;
    info->name = "Not detected";
    info->is_secure = 0;
    return 0;

done:
    info->is_secure = 1;
    find_tz_shared_memory(info);
    scan_tz_regions();
    *info = tz_info_ = *info;
    return 1;
}

int TrustZonePwn::detect_qsee(TrustZoneInfo *info) {
    char buf[4096];
    if (adb_native_shell("ls /dev/qsee 2>/dev/null && echo QSEE_EXISTS || ls /dev/qseecom 2>/dev/null && echo QSEE_EXISTS",
                         buf, sizeof(buf)) == 0 && strstr(buf, "QSEE_EXISTS")) {
        info->type = TEE_QSEE;
        info->name = "Qualcomm Secure Execution Environment (QSEE)";
        info->version = "QSEE v4+";
        info->interfaces.push_back("/dev/qsee");
        info->interfaces.push_back("/dev/qseecom");
        info->interfaces.push_back("/dev/ion");
        return 1;
    }
    if (adb_native_shell("getprop | grep -i qsee 2>/dev/null | head -3",
                         buf, sizeof(buf)) == 0 && strlen(buf) > 0) {
        info->type = TEE_QSEE;
        info->name = "Qualcomm QSEE";
        return 1;
    }
    return 0;
}

int TrustZonePwn::detect_trusty(TrustZoneInfo *info) {
    char buf[4096];
    if (adb_native_shell("ls /dev/trusty* 2>/dev/null && echo TRUSTY_EXISTS",
                         buf, sizeof(buf)) == 0 && strstr(buf, "TRUSTY_EXISTS")) {
        info->type = TEE_TRUSTY;
        info->name = "Trusty TEE (Google/AOSP)";
        info->interfaces.push_back("/dev/trusty*");
        info->interfaces.push_back("/dev/trusty-ipc*");
        return 1;
    }
    if (adb_native_shell("getprop ro.hardware 2>/dev/null | grep -i trusty",
                         buf, sizeof(buf)) == 0 && strlen(buf) > 0) {
        info->type = TEE_TRUSTY;
        info->name = "Trusty TEE";
        return 1;
    }
    return 0;
}

int TrustZonePwn::detect_teegris(TrustZoneInfo *info) {
    char buf[4096];
    if (adb_native_shell("ls /dev/teegris* 2>/dev/null && echo TEEGRIS_EXISTS",
                         buf, sizeof(buf)) == 0 && strstr(buf, "TEEGRIS_EXISTS")) {
        info->type = TEE_TEEGRIS;
        info->name = "Teegris (Samsung TEE)";
        info->interfaces.push_back("/dev/teegris*");
        return 1;
    }
    return 0;
}

int TrustZonePwn::detect_optee(TrustZoneInfo *info) {
    char buf[4096];
    if (adb_native_shell("ls /dev/optee* 2>/dev/null && echo OPTEE_EXISTS",
                         buf, sizeof(buf)) == 0 && strstr(buf, "OPTEE_EXISTS")) {
        info->type = TEE_OPTEE;
        info->name = "OP-TEE (Linaro)";
        info->interfaces.push_back("/dev/optee*");
        return 1;
    }
    return 0;
}

int TrustZonePwn::find_tz_shared_memory(TrustZoneInfo *info) {
    char buf[4096];
    if (adb_native_shell("ls /dev/ion 2>/dev/null && echo ION_EXISTS",
                         buf, sizeof(buf)) && strstr(buf, "ION_EXISTS")) {
        info->shared_mem_found = 1;
        info->interfaces.push_back("/dev/ion");
        return 1;
    }
    /* Check /proc/iomem for TZ region */
    FILE *f = fopen("/proc/iomem", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            unsigned long s, e;
            char name[64];
            if (sscanf(line, "%lx-%lx : %63[^\n]", &s, &e, name) >= 3) {
                if (strstr(name, "tz") || strstr(name, "trust") ||
                    strstr(name, "qsee") || strstr(name, "sec")) {
                    info->tz_base_addr = s;
                    info->tz_size = e - s + 1;
                    info->shared_mem_found = 1;
                    fclose(f);
                    return 1;
                }
            }
        }
        fclose(f);
    }
    return 0;
}

int TrustZonePwn::scan_tz_regions() {
    if (dm_.fd_mem < 0) dm_open_mem(&dm_);
    dm_scan_iomem(&dm_);
    for (int i = 0; i < dm_.nregions; i++) {
        if (strstr(dm_.regions[i].name, "tz") ||
            strstr(dm_.regions[i].name, "trust") ||
            strstr(dm_.regions[i].name, "sec") ||
            strstr(dm_.regions[i].name, "qsee")) {
            if (!tz_info_.tz_base_addr) {
                tz_info_.tz_base_addr = dm_.regions[i].phys_addr;
                tz_info_.tz_size = dm_.regions[i].size;
            }
        }
    }
    return tz_info_.tz_base_addr > 0;
}

int TrustZonePwn::exploit_trustzone() {
    /* Try reading TZ memory via dev/mem */
    if (dm_.fd_mem < 0 && !dm_open_mem(&dm_)) return 0;
    if (tz_info_.tz_base_addr > 0) {
        unsigned int magic;
        if (dm_read32(&dm_, tz_info_.tz_base_addr, &magic)) {
            /* Check for QSEE magic */
            if ((magic & 0xFFFF) == 0x4D53) return 1; /* "SM" signature */
        }
    }
    /* Try SPMI approach for Qualcomm */
    se_scan_spmi_devices(&se_);
    if (se_.ndevices > 0) {
        unsigned char val;
        for (int i = 0; i < se_.ndevices; i++) {
            if (se_reg_read(&se_, se_.devices[i].sid, se_.devices[i].pid,
                            0x00, &val)) {
                return 1;
            }
        }
    }
    return 0;
}

int TrustZonePwn::read_secure_memory(unsigned long addr, unsigned char *buf, size_t size) {
    if (dm_.fd_mem < 0 && !dm_open_mem(&dm_)) return 0;
    return dm_read(&dm_, addr, buf, size);
}

int TrustZonePwn::write_secure_memory(unsigned long addr, const unsigned char *buf, size_t size) {
    if (dm_.fd_mem < 0 && !dm_open_mem(&dm_)) return 0;
    return dm_write(&dm_, addr, buf, size);
}

int TrustZonePwn::extract_keys(const std::string &outfile) {
    (void)outfile;
    return 0;
}

int TrustZonePwn::disable_secure_world() {
    (void)0;
    return 0;
}

int TrustZonePwn::enter_monitor_mode() {
    (void)0;
    return 0;
}

int TrustZonePwn::execute_smc_call(unsigned long function_id, unsigned long arg0,
                                    unsigned long arg1, unsigned long *result) {
    (void)function_id; (void)arg0; (void)arg1; (void)result;
    return 0;
}

void TrustZonePwn::print_trustzone_info() const {
    printf("\n" BOLD "=== TrustZone / TEE Information ===" RESET "\n");
    printf("  TEE Type: %s\n", tz_info_.name.c_str());
    printf("  Version: %s\n", tz_info_.version.c_str());
    printf("  Secure: %s\n", tz_info_.is_secure ? "Yes" : "No");
    printf("  Base: 0x%lx\n", tz_info_.tz_base_addr);
    printf("  Size: 0x%lx\n", tz_info_.tz_size);
    printf("  Shared Mem: %s\n", tz_info_.shared_mem_found ? "Found" : "N/A");
    printf("  Interfaces:\n");
    for (const auto &iface : tz_info_.interfaces)
        printf("    - %s\n", iface.c_str());
}
