#include "cpu.h"
#include "adb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

void detect_cpu(DeviceInfo *info) {
    char *tmp;

    /* CPU cores */
    tmp = adb_shell("adb shell nproc 2>/dev/null || adb shell grep -c ^processor /proc/cpuinfo 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->cpu_cores, tmp, sizeof(info->cpu_cores)-1);

    /* CPU ABIs */
    tmp = adb_shell("adb shell getprop ro.product.cpu.abilist 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->cpu_abilist, tmp, sizeof(info->cpu_abilist)-1);
    else {
        tmp = adb_shell("adb shell getprop ro.product.cpu.abi 2>/dev/null");
        trim_newline(tmp);
        if (strlen(tmp) > 0) strncpy(info->cpu_abilist, tmp, sizeof(info->cpu_abilist)-1);
    }

    /* Max frequency */
    tmp = adb_shell("adb shell cat /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq 2>/dev/null | head -1");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        int khz = atoi(tmp);
        if (khz > 0) snprintf(info->cpu_maxfreq, sizeof(info->cpu_maxfreq), "%.1f GHz", khz/1000000.0);
    }

    /* Governor */
    tmp = adb_shell("adb shell cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->cpu_governor, tmp, sizeof(info->cpu_governor)-1);
}

void detect_cpu_deep(DeviceInfo *info) {
    char *tmp;
    char buf[2048];
    buf[0]=0;

    /* Full cpuinfo */
    tmp = adb_shell("adb shell cat /proc/cpuinfo 2>/dev/null | head -40");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        /* Parse clusters */
        int big=0, mid=0, little=0, total=0;
        const char *p = tmp;
        while (*p) {
            if (strncmp(p, "processor", 9)==0) total++;
            if (strncmp(p, "CPU part", 8)==0) {
                int part;
                if (sscanf(p, "CPU part : %x", &part)==1) {
                    if (part==0x803 || part==0x804 || part==0x805 || part==0x806) big++;
                    else if (part==0x800 || part==0x801 || part==0x802) mid++;
                    else if (part<0x800) little++;
                }
            }
            while (*p && *p!='\n') p++;
            if (*p=='\n') p++;
        }
        if (total>0) {
            char arch[256]={0};
            if (strstr(tmp, "ARM part") || strstr(tmp, "CPU implementer")) {
                if (strstr(tmp, "0x41")) { /* ARM */
                    if (big) snprintf(arch,sizeof(arch),"ARM Cortex-%c%c%c (big)",
                                      big>1?'X':'A','7','8');
                    else if (mid) snprintf(arch,sizeof(arch),"ARM Cortex-A76/A78 (mid)");
                    else snprintf(arch,sizeof(arch),"ARM Cortex-A5x/A7x");
                }
            }
            snprintf(buf,sizeof(buf),"%d cores (%d big/%d mid/%d little) %s",
                     total,big,mid,little,arch);
            strncpy(info->cpu_clusters, buf, sizeof(info->cpu_clusters)-1);
        }
    }

    /* Min frequency */
    tmp = adb_shell("adb shell cat /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_min_freq 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        int khz = atoi(tmp);
        if (khz>0) snprintf(info->cpu_minfreq,sizeof(info->cpu_minfreq),"%.1f GHz",khz/1000000.0);
    }

    /* All clusters freq */
    char all_freq[2048]={0};
    for (int i=0; i<16; i++) {
        char path[128];
        snprintf(path,sizeof(path),"adb shell cat /sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq 2>/dev/null",i);
        tmp = adb_shell(path);
        trim_newline(tmp);
        if (strlen(tmp)>0) {
            char line[64];
            snprintf(line,sizeof(line),"CPU%d: %.1f GHz  ", i, atoi(tmp)/1000000.0);
            if (strlen(all_freq)+strlen(line) < sizeof(all_freq)-1) strcat(all_freq, line);
        }
    }
    if (strlen(all_freq)>0) {
        snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), " | %s", all_freq);
        strncpy(info->cpu_clusters, buf, sizeof(info->cpu_clusters)-1);
    }

    /* Kernel modules */
    tmp = adb_shell("adb shell lsmod 2>/dev/null | head -20");
    trim_newline(tmp);
    if (strlen(tmp)>0 && strlen(tmp)<sizeof(info->kernel_modules)-1)
        strncpy(info->kernel_modules, tmp, sizeof(info->kernel_modules)-1);

    /* SELinux */
    tmp = adb_shell("adb shell getenforce 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp)>0) strncpy(info->selinux_mode, tmp, sizeof(info->selinux_mode)-1);
}

void print_cpu_info(const DeviceInfo *info) {
    printf("  " BOLD "%-20s" RESET ": %s\n", "CPU Cores", info->cpu_cores);
    printf("  " BOLD "%-20s" RESET ": %s\n", "CPU Arch", info->cpu_abilist);
    if (strlen(info->cpu_maxfreq)) printf("  " BOLD "%-20s" RESET ": %s\n", "CPU Max Freq", info->cpu_maxfreq);
    if (strlen(info->cpu_minfreq)) printf("  " BOLD "%-20s" RESET ": %s\n", "CPU Min Freq", info->cpu_minfreq);
    if (strlen(info->cpu_governor)) printf("  " BOLD "%-20s" RESET ": %s\n", "CPU Governor", info->cpu_governor);
    if (strlen(info->cpu_clusters)) printf("  " BOLD "%-20s" RESET ": %s\n", "CPU Clusters", info->cpu_clusters);
    if (strlen(info->selinux_mode)) printf("  " BOLD "%-20s" RESET ": %s\n", "SELinux", info->selinux_mode);
    if (strlen(info->kernel_modules)) printf("  " BOLD "%-20s" RESET ": %s\n", "Kernel Modules", info->kernel_modules);
}
