#include "panel.h"
#include "adb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void detect_panel(DeviceInfo *info) {
    char *tmp;

    tmp = adb_shell("adb shell dumpsys display 2>/dev/null | head -60 | grep -i -E 'panel|vendor|manufacturer|name|dsi|lcm|display' | head -10");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        char *p;
        if (strlen(info->panel_manuf) == 0) {
            p = strstr(tmp, "vendor");
            if (!p) p = strstr(tmp, "manufacturer");
            if (!p) p = strstr(tmp, "VENDOR");
            if (!p) p = strstr(tmp, "MANUFACTURER");
            if (p) {
                char *q = strchr(p, '=');
                if (!q) q = strchr(p, ':');
                if (q) {
                    q++; while (*q == ' ') q++;
                    char *r = strchr(q, '\n'); if (r) *r = 0;
                    r = strchr(q, ' '); if (r && strlen(q) > 20) *r = 0;
                    strncpy(info->panel_manuf, q, sizeof(info->panel_manuf)-1);
                }
            }
        }
        if (strlen(info->panel_type) == 0) {
            p = strstr(tmp, "type");
            if (!p) p = strstr(tmp, "TYPE");
            if (p) {
                char *q = strchr(p, '=');
                if (!q) q = strchr(p, ':');
                if (q) {
                    q++; while (*q == ' ') q++;
                    char *r = strchr(q, '\n'); if (r) *r = 0;
                    r = strchr(q, ' '); if (r && strlen(q) > 20) *r = 0;
                    strncpy(info->panel_type, q, sizeof(info->panel_type)-1);
                }
            }
        }
        if (strlen(info->display_feat) == 0) {
            p = strstr(tmp, "name");
            if (!p) p = strstr(tmp, "NAME");
            if (!p) p = strstr(tmp, "dsi");
            if (!p) p = strstr(tmp, "lcm");
            if (p) {
                char buf[128];
                int idx = p - tmp;
                int max_cpy = strlen(tmp) - idx;
                if (max_cpy > 120) max_cpy = 120;
                strncpy(buf, p, max_cpy); buf[max_cpy] = 0;
                char *q = strchr(buf, '\n'); if (q) *q = 0;
                strncpy(info->display_feat, buf, sizeof(info->display_feat)-1);
            }
        }
    }

    if (strlen(info->panel_manuf) == 0) {
        tmp = adb_shell("adb shell cat /sys/class/graphics/fb0/name 2>/dev/null");
        trim_newline(tmp);
        if (strlen(tmp) > 0) strncpy(info->panel_manuf, tmp, sizeof(info->panel_manuf)-1);
    }

    if (strlen(info->panel_manuf) == 0) {
        tmp = adb_shell("adb shell dumpsys display 2>/dev/null | grep -i 'mBuiltInDisplayId' -A5 | head -6");
        trim_newline(tmp);
        if (strlen(tmp) > 0 && strlen(info->panel_manuf) == 0) {
            char *p = strstr(tmp, "mDisplayInfo");
            if (p) {
                char *q = strchr(p, '{'); if (q) q++; char *r = strchr(q, '}'); if (r) *r = 0;
                strncpy(info->panel_manuf, q, sizeof(info->panel_manuf)-1);
            }
        }
    }

    if (strlen(info->panel_manuf) == 0) {
        tmp = adb_shell("adb shell getprop ro.vendor.display.panel 2>/dev/null");
        trim_newline(tmp);
        if (strlen(tmp) > 0) strncpy(info->panel_manuf, tmp, sizeof(info->panel_manuf)-1);
    }

    if (strlen(info->panel_manuf) == 0) {
        tmp = adb_shell("adb shell getprop ro.sf.lcd_density 2>/dev/null");
        trim_newline(tmp);
        int dpi = atoi(tmp);
        if (dpi > 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "Panel dpi: %d", dpi);
            strncpy(info->panel_manuf, buf, sizeof(info->panel_manuf)-1);
        }
    }

    if (strlen(info->panel_type) == 0) {
        tmp = adb_shell("adb shell dumpsys display 2>/dev/null | grep -i -E 'isHdr|hdr|HDR|isWideColor|WCG' | head -5");
        trim_newline(tmp);
        if (strlen(tmp) > 0) {
            if (strstr(tmp, "isHdr") || strstr(tmp, "HDR10"))
                strncpy(info->panel_type, "HDR panel", sizeof(info->panel_type)-1);
        }
    }

    if (strlen(info->panel_type) == 0) {
        tmp = adb_shell("adb shell dumpsys display 2>/dev/null | grep -i -E 'amoled|OLED|Super|IPS|TFT|LCD' | head -3");
        trim_newline(tmp);
        if (strlen(tmp) > 0) strncpy(info->panel_type, tmp, sizeof(info->panel_type)-1);
    }

    if (strlen(info->display_feat) == 0) {
        tmp = adb_shell("adb shell dumpsys display 2>/dev/null | grep -i -E 'hasHdr|hdrCapabilities|colorMode|supportedModes' | head -3");
        trim_newline(tmp);
        if (strlen(tmp) > 0) strncpy(info->display_feat, tmp, sizeof(info->display_feat)-1);
    }

    if (strlen(info->aod) == 0) {
        tmp = adb_shell("adb shell dumpsys display 2>/dev/null | grep -i 'isAlwaysOn' | head -1");
        trim_newline(tmp);
        if (strlen(tmp) > 0) {
            if (strstr(tmp, "true")) strncpy(info->aod, "Supported", sizeof(info->aod)-1);
            else if (strstr(tmp, "false")) strncpy(info->aod, "Not supported", sizeof(info->aod)-1);
        }
    }
    if (strlen(info->aod) == 0) {
        tmp = adb_shell("adb shell settings get secure doze_enabled 2>/dev/null");
        trim_newline(tmp);
        if (strcmp(tmp, "1") == 0) strncpy(info->aod, "Enabled (Doze)", sizeof(info->aod)-1);
        tmp = adb_shell("adb shell settings get global aod_mode 2>/dev/null");
        trim_newline(tmp);
        if (strcmp(tmp, "1") == 0) strncpy(info->aod, "Enabled", sizeof(info->aod)-1);
    }

    if (strlen(info->color_gamut) == 0) {
        tmp = adb_shell("adb shell dumpsys display 2>/dev/null | grep -iE 'colorMode|color.gamut|wideColor|WCG|Display.Color' | head -3");
        trim_newline(tmp);
        if (strlen(tmp) > 0) strncpy(info->color_gamut, tmp, sizeof(info->color_gamut)-1);
    }
    if (strlen(info->color_gamut) == 0) {
        tmp = adb_shell("adb shell settings get system display_color_mode 2>/dev/null");
        trim_newline(tmp);
        if (strlen(tmp) > 0) {
            char *cm[] = {"Natural", "Boosted", "Saturated", "Adaptive"};
            int cmi = atoi(tmp);
            if (cmi >= 0 && cmi <= 3)
                snprintf(info->color_gamut, sizeof(info->color_gamut), "Mode %d (%s)", cmi, cm[cmi]);
            else snprintf(info->color_gamut, sizeof(info->color_gamut), "Mode %d", cmi);
        }
    }

    if (strlen(info->touch_sampling) == 0) {
        tmp = adb_shell("adb shell dumpsys input 2>/dev/null | grep -iE 'touch.*rate|sampling|fps' | head -3");
        trim_newline(tmp);
        if (strlen(tmp) > 0) strncpy(info->touch_sampling, tmp, sizeof(info->touch_sampling)-1);
        else {
            tmp = adb_shell("adb shell getevent -p 2>/dev/null | grep -i 'touch' -A5 | grep -i 'maxFreq|resolution|freq' | head -3");
            trim_newline(tmp);
            if (strlen(tmp) > 0) strncpy(info->touch_sampling, tmp, sizeof(info->touch_sampling)-1);
        }
    }
}

void print_panel_info(const DeviceInfo *info) {
    if (strlen(info->panel_manuf)) printf("  " BOLD "%-17s" RESET ": %s\n", "Panel", info->panel_manuf);
    if (strlen(info->panel_type)) printf("  " BOLD "%-17s" RESET ": %s\n", "Panel Type", info->panel_type);
    if (strlen(info->display_feat)) printf("  " BOLD "%-17s" RESET ": %s\n", "Display Feats", info->display_feat);
    if (strlen(info->aod)) printf("  " BOLD "%-17s" RESET ": %s\n", "AOD", info->aod);
    if (strlen(info->color_gamut)) printf("  " BOLD "%-17s" RESET ": %s\n", "Color Gamut", info->color_gamut);
    if (strlen(info->touch_sampling)) printf("  " BOLD "%-17s" RESET ": %s\n", "Touch Sampling", info->touch_sampling);
}
