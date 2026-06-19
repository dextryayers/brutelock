#include "cam.h"
#include "adb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void detect_cam(DeviceInfo *info) {
    char *tmp;
    char buf[BIG_STR];
    int count;

    tmp = adb_shell("adb shell dumpsys media.camera 2>/dev/null | grep -iE 'rear|back|main|camera' | head -10");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->rear_cams, tmp, sizeof(info->rear_cams)-1);

    tmp = adb_shell("adb shell dumpsys media.camera 2>/dev/null | grep -iE 'front|selfie' | head -5");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->front_cam, tmp, sizeof(info->front_cam)-1);

    count = 0;
    buf[0] = 0;
    for (int i = 0; i < 10; i++) {
        snprintf(buf, sizeof(buf),
                 "adb shell dumpsys media.camera 2>/dev/null | grep -iA2 'Camera %d' | head -6", i);
        tmp = adb_shell(buf);
        trim_newline(tmp);
        if (strlen(tmp) > 0) {
            if (count == 0) info->rear_cams[0] = 0;
            append_str(info->rear_cams, sizeof(info->rear_cams), "[Cam%d] %s ", i, tmp);
            count++;
        }
    }

    tmp = adb_shell("adb shell dumpsys media.camera 2>/dev/null | grep -iE 'resolution|size|video|4k|1080p' | head -5");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->video_cap, tmp, sizeof(info->video_cap)-1);
    else {
        tmp = adb_shell("adb shell dumpsys media 2>/dev/null | grep -iE 'camcorder|profile' | head -3");
        trim_newline(tmp);
        if (strlen(tmp) > 0) strncpy(info->video_cap, tmp, sizeof(info->video_cap)-1);
    }

    tmp = adb_shell("adb shell dumpsys camera 2>/dev/null | grep -iE 'flash|led' | head -3");
    trim_newline(tmp);
    if (strlen(tmp) > 0 && !strstr(tmp, "0")) strncpy(info->flash, "LED flash detected", sizeof(info->flash)-1);
    else strncpy(info->flash, "LED", sizeof(info->flash)-1);

    if (strlen(info->rear_cams) > 0) {
        char tmp2[BIG_STR];
        strncpy(tmp2, info->rear_cams, sizeof(tmp2)-1);
        int num_cams = 0;
        char *p = tmp2;
        while ((p = strstr(p, "[Cam"))) { num_cams++; p++; }
        snprintf(buf, sizeof(buf), "%d camera(s)", num_cams);
        strncpy(info->camera_info, buf, sizeof(info->camera_info)-1);
    }
}

void print_cam_info(const DeviceInfo *info) {
    if (strlen(info->camera_info)) printf("  " BOLD "%-20s" RESET ": %s\n", "Camera System", info->camera_info);
    if (strlen(info->rear_cams)) {
        printf("  " BOLD "%-20s" RESET ":\n", "Rear Cameras");
        char copy[BIG_STR];
        strncpy(copy, info->rear_cams, sizeof(copy)-1);
        char *line = strtok(copy, "\n");
        while (line) {
            if (strlen(line) > 0) printf("    %s\n", line);
            line = strtok(NULL, "\n");
        }
    }
    if (strlen(info->front_cam)) printf("  " BOLD "%-20s" RESET ": %s\n", "Front Camera", info->front_cam);
    if (strlen(info->video_cap)) printf("  " BOLD "%-20s" RESET ": %s\n", "Video", info->video_cap);
    if (strlen(info->flash)) printf("  " BOLD "%-20s" RESET ": %s\n", "Flash", info->flash);
}
