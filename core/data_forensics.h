#ifndef DATA_FORENSICS_H
#define DATA_FORENSICS_H

#include <string>
#include <vector>
#include <map>
#include <cstdint>

struct SQLiteRecord {
    int64_t rowid;
    std::vector<std::string> fields;
};

struct SQLiteTable {
    std::string name;
    std::string sql;
};

class SQLiteDB {
public:
    unsigned char *data;
    size_t size;
    int page_count;
    int page_size;

    SQLiteDB();
    ~SQLiteDB();
    SQLiteDB(const SQLiteDB &) = delete;
    SQLiteDB &operator=(const SQLiteDB &) = delete;

    bool parse(const unsigned char *raw, size_t raw_size);
    std::vector<SQLiteRecord> query(const std::string &table);
    std::vector<SQLiteTable> get_tables();
};

struct AccountInfo {
    std::string type, name, password_hash;
    std::vector<std::string> tokens;
    bool is_google, is_samsung, is_xiaomi, is_facebook;
};

struct ContactEntry {
    std::string name, number, email;
};

struct CallLogEntry {
    std::string number, name, type, date, duration;
};

struct SmsEntry {
    std::string address, body, date, type;
};

struct WifiEntry {
    std::string ssid, psk, key_mgmt, bssid;
    bool hidden;
};

struct AppInfo {
    std::string package, name, version, data_dir;
    bool is_system, is_launcher, is_locked;
};

struct DeviceData {
    /* Authentication data */
    std::string gatekeeper_password_key;     /* /data/system/gatekeeper.password.key */
    std::string gatekeeper_pattern_key;      /* /data/system/gatekeeper.pattern.key */
    std::string gesture_key;                 /* /data/system/gesture.key (legacy) */
    std::string locksettings_db;             /* /data/system/locksettings.db */
    std::string locksettings_db_wal;         /* Write-ahead log */

    /* Lock settings */
    std::string lock_type;                   /* PIN, Password, Pattern, None */
    int pin_length;
    int password_quality;
    bool has_fingerprint, has_face;

    /* Accounts */
    std::vector<AccountInfo> accounts;
    std::string google_token;
    std::string samsung_token;

    /* Personal data */
    std::vector<ContactEntry> contacts;
    std::vector<CallLogEntry> call_logs;
    std::vector<SmsEntry> sms_messages;
    std::vector<WifiEntry> wifi_networks;
    std::vector<std::string> photos;

    /* Apps */
    std::vector<AppInfo> apps;

    /* System data */
    std::string build_prop, default_prop;
    std::string wpa_supplicant_conf;

    /* Encryption */
    bool is_encrypted;
    std::string encryption_type;
    std::string fe_key;                      /* File-based encryption key */
    std::string credential_encrypted_key;

    /* Call recordings */
    std::vector<std::string> call_recordings;
    /* Messenger data */
    std::string whatsapp_db;
    std::string telegram_db;
    std::string signal_db;
    std::string browser_history;
    std::string clipboard_text;
    std::string notification_history;

    std::string to_json() const;
};

class DataForensics {
public:
    DataForensics();
    ~DataForensics();

    /* Extract all possible data from device */
    DeviceData extract_all();
    DeviceData extract_from_adb();
    DeviceData extract_from_recovery();
    DeviceData extract_from_fastboot();

    /* Specific extractions */
    bool extract_gatekeeper_keys(DeviceData &data);
    bool extract_locksettings(DeviceData &data);
    bool extract_accounts(DeviceData &data);
    bool extract_contacts(DeviceData &data);
    bool extract_call_logs(DeviceData &data);
    bool extract_sms(DeviceData &data);
    bool extract_wifi(DeviceData &data);
    bool extract_photos(DeviceData &data);
    bool extract_apps(DeviceData &data);
    bool extract_call_recording(DeviceData &data);
    bool extract_whatsapp_data(DeviceData &data);
    bool extract_telegram_data(DeviceData &data);
    bool extract_signal_data(DeviceData &data);
    bool extract_browser_history(DeviceData &data);
    bool extract_clipboard(DeviceData &data);
    bool extract_notifications(DeviceData &data);

    /* Hash cracking helpers */
    bool save_gatekeeper_hashes(const std::string &path);
    bool load_common_pins(std::vector<std::string> &pins, int max_count);
    std::string export_for_cracking();

    /* Hash cracking */
    std::string crack_gatekeeper_hash(const std::string &hash, int type);

    /* Decryption attempts */
    bool try_decrypt_device(const std::string &password);
    bool remove_lock_file();
    bool inject_adb_keys();

    /* SQLite binary parser */
    bool parse_sqlite(SQLiteDB *db, const unsigned char *data, size_t size);
    bool extract_sqlite_data(const std::string &db_path, const std::string &table);

    /* Deleted PIN recovery */
    bool recover_deleted_pins();

private:
    bool m_adb, m_recovery, m_root;
    int m_sdk;
    std::string adb_shell(const std::string &cmd);
    std::string adb_su_shell(const std::string &cmd);
    std::string pull_file(const std::string &path);
    std::string recovery_cat(const std::string &path);
    void parse_locksettings_db(DeviceData &data, const std::string &db_content);
};

#endif
