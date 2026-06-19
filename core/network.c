#include "network.h"
#include "adb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void detect_network(DeviceInfo *info) {
    char *tmp, buf[BIG_STR];
    int count;

    count = 0;
    for (int iface = 0; iface < 12; iface++) {
        const char *ifnames[] = {"wlan0","eth0","usb0","rmnet0","rmnet1","rmnet2",
                                 "sit0","lo","dummy0","bond0","bt-pan","p2p0"};
        if (iface >= 12) break;
        snprintf(buf, sizeof(buf), "adb shell ip addr show %s 2>/dev/null | grep 'inet ' | awk '{print $2}'", ifnames[iface]);
        tmp = adb_shell(buf);
        trim_newline(tmp);
        if (strlen(tmp) > 0) {
            char line[128];
            if (count == 0) info->ip_list[0] = 0;
            snprintf(line, sizeof(line), "%s:%s ", ifnames[iface], tmp);
            append_str(info->ip_list, sizeof(info->ip_list), "%s", line);
            if (count == 0) {
                char *p = strchr(tmp, '/');
                if (p) { *p = 0; strncpy(info->ip, tmp, sizeof(info->ip)-1); }
                else strncpy(info->ip, tmp, sizeof(info->ip)-1);
            }
            count++;
        }
        snprintf(buf, sizeof(buf), "adb shell ip addr show %s 2>/dev/null | grep 'link/ether' | awk '{print $2}'", ifnames[iface]);
        tmp = adb_shell(buf);
        trim_newline(tmp);
        if (strlen(tmp) > 0) {
            char line[128];
            if (count == 12) info->mac_list[0] = 0;
            snprintf(line, sizeof(line), "%s:%s ", ifnames[iface], tmp);
            append_str(info->mac_list, sizeof(info->mac_list), "%s", line);
            if (count == 12) strncpy(info->mac, tmp, sizeof(info->mac)-1);
            count++;
        }
    }

    tmp = adb_shell("adb shell dumpsys wifi 2>/dev/null | grep 'mWifiInfo' | head -1");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        char *p = strstr(tmp, "SSID:");
        if (p) {
            p += 5;
            char *q = strchr(p, ' ');
            if (q) *q = 0;
            normalize_str(info->wifi_ssid, p);
        }
        p = strstr(tmp, "BSSID:");
        if (p) {
            p += 6;
            char *q = strchr(p, ' ');
            if (q) *q = 0;
            strncpy(info->wifi_bssid, p, sizeof(info->wifi_bssid)-1);
        }
        p = strstr(tmp, "RSSI:");
        if (p) {
            p += 5;
            char *q = strchr(p, ' ');
            if (q) *q = 0;
            strncpy(info->wifi_rssi, p, sizeof(info->wifi_rssi)-1);
        }
        p = strstr(tmp, "frequency:");
        if (p) {
            p += 10;
            char *q = strchr(p, ' ');
            if (q) *q = 0;
            strncpy(info->wifi_freq, p, sizeof(info->wifi_freq)-1);
        }
        p = strstr(tmp, "linkSpeed:");
        if (p) {
            p += 10;
            char *q = strchr(p, ' ');
            if (q) *q = 0;
            snprintf(buf, sizeof(buf), "%s Mbps", p);
            strncpy(info->gateway, buf, sizeof(info->gateway)-1);
        }
    }

    tmp = adb_shell("adb shell ip route 2>/dev/null | grep default | awk '{print $3}' | head -1");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        if (strlen(info->gateway) > 0 && strchr(info->gateway, '.')) {
        } else {
            strncpy(info->gateway, tmp, sizeof(info->gateway)-1);
        }
    }

    tmp = adb_shell("adb shell getprop net.dns1 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) == 0) {
        tmp = adb_shell("adb shell getprop dhcp.wlan0.dns1 2>/dev/null");
        trim_newline(tmp);
    }
    if (strlen(tmp) > 0) {
        char *d2 = adb_shell("adb shell getprop net.dns2 2>/dev/null");
        trim_newline(d2);
        if (strlen(d2) > 0) snprintf(info->dns_list, sizeof(info->dns_list), "%s %s", tmp, d2);
        else strncpy(info->dns_list, tmp, sizeof(info->dns_list)-1);
    }

    tmp = adb_shell("adb shell ip neigh 2>/dev/null | head -15");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        append_str(info->dns_list, sizeof(info->dns_list), " | ARP neighbors:\n%s", tmp);
    }

    tmp = adb_shell("adb shell settings get global http_proxy 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && strcmp(tmp, ":0") && strcmp(tmp, "null")) {
        append_str(info->dns_list, sizeof(info->dns_list), " | HTTP proxy: %s", tmp);
    }
}

void print_network_info(const DeviceInfo *info) {
    printf("  " BOLD "%-16s" RESET ": %s\n", "IP Addresses", info->ip_list);
    printf("  " BOLD "%-16s" RESET ": %s\n", "MAC Addresses", info->mac_list);
    if (strlen(info->wifi_ssid)) printf("  " BOLD "%-16s" RESET ": %s\n", "WiFi SSID", info->wifi_ssid);
    if (strlen(info->wifi_bssid)) printf("  " BOLD "%-16s" RESET ": %s\n", "WiFi BSSID", info->wifi_bssid);
    if (strlen(info->wifi_freq)) printf("  " BOLD "%-16s" RESET ": %s MHz\n", "WiFi Freq", info->wifi_freq);
    if (strlen(info->wifi_rssi)) printf("  " BOLD "%-16s" RESET ": %s dBm\n", "WiFi RSSI", info->wifi_rssi);
    if (strlen(info->gateway)) printf("  " BOLD "%-16s" RESET ": %s\n", "Gateway/Link", info->gateway);
    if (strlen(info->dns_list)) printf("  " BOLD "%-16s" RESET ": %s\n", "DNS", info->dns_list);
}
