#include "sysfs.h"
#include "adb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void detect_sysfs(DeviceInfo *info) {
    char *tmp;

    tmp = adb_shell("adb shell uname -a 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->kernel_full, tmp, sizeof(info->kernel_full)-1);

    tmp = adb_shell("adb shell getenforce 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->selinux_mode, tmp, sizeof(info->selinux_mode)-1);

    tmp = adb_shell("adb shell cat /proc/partitions 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->partitions, tmp, sizeof(info->partitions)-1);

    tmp = adb_shell("adb shell mount 2>/dev/null | head -40");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->mounts, tmp, sizeof(info->mounts)-1);

    tmp = adb_shell("adb shell cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->cpu_governor, tmp, sizeof(info->cpu_governor)-1);

    tmp = adb_shell("adb shell cat /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_min_freq 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        long f = atol(tmp) / 1000;
        snprintf(info->cpu_minfreq, sizeof(info->cpu_minfreq), "%ld MHz", f);
    }

    tmp = adb_shell("adb shell cat /proc/swaps 2>/dev/null | grep -v Filename | head -5");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->swap_total, tmp, sizeof(info->swap_total)-1);

    tmp = adb_shell("adb shell cat /proc/meminfo 2>/dev/null | head -20");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->mem_details, tmp, sizeof(info->mem_details)-1);

    tmp = adb_shell("adb shell lsmod 2>/dev/null | head -30");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->kernel_modules, tmp, sizeof(info->kernel_modules)-1);
}

void print_sysfs_info(const DeviceInfo *info) {
    printf("  " BOLD "%-16s" RESET ": %s\n", "Kernel", info->kernel_full);
    if (strlen(info->selinux_mode)) printf("  " BOLD "%-16s" RESET ": %s\n", "SELinux", info->selinux_mode);
    if (strlen(info->cpu_governor)) printf("  " BOLD "%-16s" RESET ": %s\n", "CPU Governor", info->cpu_governor);
    if (strlen(info->cpu_minfreq)) printf("  " BOLD "%-16s" RESET ": %s\n", "CPU Min Freq", info->cpu_minfreq);
    if (strlen(info->swap_total)) printf("  " BOLD "%-16s" RESET ": %s\n", "Swap", info->swap_total);
    if (strlen(info->kernel_modules)) {
        printf("  " BOLD "%-16s" RESET ":\n%s\n", "Kernel Modules", info->kernel_modules);
    }
}
