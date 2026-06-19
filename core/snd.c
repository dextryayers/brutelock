#include "snd.h"
#include "adb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void detect_snd(DeviceInfo *info) {
    char *tmp, buf[BIG_STR];

    tmp = adb_shell("adb shell dumpsys media.audio_policy 2>/dev/null | grep -i 'codec' | head -5");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->audio_codec, tmp, sizeof(info->audio_codec)-1);

    tmp = adb_shell("adb shell dumpsys audio 2>/dev/null | grep -iE 'dsp|audio.dsp|chip' | head -3");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->audio_dsp, tmp, sizeof(info->audio_dsp)-1);

    tmp = adb_shell("adb shell dumpsys audio 2>/dev/null | grep -iE 'speaker|stereo|mono' | head -3");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        if (strstr(tmp, "stereo") || strstr(tmp, "dual")) strncpy(info->speaker_cfg, "Stereo", sizeof(info->speaker_cfg)-1);
        else if (strstr(tmp, "mono")) strncpy(info->speaker_cfg, "Mono", sizeof(info->speaker_cfg)-1);
        else strncpy(info->speaker_cfg, tmp, sizeof(info->speaker_cfg)-1);
    }

    tmp = adb_shell("adb shell dumpsys audio 2>/dev/null | grep -c 'input sink'");
    trim_newline(tmp);
    int mic = atoi(tmp);
    if (mic == 0) {
        tmp = adb_shell("adb shell dumpsys audio 2>/dev/null | grep -c 'mic'");
        trim_newline(tmp);
        mic = atoi(tmp);
    }
    if (mic > 0) snprintf(info->mic_num, sizeof(info->mic_num), "%d", mic);

    tmp = adb_shell("adb shell dumpsys audio 2>/dev/null | grep -iE 'dolby|atmos|atmos|hi.res|hi-res' | head -3");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        buf[0] = 0;
        if (strstr(tmp, "dolby")) append_str(buf, sizeof(buf), "Dolby ");
        if (strstr(tmp, "atmos")) append_str(buf, sizeof(buf), "Atmos ");
        if (strstr(tmp, "hi-res") || strstr(tmp, "hi.res")) append_str(buf, sizeof(buf), "Hi-Res ");
        strncpy(info->audio_feat, buf, sizeof(info->audio_feat)-1);
    }
    if (strlen(info->audio_feat) == 0) {
        tmp = adb_shell("adb shell getprop ro.vendor.audio.dolby 2>/dev/null");
        trim_newline(tmp);
        if (strcmp(tmp, "1") == 0) strncpy(info->audio_feat, "Dolby Atmos", sizeof(info->audio_feat)-1);
    }
}

void print_snd_info(const DeviceInfo *info) {
    if (strlen(info->audio_dsp)) printf("  " BOLD "%-20s" RESET ": %s\n", "Audio DSP", info->audio_dsp);
    if (strlen(info->audio_codec)) printf("  " BOLD "%-20s" RESET ": %s\n", "Audio Codec", info->audio_codec);
    if (strlen(info->speaker_cfg)) printf("  " BOLD "%-20s" RESET ": %s\n", "Speakers", info->speaker_cfg);
    if (strlen(info->mic_num)) printf("  " BOLD "%-20s" RESET ": %s mic(s)\n", "Microphones", info->mic_num);
    if (strlen(info->audio_feat)) printf("  " BOLD "%-20s" RESET ": %s\n", "Audio Features", info->audio_feat);
}
