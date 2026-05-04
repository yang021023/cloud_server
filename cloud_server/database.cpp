#include "database.h"
#include "logger.h"
#include "ai_analyzer.h"
#include "qwen_client.h"

#include <ctime>
#include "sqlite3.h"

namespace {
    sqlite3* DB(void* db) { return static_cast<sqlite3*>(db); }
    long long NowTs() { return static_cast<long long>(std::time(nullptr)); }
    std::string Text(sqlite3_stmt* stmt, int index) {
        const auto* text = sqlite3_column_text(stmt, index);
        return text ? reinterpret_cast<const char*>(text) : "";
    }
}

Database::Database(const std::string& db_path) : db_path_(db_path) {}
Database::~Database() { Close(); }

bool Database::Open() {
    sqlite3* raw = nullptr;
    if (sqlite3_open(db_path_.c_str(), &raw) != SQLITE_OK) {
        Logger::Error("[CloudServer] Failed to open database.");
        return false;
    }
    db_ = raw;
    return true;
}

void Database::Close() {
    if (!db_) return;
    sqlite3_close(DB(db_));
    db_ = nullptr;
}

bool Database::Execute(const std::string& sql) {
    char* error = nullptr;
    const int rc = sqlite3_exec(DB(db_), sql.c_str(), nullptr, nullptr, &error);
    if (rc != SQLITE_OK) {
        if (error) {
            Logger::Error(std::string("[CloudServer] SQL error: ") + error);
            sqlite3_free(error);
        }
        return false;
    }
    return true;
}

bool Database::Initialize() {
    return Execute(
        "CREATE TABLE IF NOT EXISTS sensor_data(id INTEGER PRIMARY KEY AUTOINCREMENT,device_id TEXT NOT NULL,temperature REAL NOT NULL,humidity REAL NOT NULL,light REAL NOT NULL,timestamp INTEGER NOT NULL,UNIQUE(device_id,timestamp));"
        "CREATE TABLE IF NOT EXISTS ai_analysis(id INTEGER PRIMARY KEY AUTOINCREMENT,device_id TEXT NOT NULL,timestamp INTEGER NOT NULL,is_abnormal INTEGER NOT NULL,reason TEXT,suggestion TEXT);"
        "CREATE TABLE IF NOT EXISTS users(id INTEGER PRIMARY KEY AUTOINCREMENT,username TEXT NOT NULL UNIQUE,password TEXT NOT NULL,role TEXT NOT NULL DEFAULT 'viewer',created_at INTEGER NOT NULL,last_login INTEGER);"
        "CREATE TABLE IF NOT EXISTS user_device_permissions(id INTEGER PRIMARY KEY AUTOINCREMENT,username TEXT NOT NULL,device_id TEXT NOT NULL,UNIQUE(username,device_id));"
        "CREATE TABLE IF NOT EXISTS device_info(id INTEGER PRIMARY KEY AUTOINCREMENT,device_id TEXT NOT NULL UNIQUE,device_name TEXT,location TEXT,registered_at INTEGER NOT NULL,description TEXT);"
        "CREATE TABLE IF NOT EXISTS alerts(id INTEGER PRIMARY KEY AUTOINCREMENT,device_id TEXT NOT NULL,timestamp INTEGER NOT NULL,alert_type TEXT NOT NULL,value REAL,threshold REAL,message TEXT,resolved INTEGER DEFAULT 0);"
        "CREATE TABLE IF NOT EXISTS rule_logs(id INTEGER PRIMARY KEY AUTOINCREMENT,device_id TEXT NOT NULL,timestamp INTEGER NOT NULL,rule_name TEXT NOT NULL,trigger_value REAL,action TEXT);"
        "CREATE TABLE IF NOT EXISTS control_commands(id INTEGER PRIMARY KEY AUTOINCREMENT,device_id TEXT NOT NULL,timestamp INTEGER NOT NULL,crop_name TEXT NOT NULL,command TEXT NOT NULL,reason TEXT,status TEXT NOT NULL);"
        "CREATE TABLE IF NOT EXISTS device_crop_selection(device_id TEXT PRIMARY KEY,crop_name TEXT NOT NULL,source TEXT NOT NULL,updated_at INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS crop_profiles(crop_name TEXT PRIMARY KEY,min_temperature REAL NOT NULL,max_temperature REAL NOT NULL,min_humidity REAL NOT NULL,max_humidity REAL NOT NULL,min_light REAL NOT NULL,max_light REAL NOT NULL,updated_at INTEGER NOT NULL);"
    );
}

bool Database::SeedDefaultData() {
    const long long now = NowTs();
    return Execute(
        "INSERT OR IGNORE INTO users(username,password,role,created_at,last_login) VALUES('admin','240be518fabd2724ddb6f04eeb1da5967448d7e831c08c8fa822809f74c720a9','admin'," + std::to_string(now) + "," + std::to_string(now) + ");"
        "UPDATE users SET password='240be518fabd2724ddb6f04eeb1da5967448d7e831c08c8fa822809f74c720a9', role='admin' WHERE username='admin';"
        "INSERT OR IGNORE INTO device_info(device_id,device_name,location,registered_at,description) VALUES"
        "('device_001','Sensor A1','Zone A'," + std::to_string(now) + ",'default device'),"
        "('device_002','Sensor B1','Zone B'," + std::to_string(now) + ",'default device'),"
        "('device_003','Sensor C1','Zone C'," + std::to_string(now) + ",'default device');"
        "INSERT OR IGNORE INTO user_device_permissions(username,device_id) VALUES"
        "('admin','device_001'),('admin','device_002'),('admin','device_003');"
        "DELETE FROM device_crop_selection WHERE source='seed';"
        "INSERT OR IGNORE INTO crop_profiles(crop_name,min_temperature,max_temperature,min_humidity,max_humidity,min_light,max_light,updated_at) VALUES"
        "('default',18,25,40,70,200,900," + std::to_string(now) + "),"
        "('tomato',20,28,55,75,300,800," + std::to_string(now) + "),"
        "('cucumber',18,26,60,85,250,700," + std::to_string(now) + "),"
        "('pepper',22,30,50,70,280,850," + std::to_string(now) + ");"
        "INSERT OR IGNORE INTO sensor_data(device_id,temperature,humidity,light,timestamp) VALUES"
        "('device_001',28.5,55.2,620.0," + std::to_string(now - 4) + "),"
        "('device_002',31.2,42.8,380.0," + std::to_string(now - 2) + "),"
        "('device_003',24.1,71.3,890.0," + std::to_string(now) + ");"
        "INSERT OR IGNORE INTO ai_analysis(device_id,timestamp,is_abnormal,reason,suggestion) VALUES"
        "('device_001'," + std::to_string(now - 4) + ",0,'NORMAL','KEEP_CURRENT_ENVIRONMENT'),"
        "('device_002'," + std::to_string(now - 2) + ",1,'HIGH_TEMPERATURE','TURN_ON_VENTILATION');"
        "INSERT OR IGNORE INTO alerts(device_id,timestamp,alert_type,value,threshold,message,resolved) VALUES"
        "('device_002'," + std::to_string(now - 2) + ",'temperature_high',31.2,30.0,'HIGH_TEMPERATURE_ALERT',0);"
        "INSERT OR IGNORE INTO rule_logs(device_id,timestamp,rule_name,trigger_value,action) VALUES"
        "('device_002'," + std::to_string(now - 2) + ",'fan_on',31.2,'TURN_ON_FAN');"
        "INSERT OR IGNORE INTO control_commands(device_id,timestamp,crop_name,command,reason,status) VALUES"
        "('device_002'," + std::to_string(now - 2) + ",'default','TURN_ON_FAN','HIGH_TEMPERATURE','pending');"
    );
}

bool Database::ValidateUser(const std::string& username, const std::string& password_hash, std::string& role) {
    const char* sql = "SELECT role FROM users WHERE username=? AND password=? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(DB(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, password_hash.c_str(), -1, SQLITE_TRANSIENT);
    const int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) role = Text(stmt, 0);
    sqlite3_finalize(stmt);
    return rc == SQLITE_ROW;
}

bool Database::UpdateLastLogin(const std::string& username) {
    const char* sql = "UPDATE users SET last_login=? WHERE username=?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(DB(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int64(stmt, 1, NowTs());
    sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::CreateUser(const std::string& username, const std::string& password_hash, const std::string& role) {
    const char* sql = "INSERT INTO users(username,password,role,created_at) VALUES(?,?,?,?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(DB(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, password_hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, role.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, NowTs());
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

std::string Database::GetUserRole(const std::string& username) const {
    const char* sql = "SELECT role FROM users WHERE username=? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    std::string role;
    if (sqlite3_prepare_v2(DB(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) return role;
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        role = Text(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return role;
}

std::vector<UserRecord> Database::GetUsers() const {
    std::vector<UserRecord> out;
    const char* sql = "SELECT username,role,COALESCE(last_login,0) FROM users ORDER BY username;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(DB(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) return out;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        out.push_back({ Text(stmt, 0), Text(stmt, 1), sqlite3_column_int64(stmt, 2) });
    }
    sqlite3_finalize(stmt);
    return out;
}

std::vector<std::string> Database::GetAssignedDevices(const std::string& username) const {
    std::vector<std::string> out;
    const char* sql = "SELECT device_id FROM user_device_permissions WHERE username=? ORDER BY device_id;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(DB(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) return out;
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        out.push_back(Text(stmt, 0));
    }
    sqlite3_finalize(stmt);
    return out;
}

bool Database::SetAssignedDevices(const std::string& username, const std::vector<std::string>& device_ids) {
    if (!Execute("DELETE FROM user_device_permissions WHERE username='" + username + "';")) {
        return false;
    }
    for (const auto& device_id : device_ids) {
        if (!Execute("INSERT OR IGNORE INTO user_device_permissions(username,device_id) VALUES('" + username + "','" + device_id + "');")) {
            return false;
        }
    }
    return true;
}

void Database::SetCurrentCrop(const std::string& device_id, const std::string& crop_name, const std::string& source) {
    const std::string safe_crop = crop_name.empty() ? "default" : crop_name;
    const std::string safe_source = source.empty() ? "frontend" : source;
    Execute(
        "INSERT INTO device_crop_selection(device_id,crop_name,source,updated_at) VALUES('" + device_id + "','" + safe_crop + "','" + safe_source + "'," + std::to_string(NowTs()) + ") "
        "ON CONFLICT(device_id) DO UPDATE SET crop_name=excluded.crop_name,source=excluded.source,updated_at=excluded.updated_at;"
    );
}

CropSelectionRecord Database::GetCurrentCrop(const std::string& device_id) const {
    CropSelectionRecord out{ device_id, "default", "default", 0 };
    const char* sql = "SELECT device_id,crop_name,source,updated_at FROM device_crop_selection WHERE device_id=? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(DB(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) return out;
    sqlite3_bind_text(stmt, 1, device_id.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        out.device_id = Text(stmt, 0);
        out.crop_name = Text(stmt, 1);
        out.source = Text(stmt, 2);
        out.updated_at = sqlite3_column_int64(stmt, 3);
    }
    sqlite3_finalize(stmt);
    return out;
}

std::vector<CropSelectionRecord> Database::GetAllCurrentCrops() const {
    std::vector<CropSelectionRecord> out;
    const char* sql = "SELECT device_id,crop_name,source,updated_at FROM device_crop_selection ORDER BY device_id;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(DB(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) return out;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        out.push_back({ Text(stmt,0), Text(stmt,1), Text(stmt,2), sqlite3_column_int64(stmt,3) });
    }
    sqlite3_finalize(stmt);
    return out;
}

bool Database::UpsertCropProfile(const CropProfileRecord& profile) {
    const char* sql = "INSERT INTO crop_profiles(crop_name,min_temperature,max_temperature,min_humidity,max_humidity,min_light,max_light,updated_at) VALUES(?,?,?,?,?,?,?,?) ON CONFLICT(crop_name) DO UPDATE SET min_temperature=excluded.min_temperature,max_temperature=excluded.max_temperature,min_humidity=excluded.min_humidity,max_humidity=excluded.max_humidity,min_light=excluded.min_light,max_light=excluded.max_light,updated_at=excluded.updated_at;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(DB(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, profile.crop_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 2, profile.min_temperature);
    sqlite3_bind_double(stmt, 3, profile.max_temperature);
    sqlite3_bind_double(stmt, 4, profile.min_humidity);
    sqlite3_bind_double(stmt, 5, profile.max_humidity);
    sqlite3_bind_double(stmt, 6, profile.min_light);
    sqlite3_bind_double(stmt, 7, profile.max_light);
    sqlite3_bind_int64(stmt, 8, NowTs());
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

CropProfileRecord Database::GetCropProfile(const std::string& crop_name) const {
    CropProfileRecord out;
    out.crop_name = crop_name.empty() ? "default" : crop_name;
    const char* sql = "SELECT crop_name,min_temperature,max_temperature,min_humidity,max_humidity,min_light,max_light FROM crop_profiles WHERE crop_name=? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(DB(db_), sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, out.crop_name.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            out.crop_name = Text(stmt, 0);
            out.min_temperature = sqlite3_column_double(stmt, 1);
            out.max_temperature = sqlite3_column_double(stmt, 2);
            out.min_humidity = sqlite3_column_double(stmt, 3);
            out.max_humidity = sqlite3_column_double(stmt, 4);
            out.min_light = sqlite3_column_double(stmt, 5);
            out.max_light = sqlite3_column_double(stmt, 6);
            sqlite3_finalize(stmt);
            return out;
        }
        sqlite3_finalize(stmt);
    }

    if (out.crop_name != "default") {
        return GetCropProfile("default");
    }
    return out;
}

std::vector<CropProfileRecord> Database::GetAllCropProfiles() const {
    std::vector<CropProfileRecord> out;
    const char* sql = "SELECT crop_name,min_temperature,max_temperature,min_humidity,max_humidity,min_light,max_light FROM crop_profiles ORDER BY crop_name;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(DB(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) return out;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        out.push_back({
            Text(stmt, 0),
            sqlite3_column_double(stmt, 1),
            sqlite3_column_double(stmt, 2),
            sqlite3_column_double(stmt, 3),
            sqlite3_column_double(stmt, 4),
            sqlite3_column_double(stmt, 5),
            sqlite3_column_double(stmt, 6)
        });
    }
    sqlite3_finalize(stmt);
    return out;
}

bool Database::IngestSensorData(const SensorDataRecord& data) {
    const char* sensor_sql = "INSERT OR REPLACE INTO sensor_data(device_id,temperature,humidity,light,timestamp) VALUES(?,?,?,?,?);";
    sqlite3_stmt* sensor_stmt = nullptr;
    if (sqlite3_prepare_v2(DB(db_), sensor_sql, -1, &sensor_stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(sensor_stmt, 1, data.device_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(sensor_stmt, 2, data.temperature);
    sqlite3_bind_double(sensor_stmt, 3, data.humidity);
    sqlite3_bind_double(sensor_stmt, 4, data.light);
    sqlite3_bind_int64(sensor_stmt, 5, data.timestamp);
    const bool sensor_ok = sqlite3_step(sensor_stmt) == SQLITE_DONE;
    sqlite3_finalize(sensor_stmt);
    if (!sensor_ok) return false;

    const long long registered_at = data.timestamp > 0 ? data.timestamp : NowTs();
    const std::string suffix = data.device_id.size() >= 3 ? data.device_id.substr(data.device_id.size() - 3) : data.device_id;
    const std::string location = data.location.empty() ? "Auto Zone" : data.location;
    if (!Execute(
        "INSERT OR IGNORE INTO device_info(device_id,device_name,location,registered_at,description) VALUES('" + data.device_id + "','Sensor " + suffix + "','" + location + "'," + std::to_string(registered_at) + ",'auto-registered by edge');"
    )) {
        return false;
    }
    if (!Execute(
        "UPDATE device_info SET location='" + location + "' WHERE device_id='" + data.device_id + "';"
    )) {
        return false;
    }

    AiAnalyzer analyzer;
    const CropSelectionRecord crop_selection = GetCurrentCrop(data.device_id);
    const std::string crop_name = crop_selection.crop_name.empty() ? "default" : crop_selection.crop_name;
    const AiResult result = analyzer.AnalyzeWithDevice(data.device_id, crop_name, data.temperature, data.humidity, data.light);

    const char* analysis_sql = "INSERT INTO ai_analysis(device_id,timestamp,is_abnormal,reason,suggestion) VALUES(?,?,?,?,?);";
    sqlite3_stmt* analysis_stmt = nullptr;
    if (sqlite3_prepare_v2(DB(db_), analysis_sql, -1, &analysis_stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(analysis_stmt, 1, data.device_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(analysis_stmt, 2, data.timestamp);
    sqlite3_bind_int(analysis_stmt, 3, result.is_abnormal ? 1 : 0);
    sqlite3_bind_text(analysis_stmt, 4, result.reason.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(analysis_stmt, 5, result.suggestion.c_str(), -1, SQLITE_TRANSIENT);
    const bool analysis_ok = sqlite3_step(analysis_stmt) == SQLITE_DONE;
    sqlite3_finalize(analysis_stmt);
    if (!analysis_ok) return false;

    std::string command = "KEEP_STABLE";
    if (result.reason.find("HIGH_TEMPERATURE") != std::string::npos) {
        command = "TURN_ON_FAN";
    } else if (result.reason.find("LOW_HUMIDITY") != std::string::npos) {
        command = "START_IRRIGATION";
    } else if (result.reason.find("LOW_LIGHT") != std::string::npos) {
        command = "TURN_ON_GROW_LIGHT";
    }

    if (!Execute(
        "INSERT INTO control_commands(device_id,timestamp,crop_name,command,reason,status) VALUES('" + data.device_id + "'," + std::to_string(data.timestamp) + ",'" + crop_name + "','" + command + "','" + result.reason + "','pending');"
    )) return false;

    if (data.temperature > 30.0) {
        if (!Execute(
            "INSERT INTO rule_logs(device_id,timestamp,rule_name,trigger_value,action) VALUES('" + data.device_id + "'," + std::to_string(data.timestamp) + ",'fan_on'," + std::to_string(data.temperature) + ",'FAN_ON');"
        )) return false;
    }
    if (data.humidity < 40.0) {
        if (!Execute(
            "INSERT INTO rule_logs(device_id,timestamp,rule_name,trigger_value,action) VALUES('" + data.device_id + "'," + std::to_string(data.timestamp) + ",'irrigation_on'," + std::to_string(data.humidity) + ",'IRRIGATION_ON');"
        )) return false;
    }

    if (result.is_abnormal) {
        if (!Execute(
            "INSERT INTO alerts(device_id,timestamp,alert_type,value,threshold,message,resolved) VALUES('" + data.device_id + "'," + std::to_string(data.timestamp) + ",'" + result.reason + "'," + std::to_string(data.temperature) + ",0,'" + result.suggestion + "',0);"
        )) return false;
    }

    return true;
}

std::vector<SensorDataRecord> Database::GetLatestData() {
    std::vector<SensorDataRecord> out;
    const char* sql = "SELECT s.device_id,s.temperature,s.humidity,s.light,s.timestamp FROM sensor_data s INNER JOIN (SELECT device_id,MAX(timestamp) AS max_ts FROM sensor_data GROUP BY device_id) latest ON s.device_id=latest.device_id AND s.timestamp=latest.max_ts ORDER BY s.device_id;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(DB(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) return out;
    while (sqlite3_step(stmt) == SQLITE_ROW) out.push_back({ Text(stmt,0), "", sqlite3_column_double(stmt,1), sqlite3_column_double(stmt,2), sqlite3_column_double(stmt,3), sqlite3_column_int64(stmt,4) });
    sqlite3_finalize(stmt);
    return out;
}

std::vector<SensorDataRecord> Database::GetHistoryData(const std::string& device_id, int limit) {
    std::vector<SensorDataRecord> out;
    const char* sql = "SELECT device_id,temperature,humidity,light,timestamp FROM sensor_data WHERE device_id=? ORDER BY timestamp DESC LIMIT ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(DB(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) return out;
    sqlite3_bind_text(stmt, 1, device_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, limit);
    while (sqlite3_step(stmt) == SQLITE_ROW) out.push_back({ Text(stmt,0), "", sqlite3_column_double(stmt,1), sqlite3_column_double(stmt,2), sqlite3_column_double(stmt,3), sqlite3_column_int64(stmt,4) });
    sqlite3_finalize(stmt);
    return out;
}

std::vector<AnalysisRecord> Database::GetAnalysis(const std::string& device_id) {
    std::vector<AnalysisRecord> out;
    const char* sql = "SELECT device_id,timestamp,is_abnormal,reason,suggestion FROM ai_analysis WHERE device_id=? ORDER BY timestamp DESC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(DB(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) return out;
    sqlite3_bind_text(stmt, 1, device_id.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) out.push_back({ Text(stmt,0),sqlite3_column_int64(stmt,1),sqlite3_column_int(stmt,2) != 0,Text(stmt,3),Text(stmt,4) });
    sqlite3_finalize(stmt);
    return out;
}

std::vector<DeviceRecord> Database::GetDevices() {
    std::vector<DeviceRecord> out;
    const char* sql = "SELECT d.device_id,d.device_name,d.location,COALESCE((SELECT MAX(timestamp) FROM sensor_data s WHERE s.device_id=d.device_id),0) AS last_seen FROM device_info d ORDER BY d.device_id;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(DB(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) return out;
    const long long now = NowTs();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        DeviceRecord item{ Text(stmt,0),Text(stmt,1),Text(stmt,2),false,sqlite3_column_int64(stmt,3) };
        item.online = item.last_seen > 0 && (now - item.last_seen) <= 10;
        out.push_back(item);
    }
    sqlite3_finalize(stmt);
    return out;
}

std::vector<AlertRecord> Database::GetAlerts(const std::string& device_id) {
    std::vector<AlertRecord> out;
    const char* sql = device_id.empty() ? "SELECT id,device_id,timestamp,alert_type,value,message,resolved FROM alerts ORDER BY timestamp DESC;" : "SELECT id,device_id,timestamp,alert_type,value,message,resolved FROM alerts WHERE device_id=? ORDER BY timestamp DESC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(DB(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) return out;
    if (!device_id.empty()) sqlite3_bind_text(stmt, 1, device_id.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) out.push_back({ sqlite3_column_int(stmt,0),Text(stmt,1),sqlite3_column_int64(stmt,2),Text(stmt,3),sqlite3_column_double(stmt,4),Text(stmt,5),sqlite3_column_int(stmt,6) });
    sqlite3_finalize(stmt);
    return out;
}

std::vector<RuleLogRecord> Database::GetRuleLogs(const std::string& device_id) {
    std::vector<RuleLogRecord> out;
    const char* sql = device_id.empty() ? "SELECT device_id,timestamp,rule_name,trigger_value,action FROM rule_logs ORDER BY timestamp DESC;" : "SELECT device_id,timestamp,rule_name,trigger_value,action FROM rule_logs WHERE device_id=? ORDER BY timestamp DESC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(DB(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) return out;
    if (!device_id.empty()) sqlite3_bind_text(stmt, 1, device_id.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) out.push_back({ Text(stmt,0),sqlite3_column_int64(stmt,1),Text(stmt,2),sqlite3_column_double(stmt,3),Text(stmt,4) });
    sqlite3_finalize(stmt);
    return out;
}

std::vector<ControlCommandRecord> Database::GetControlCommands(const std::string& device_id, const std::string& status) {
    std::vector<ControlCommandRecord> out;
    const bool has_device = !device_id.empty();
    const bool has_status = !status.empty();

    const char* sql = nullptr;
    if (has_device && has_status) {
        sql = "SELECT id,device_id,timestamp,crop_name,command,reason,status FROM control_commands WHERE device_id=? AND status=? ORDER BY timestamp DESC;";
    } else if (has_device) {
        sql = "SELECT id,device_id,timestamp,crop_name,command,reason,status FROM control_commands WHERE device_id=? ORDER BY timestamp DESC;";
    } else if (has_status) {
        sql = "SELECT id,device_id,timestamp,crop_name,command,reason,status FROM control_commands WHERE status=? ORDER BY timestamp DESC;";
    } else {
        sql = "SELECT id,device_id,timestamp,crop_name,command,reason,status FROM control_commands ORDER BY timestamp DESC;";
    }

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(DB(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) return out;
    int bind_index = 1;
    if (has_device) sqlite3_bind_text(stmt, bind_index++, device_id.c_str(), -1, SQLITE_TRANSIENT);
    if (has_status) sqlite3_bind_text(stmt, bind_index++, status.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) out.push_back({ sqlite3_column_int(stmt,0),Text(stmt,1),sqlite3_column_int64(stmt,2),Text(stmt,3),Text(stmt,4),Text(stmt,5),Text(stmt,6) });
    sqlite3_finalize(stmt);
    return out;
}

bool Database::UpdateControlCommandStatus(int id, const std::string& status) {
    const char* sql = "UPDATE control_commands SET status=? WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(DB(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, id);
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::ResolveAlert(int id) {
    const char* sql = "UPDATE alerts SET resolved=1 WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(DB(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt, 1, id);
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::ResolveLatestAlertByDevice(const std::string& device_id) {
    const char* sql = "UPDATE alerts SET resolved=1 WHERE id=(SELECT id FROM alerts WHERE device_id=? AND resolved=0 ORDER BY timestamp DESC LIMIT 1);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(DB(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, device_id.c_str(), -1, SQLITE_TRANSIENT);
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}
