#define _GNU_SOURCE
#include "safe.h"
#include "detect.h"
#include "network.h"
#include "sysfs.h"
#include "database.h"
#include "service.h"
#include "hardware.h"
#include "account.h"
#include "telephony.h"
#include "extra.h"
#include "devices.h"
#include "debug.h"
#include "bypass.h"
#include "chip.h"
#include "panel.h"
#include "cam.h"
#include "snd.h"
#include "pwr.h"
#include "radio.h"
#include "mem.h"
#include "sec.h"
#include "sens.h"
#include "allprop.h"
#include "filesys.h"
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#define ALARM_TIMEOUT 25

static volatile int alarm_fired = 0;

static void alarm_handler(int sig) {
    (void)sig;
    alarm_fired = 1;
}

static void stage(const char *name) {
    printf("[STAGE:%s]\n", name);
    fflush(stdout);
}

void safe_detect_all(DeviceInfo *info) {
    alarm_fired = 0;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = alarm_handler;
    sigaction(SIGALRM, &sa, NULL);

    alarm(ALARM_TIMEOUT);

    stage("device");    if (!alarm_fired) detect_device(info);
    stage("network");   if (!alarm_fired) detect_network(info);
    stage("sysfs");     if (!alarm_fired) detect_sysfs(info);
    stage("database");  if (!alarm_fired) detect_database(info);
    stage("services");  if (!alarm_fired) detect_services(info);
    stage("hardware");  if (!alarm_fired) detect_hardware(info);
    stage("accounts");  if (!alarm_fired) detect_accounts(info);
    stage("telephony"); if (!alarm_fired) detect_telephony(info);
    stage("extra");     if (!alarm_fired) detect_extra(info);
    stage("devices");   if (!alarm_fired) detect_devices(info);
    stage("debug");     if (!alarm_fired) detect_debug(info);
    stage("bypass");    if (!alarm_fired) detect_bypass(info);
    stage("chip");      if (!alarm_fired) detect_chip(info);
    stage("panel");     if (!alarm_fired) detect_panel(info);
    stage("cam");       if (!alarm_fired) detect_cam(info);
    stage("snd");       if (!alarm_fired) detect_snd(info);
    stage("pwr");       if (!alarm_fired) detect_pwr(info);
    stage("radio");     if (!alarm_fired) detect_radio(info);
    stage("mem");       if (!alarm_fired) detect_mem(info);
    stage("sec");       if (!alarm_fired) detect_sec(info);
    stage("sens");      if (!alarm_fired) detect_sens(info);
    stage("filesys");   if (!alarm_fired) detect_filesys(info);
    stage("allprop");   if (!alarm_fired) detect_allprop(info);

    alarm(0);
    signal(SIGALRM, SIG_DFL);

    if (alarm_fired) {
        fprintf(stderr, "\n[!] Detection timed out after %ds. Some info missing.\n", ALARM_TIMEOUT);
    }
}
