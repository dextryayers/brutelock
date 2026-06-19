#ifndef DEVICE_FINGERPRINT_H
#define DEVICE_FINGERPRINT_H

#include <string>

struct FingerprintData {
    std::string serial;
    std::string imei1, imei2;
    std::string phone;
    std::string model;
    std::string brand;
    std::string android;
    std::string sdk;
    std::string build_fp;
    std::string security_patch;
    std::string kernel;
    std::string bootloader;
    std::string hardware;
    std::string fingerprint;
    std::string face_unlock;
    std::string lock_type;
    std::string screen_size;
    std::string density;
    std::string battery;
    std::string chipset;
    std::string cpu_cores;
    std::string ram;

    /* Derived */
    std::string device_id_hash;
    int probable_pin_length;
    int has_biometric;
    int has_face;
    int has_fp;
};

extern "C" {
    FingerprintData fp_analyze_device(void);
    void fp_print_info(const FingerprintData *fp);
    int fp_predict_pin_length(const FingerprintData *fp);
    int fp_generate_pins(const FingerprintData *fp, char pins[][16], int max);
}

#endif
