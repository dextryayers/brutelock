#ifndef TRUSTZONE_PWN_H
#define TRUSTZONE_PWN_H

#include <string>
#include <vector>
#include "devmem_exploit.h"
#include "spmi_exploit.h"

typedef enum {
    TEE_UNKNOWN,
    TEE_QSEE,
    TEE_TRUSTY,
    TEE_TEEGRIS,
    TEE_KINIBI,
    TEE_OPTEE,
    TEE_TRUSTEDCORE,
    TEE_MAX
} TeeType;

typedef struct {
    TeeType type;
    std::string name;
    std::string version;
    int is_secure;
    std::vector<std::string> interfaces;
    int shared_mem_found;
    unsigned long tz_base_addr;
    unsigned long tz_size;
} TrustZoneInfo;

class TrustZonePwn {
public:
    TrustZonePwn();
    ~TrustZonePwn();

    int detect_trustzone(TrustZoneInfo *info);
    int exploit_trustzone();
    int read_secure_memory(unsigned long addr, unsigned char *buf, size_t size);
    int write_secure_memory(unsigned long addr, const unsigned char *buf, size_t size);
    int extract_keys(const std::string &outfile);
    int disable_secure_world();
    int enter_monitor_mode();
    int execute_smc_call(unsigned long function_id, unsigned long arg0,
                         unsigned long arg1, unsigned long *result);

    int is_trustzone_available() const { return tz_info_.is_secure; }
    TeeType get_tee_type() const { return tz_info_.type; }
    std::string get_last_error() const { return last_error_; }
    DevMemExploit *get_devmem() { return &dm_; }

    void print_trustzone_info() const;

private:
    TrustZoneInfo tz_info_;
    DevMemExploit dm_;
    SPMIExploit se_;
    std::string last_error_;

    int detect_qsee(TrustZoneInfo *info);
    int detect_trusty(TrustZoneInfo *info);
    int detect_teegris(TrustZoneInfo *info);
    int detect_optee(TrustZoneInfo *info);
    int find_tz_shared_memory(TrustZoneInfo *info);
    int scan_tz_regions();
};

#endif
