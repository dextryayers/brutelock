#include "allprop.h"
#include "adb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void try_fill(char *dst, size_t sz, const char *val) {
    if (strlen(dst) == 0 && strlen(val) > 0) {
        strncpy(dst, val, sz - 1);
        dst[sz - 1] = 0;
    }
}

void detect_allprop(DeviceInfo *info) {
    char *tmp, buf[8192];
    int count = 0;

    buf[0] = 0;
    const char *gpu_keys[] = {
        "ro.hardware.gpu", "ro.vendor.gpu", "ro.product.gpu",
        "ro.gpu.model", "ro.vendor.hardware.gpu", "ro.board.gpu",
        "debug.gpu.model", NULL
    };
    for (int i = 0; gpu_keys[i]; i++) {
        snprintf(buf, sizeof(buf), "adb shell getprop %s 2>/dev/null", gpu_keys[i]);
        tmp = adb_shell(buf);
        trim_newline(tmp);
        if (strlen(tmp) > 0) { try_fill(info->gpu, sizeof(info->gpu), tmp); count++; }
    }
    if (count > 0 && strlen(info->gpu_renderer) == 0 && strlen(info->gpu) > 0) {
        snprintf(info->gpu_renderer, sizeof(info->gpu_renderer)-1, "%s (prop)", info->gpu);
    }

    tmp = adb_shell("adb shell dumpsys meminfo 2>/dev/null | grep -iE 'Total RAM|RAM:' | head -1");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->ram_total) == 0) {
        char *p = strstr(tmp, ":");
        if (p) { p++; while (*p == ' ') p++; strncpy(info->ram_total, p, sizeof(info->ram_total)-1); }
    }

    count = 0;
    const char *imei_keys[] = {
        "ril.imei", "persist.radio.imei", "gsm.baseband.imei",
        "net.rild.imei", "ril.IMEI", "ro.ril.imei", "gsm.imei",
        "ro.telephony.imei", NULL
    };
    for (int i = 0; imei_keys[i]; i++) {
        snprintf(buf, sizeof(buf), "adb shell getprop %s 2>/dev/null", imei_keys[i]);
        tmp = adb_shell(buf);
        trim_newline(tmp);
        if (strlen(tmp) > 0 && strlen(tmp) > 10) {
            try_fill(info->imei1, sizeof(info->imei1), tmp);
            count++;
        }
    }
    if (strlen(info->imei) == 0 && strlen(info->imei1) > 0)
        strncpy(info->imei, info->imei1, sizeof(info->imei)-1);

    tmp = adb_shell("adb shell getprop gsm.baseband 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->baseband) == 0)
        strncpy(info->baseband, tmp, sizeof(info->baseband)-1);

    const char *baseband_keys[] = {
        "gsm.version.baseband", "ro.vendor.baseband", "ro.modem.name",
        "persist.radio.baseband", NULL
    };
    for (int i = 0; baseband_keys[i]; i++) {
        snprintf(buf, sizeof(buf), "adb shell getprop %s 2>/dev/null", baseband_keys[i]);
        tmp = adb_shell(buf);
        trim_newline(tmp);
        if (strlen(tmp) > 0) { try_fill(info->baseband, sizeof(info->baseband), tmp); break; }
    }

    tmp = adb_shell("adb shell getprop ro.display.hdr 2>/dev/null");
    trim_newline(tmp);
    if (strcmp(tmp, "1") == 0 && strlen(info->display_hdr) == 0)
        strncpy(info->display_hdr, "HDR Supported (prop)", sizeof(info->display_hdr)-1);

    tmp = adb_shell("adb shell getprop ro.vendor.display.hdr 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->hdr_formats) == 0)
        strncpy(info->hdr_formats, tmp, sizeof(info->hdr_formats)-1);

    tmp = adb_shell("adb shell getprop ro.vendor.display.refresh_rate 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->display_hz) == 0)
        strncpy(info->display_hz, tmp, sizeof(info->display_hz)-1);

    tmp = adb_shell("adb shell getprop ro.vendor.display.max_luminance 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        char lum[64];
        snprintf(lum, sizeof(lum), "%s nits", tmp);
        try_fill(info->display_feat, sizeof(info->display_feat), lum);
    }

    tmp = adb_shell("adb shell getprop ro.vendor.panel.name 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) try_fill(info->panel_manuf, sizeof(info->panel_manuf), tmp);

    tmp = adb_shell("adb shell getprop ro.vendor.camera.count 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->camera_info) == 0)
        snprintf(info->camera_info, sizeof(info->camera_info), "%s cam(s) (prop)", tmp);

    const char *cam_keys[] = {
        "ro.vendor.camera.rear", "ro.vendor.camera.front",
        "ro.camera.rear", "ro.camera.front",
        "persist.vendor.camera.rear", NULL
    };
    for (int i = 0; cam_keys[i]; i++) {
        snprintf(buf, sizeof(buf), "adb shell getprop %s 2>/dev/null", cam_keys[i]);
        tmp = adb_shell(buf);
        trim_newline(tmp);
        if (strlen(tmp) > 0) {
            try_fill(info->rear_cams, sizeof(info->rear_cams), tmp);
            count++;
        }
    }

    tmp = adb_shell("adb shell getprop persist.vendor.camera.front 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) try_fill(info->front_cam, sizeof(info->front_cam), tmp);

    tmp = adb_shell("adb shell getprop ro.vendor.audio.codec 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) try_fill(info->audio_codec, sizeof(info->audio_codec), tmp);

    tmp = adb_shell("adb shell getprop ro.vendor.audio.speaker 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) try_fill(info->speaker_cfg, sizeof(info->speaker_cfg), tmp);

    tmp = adb_shell("adb shell getprop ro.vendor.wifi.version 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->wifi_ver) == 0)
        snprintf(info->wifi_ver, sizeof(info->wifi_ver), "v%s", tmp);

    tmp = adb_shell("adb shell getprop ro.vendor.bt.version 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->bt_ver) == 0)
        snprintf(info->bt_ver, sizeof(info->bt_ver), "BT %s", tmp);

    tmp = adb_shell("adb shell getprop ro.vendor.nfc.support 2>/dev/null");
    trim_newline(tmp);
    if (strcmp(tmp, "1") == 0 && strlen(info->nfc_state) == 0)
        strncpy(info->nfc_state, "Supported (prop)", sizeof(info->nfc_state)-1);

    tmp = adb_shell("adb shell getprop ro.vendor.fingerprint.support 2>/dev/null");
    trim_newline(tmp);
    if (strcmp(tmp, "1") == 0 && strlen(info->fingerprint) == 0)
        strncpy(info->fingerprint, "Present (prop)", sizeof(info->fingerprint)-1);

    tmp = adb_shell("adb shell getprop ro.vendor.face.unlock 2>/dev/null");
    trim_newline(tmp);
    if (strcmp(tmp, "1") == 0 && strlen(info->face_unlock) == 0)
        strncpy(info->face_unlock, "Present (prop)", sizeof(info->face_unlock)-1);

    tmp = adb_shell("adb shell getprop ro.vendor.battery.capacity 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->batt_cap) == 0)
        snprintf(info->batt_cap, sizeof(info->batt_cap), "%s mAh (prop)", tmp);

    tmp = adb_shell("adb shell getprop ro.vendor.charge.type 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->charge_type) == 0)
        strncpy(info->charge_type, tmp, sizeof(info->charge_type)-1);

    tmp = adb_shell("adb shell getprop ro.vendor.usb.type 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->usb_ver) == 0)
        snprintf(info->usb_ver, sizeof(info->usb_ver), "USB %s", tmp);

    tmp = adb_shell("adb shell getprop ro.vendor.ram.type 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->ram_type) == 0)
        snprintf(info->ram_type, sizeof(info->ram_type), "LPDDR%s", tmp);

    tmp = adb_shell("adb shell getprop ro.vendor.storage.type 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->storage_type) == 0)
        snprintf(info->storage_type, sizeof(info->storage_type), "UFS %s", tmp);

    tmp = adb_shell("adb shell getprop ro.vendor.oem.unlock 2>/dev/null");
    trim_newline(tmp);
    if (strcmp(tmp, "1") == 0) strncpy(info->oem_unlock, "Supported", sizeof(info->oem_unlock)-1);

    tmp = adb_shell("adb shell getprop ro.vendor.selinux 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->selinux_context) == 0)
        strncpy(info->selinux_context, tmp, sizeof(info->selinux_context)-1);

    tmp = adb_shell("adb shell getprop ro.vendor.boot.reason 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        char br[128];
        snprintf(br, sizeof(br), "* Boot reason: %s", tmp);
        if (strlen(info->bypass_list) == 0 || strcmp(info->bypass_list, "None identified")) {
            append_str(info->bypass_list, sizeof(info->bypass_list), "%s ", br);
        } else strncpy(info->bypass_list, br, sizeof(info->bypass_list)-1);
    }

    tmp = adb_shell("adb shell getprop ro.vendor.build.security_patch 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->security_patch) == 0)
        strncpy(info->security_patch, tmp, sizeof(info->security_patch)-1);

    tmp = adb_shell("adb shell settings get secure android_id 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        char aid[64];
        snprintf(aid, sizeof(aid), "AndroidID:%s", tmp);
        if (strlen(info->imei1) == 0 && strlen(tmp) >= 16)
            strncpy(info->imei1, tmp, sizeof(info->imei1)-1);
    }

    count = 0;
    for (int slot = 0; slot < 4; slot++) {
        snprintf(buf, sizeof(buf), "adb shell getprop persist.radio.sim.%d 2>/dev/null", slot);
        tmp = adb_shell(buf);
        trim_newline(tmp);
        if (strlen(tmp) > 0 && strlen(info->sim_state) == 0) {
            strncpy(info->sim_state, tmp, sizeof(info->sim_state)-1);
            count++;
        }
    }

    tmp = adb_shell("adb shell getprop gsm.operator.isroaming 2>/dev/null");
    trim_newline(tmp);
    if (strcmp(tmp, "true") == 0 && strlen(info->data_roaming) == 0)
        strncpy(info->data_roaming, "On", sizeof(info->data_roaming)-1);

    tmp = adb_shell("adb shell getprop persist.sys.account 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->google_accounts) == 0)
        snprintf(info->google_accounts, sizeof(info->google_accounts), "Account: %s", tmp);

    tmp = adb_shell("adb shell getprop ro.vendor.language 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->locale) == 0)
        strncpy(info->locale, tmp, sizeof(info->locale)-1);

    if (strlen(info->biometrics) == 0 && strlen(info->fingerprint) > 0)
        strncpy(info->biometrics, info->fingerprint, sizeof(info->biometrics)-1);
    if (strlen(info->biometrics) > 0 && strlen(info->face_unlock) > 0) {
        if (!strstr(info->biometrics, "face")) {
            append_str(info->biometrics, sizeof(info->biometrics), " face");
        }
    }
}
