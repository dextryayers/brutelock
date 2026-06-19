#ifndef ADB_SYNC_H
#define ADB_SYNC_H

#ifdef __cplusplus
extern "C" {
#endif

/* ── ADB SYNC service for native file pull/push ── */

/* List directory contents via SYNC service */
/* Returns number of entries found, entries stored in output buffer */
int adb_sync_list(const char *remote_dir, char *output, int out_sz);

/* Pull (receive) a file from device */
/* Reads file data into buffer, returns bytes received, -1 on error */
int adb_sync_pull(const char *remote_path, unsigned char *data, int maxlen);

/* Pull and save to local file */
int adb_sync_pull_file(const char *remote_path, const char *local_path);

/* Push (send) a file to device */
int adb_sync_push(const char *local_path, const char *remote_path);

/* Push raw data to device file */
int adb_sync_push_data(const unsigned char *data, int len,
                       const char *remote_path);

/* Stat a remote file (returns 0 if exists, -1 if error) */
int adb_sync_stat(const char *remote_path, unsigned int *size,
                  unsigned int *mode);

/* High-level: Pull locksettings.db from recovery */
int adb_sync_pull_locksettings(const char *local_dir);

/* Check if file exists on device */
int adb_sync_exists(const char *remote_path);

#ifdef __cplusplus
}
#endif

#endif
