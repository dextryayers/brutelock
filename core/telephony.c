#include "telephony.h"
#include "adb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void detect_telephony(DeviceInfo *info) {
    char *tmp;

    tmp = adb_shell("adb shell dumpsys iphonesubinfo 2>/dev/null | grep -oP '\\d{14,16}' | head -1");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(tmp) >= 14) strncpy(info->imei1, tmp, sizeof(info->imei1)-1);
    else {
        tmp = adb_shell("adb shell service call iphonesubinfo 1 2>/dev/null | grep -oP '\\d{14,16}' | head -1");
        trim_newline(tmp);
        if (strlen(tmp) > 0 && strlen(tmp) >= 14) strncpy(info->imei1, tmp, sizeof(info->imei1)-1);
    }

    if (strlen(info->imei1) == 0) {
        tmp = adb_shell("adb shell getprop ril.imei 2>/dev/null");
        trim_newline(tmp);
        if (strlen(tmp) >= 14) strncpy(info->imei1, tmp, sizeof(info->imei1)-1);
        else {
            tmp = adb_shell("adb shell getprop persist.radio.imei 2>/dev/null");
            trim_newline(tmp);
            if (strlen(tmp) >= 14) strncpy(info->imei1, tmp, sizeof(info->imei1)-1);
            else {
                tmp = adb_shell("adb shell getprop gsm.baseband.imei 2>/dev/null");
                trim_newline(tmp);
                if (strlen(tmp) >= 14) strncpy(info->imei1, tmp, sizeof(info->imei1)-1);
            }
        }
    }

    if (strlen(info->imei) == 0 && strlen(info->imei1) > 0)
        strncpy(info->imei, info->imei1, sizeof(info->imei)-1);

    if (strlen(info->imei1) > 0) {
        tmp = adb_shell("adb shell dumpsys iphonesubinfo 2>/dev/null | grep -oP '\\d{14,16}' | tail -1");
        trim_newline(tmp);
        if (strlen(tmp) > 0 && strcmp(tmp, info->imei1) && strlen(tmp) >= 14) {
            strncpy(info->imei2_2, tmp, sizeof(info->imei2_2)-1);
            strncpy(info->imei2, tmp, sizeof(info->imei2)-1);
        }
    }

    if (strlen(info->imei2) == 0) {
        tmp = adb_shell("adb shell getprop ril.imei2 2>/dev/null");
        trim_newline(tmp);
        if (strlen(tmp) >= 14) strncpy(info->imei2, tmp, sizeof(info->imei2)-1);
        else {
            tmp = adb_shell("adb shell getprop persist.radio.imei2 2>/dev/null");
            trim_newline(tmp);
            if (strlen(tmp) >= 14) strncpy(info->imei2, tmp, sizeof(info->imei2)-1);
        }
    }

    if (strlen(info->imei1) > 0) {
        tmp = adb_shell("adb shell dumpsys iphonesubinfo 2>/dev/null | grep -oP '\\d{14,16}' | tail -1");
        trim_newline(tmp);
        if (strlen(tmp) > 0) strncpy(info->meid, tmp, sizeof(info->meid)-1);
    }

    tmp = adb_shell("adb shell service call iphonesubinfo 7 2>/dev/null | grep -oP '\\d{14,16}' | head -1");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->imsi, tmp, sizeof(info->imsi)-1);

    tmp = adb_shell("adb shell settings get secure android_id 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strlen(info->imsi) == 0) {
        strncpy(info->imsi, tmp, sizeof(info->imsi)-1);
    }

    tmp = adb_shell("adb shell getprop gsm.operator.alpha 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->network_operator, tmp, sizeof(info->network_operator)-1);

    tmp = adb_shell("adb shell getprop gsm.sim.operator.alpha 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->sim_operator, tmp, sizeof(info->sim_operator)-1);

    tmp = adb_shell("adb shell getprop gsm.network.type 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->network_type, tmp, sizeof(info->network_type)-1);
    if (strlen(info->network_type) == 0) {
        tmp = adb_shell("adb shell dumpsys telephony.registry 2>/dev/null | grep 'mDataNetworkType' | head -1");
        trim_newline(tmp);
        if (strlen(tmp) > 0) {
            char *p = strstr(tmp, "mDataNetworkType=");
            if (p) { p += 17; char *q = strchr(p, ' '); if (q) *q = 0; strncpy(info->network_type, p, sizeof(info->network_type)-1); }
        }
    }

    if (strlen(info->network_type) == 0) {
        tmp = adb_shell("adb shell getprop gsm.operator.numeric 2>/dev/null");
        trim_newline(tmp);
        if (strlen(tmp) >= 5) {
            char mcc_mnc[8]; strncpy(mcc_mnc, tmp, 5); mcc_mnc[5] = 0;
            if (strlen(info->network_operator) > 0) {
                append_str(info->network_operator, sizeof(info->network_operator), " (MCC/MNC:%s)", mcc_mnc);
            } else snprintf(info->network_operator, sizeof(info->network_operator), "MCC/MNC:%s", mcc_mnc);
        }
    }

    tmp = adb_shell("adb shell dumpsys telephony.registry 2>/dev/null | grep 'mSignalStrength' | head -1 | grep -oP '\\d+'");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->signal_strength, tmp, sizeof(info->signal_strength)-1);

    if (strlen(info->signal_strength) == 0) {
        tmp = adb_shell("adb shell dumpsys telephony.registry 2>/dev/null | grep 'mSignalStrength' | head -1");
        trim_newline(tmp);
        if (strlen(tmp) > 0) {
            char *p = strstr(tmp, "mSignalStrength=");
            if (p) { p += 16; char *q = strchr(p, ' '); if (q) *q = 0; strncpy(info->signal_strength, p, sizeof(info->signal_strength)-1); }
        }
    }

    tmp = adb_shell("adb shell getprop gsm.sim.state 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->sim_state, tmp, sizeof(info->sim_state)-1);

    if (strlen(info->sim_state) == 0) {
        for (int s = 0; s < 2; s++) {
            char key[64];
            snprintf(key, sizeof(key), "adb shell getprop gsm.sim.state.%d 2>/dev/null", s);
            tmp = adb_shell(key);
            trim_newline(tmp);
            if (strlen(tmp) > 0) {
                if (s == 0) strncpy(info->sim_state, tmp, sizeof(info->sim_state)-1);
                else { append_str(info->sim_state, sizeof(info->sim_state), " / SIM%d:%s", s, tmp); }
            }
        }
    }

    tmp = adb_shell("adb shell dumpsys telephony.registry 2>/dev/null | grep 'mDataRoaming' | head -1 | grep -oP '[01]'");
    trim_newline(tmp);
    if (strcmp(tmp, "1") == 0) strncpy(info->data_roaming, "On", sizeof(info->data_roaming)-1);
    else if (strcmp(tmp, "0") == 0) strncpy(info->data_roaming, "Off", sizeof(info->data_roaming)-1);
    else {
        tmp = adb_shell("adb shell getprop gsm.operator.isroaming 2>/dev/null");
        trim_newline(tmp);
        if (strcmp(tmp, "true") == 0) strncpy(info->data_roaming, "On", sizeof(info->data_roaming)-1);
    }

    tmp = adb_shell("adb shell dumpsys telephony.registry 2>/dev/null | grep -iE 'mCellIdentity|mCellLocation|cellid|CellID' | head -3");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->cell_id, tmp, sizeof(info->cell_id)-1);
    else {
        tmp = adb_shell("adb shell service call phone 100 2>/dev/null | grep -oP '\\d+' | head -5");
        trim_newline(tmp);
        if (strlen(tmp) > 0) strncpy(info->cell_id, tmp, sizeof(info->cell_id)-1);
    }

    tmp = adb_shell("adb shell dumpsys telephony.registry 2>/dev/null | grep -iE 'lac|LAC|LocationArea' | head -1");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->lac, tmp, sizeof(info->lac)-1);
    else {
        tmp = adb_shell("adb shell dumpsys telephony.registry 2>/dev/null | grep 'mCellLocation' | head -1 | grep -oP 'LAC=\\d+' | head -1");
        trim_newline(tmp);
        if (strlen(tmp) > 0) {
            char *p = strchr(tmp, '=');
            if (p) { p++; strncpy(info->lac, p, sizeof(info->lac)-1); }
        }
    }

    tmp = adb_shell("adb shell getprop gsm.operator.ispsdigit 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->phone_number, tmp, sizeof(info->phone_number)-1);
    else {
        tmp = adb_shell("adb shell service call iphonesubinfo 11 2>/dev/null | grep -oP '\\+?\\d{7,15}' | head -1");
        trim_newline(tmp);
        if (strlen(tmp) > 0) strncpy(info->phone_number, tmp, sizeof(info->phone_number)-1);
        else {
            tmp = adb_shell("adb shell getprop gsm.operator.iso-country 2>/dev/null");
            trim_newline(tmp);
        }
    }

    tmp = adb_shell("adb shell dumpsys telephony.registry 2>/dev/null | grep -iE 'volte|VoLTE|ims' | head -3");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        if (strstr(tmp, "1") || strstr(tmp, "true") || strstr(tmp, "available"))
            strncpy(info->volte, "Enabled", sizeof(info->volte)-1);
        else strncpy(info->volte, "Disabled", sizeof(info->volte)-1);
    }
    if (strlen(info->volte) == 0) {
        tmp = adb_shell("adb shell getprop persist.dbg.volte_avail_ovr 2>/dev/null");
        trim_newline(tmp);
        if (strcmp(tmp, "1") == 0) strncpy(info->volte, "Available", sizeof(info->volte)-1);
        else {
            tmp = adb_shell("adb shell getprop ro.vendor.volte 2>/dev/null");
            trim_newline(tmp);
            if (strcmp(tmp, "1") == 0) strncpy(info->volte, "Supported", sizeof(info->volte)-1);
        }
    }
}

void print_telephony_info(const DeviceInfo *info) {
    printf("  " BOLD "%-17s" RESET ": %s\n", "IMEI (SIM1)", info->imei1);
    if (strlen(info->imei2)) printf("  " BOLD "%-17s" RESET ": %s\n", "IMEI (SIM2)", info->imei2);
    else if (strlen(info->imei2_2)) printf("  " BOLD "%-17s" RESET ": %s\n", "IMEI (SIM2)", info->imei2_2);
    if (strlen(info->meid)) printf("  " BOLD "%-17s" RESET ": %s\n", "MEID", info->meid);
    if (strlen(info->imsi)) printf("  " BOLD "%-17s" RESET ": %s\n", "IMSI/ID", info->imsi);
    if (strlen(info->phone_number)) printf("  " BOLD "%-17s" RESET ": %s\n", "Phone Number", info->phone_number);
    if (strlen(info->network_operator)) printf("  " BOLD "%-17s" RESET ": %s\n", "Operator", info->network_operator);
    if (strlen(info->sim_operator)) printf("  " BOLD "%-17s" RESET ": %s\n", "SIM Operator", info->sim_operator);
    if (strlen(info->network_type)) printf("  " BOLD "%-17s" RESET ": %s\n", "Network Type", info->network_type);
    if (strlen(info->signal_strength)) printf("  " BOLD "%-17s" RESET ": %s\n", "Signal", info->signal_strength);
    if (strlen(info->sim_state)) printf("  " BOLD "%-17s" RESET ": %s\n", "SIM State", info->sim_state);
    if (strlen(info->volte)) printf("  " BOLD "%-17s" RESET ": %s\n", "VoLTE", info->volte);
    if (strlen(info->data_roaming)) printf("  " BOLD "%-17s" RESET ": %s\n", "Roaming", info->data_roaming);
    if (strlen(info->cell_id)) printf("  " BOLD "%-17s" RESET ": %s\n", "Cell ID", info->cell_id);
    if (strlen(info->lac)) printf("  " BOLD "%-17s" RESET ": %s\n", "LAC", info->lac);
}
