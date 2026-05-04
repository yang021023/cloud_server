#include "rest_server.h"
#include "sqlite3.h"
#include "auth.h"
#include "logger.h"
#include "qwen_client.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#define NOMINMAX
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")

namespace {
long long NowTs() {
    return static_cast<long long>(std::time(nullptr));
}

std::string JsonEscape(const std::string& input) {
    std::string out;
    for (char ch : input) {
        switch (ch) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out += ch; break;
        }
    }
    return out;
}

std::map<std::string, std::string> ParseQuery(const std::string& path) {
    std::map<std::string, std::string> result;
    const auto pos = path.find('?');
    if (pos == std::string::npos) return result;
    std::stringstream ss(path.substr(pos + 1));
    std::string item;
    while (std::getline(ss, item, '&')) {
        const auto eq = item.find('=');
        if (eq != std::string::npos) result[item.substr(0, eq)] = item.substr(eq + 1);
    }
    return result;
}

std::string PathWithoutQuery(const std::string& path) {
    const auto pos = path.find('?');
    return pos == std::string::npos ? path : path.substr(0, pos);
}

std::string GetBody(const std::string& request) {
    const auto pos = request.find("\r\n\r\n");
    return pos == std::string::npos ? "" : request.substr(pos + 4);
}

std::string ExtractJsonField(const std::string& body, const std::string& key) {
    const std::string quoted_key = "\"" + key + "\"";
    const auto key_pos = body.find(quoted_key);
    if (key_pos == std::string::npos) return "";

    auto colon_pos = body.find(':', key_pos + quoted_key.size());
    if (colon_pos == std::string::npos) return "";
    ++colon_pos;

    while (colon_pos < body.size() && std::isspace(static_cast<unsigned char>(body[colon_pos]))) {
        ++colon_pos;
    }
    if (colon_pos >= body.size() || body[colon_pos] != '"') return "";

    const auto value_start = colon_pos + 1;
    const auto value_end = body.find('"', value_start);
    if (value_end == std::string::npos) return "";
    return body.substr(value_start, value_end - value_start);
}

std::string HttpResponse(const std::string& body, const std::string& status = "200 OK") {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status << "\r\n"
        << "Content-Type: application/json; charset=utf-8\r\n"
        << "Access-Control-Allow-Origin: *\r\n"
        << "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
        << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        << "Content-Length: " << body.size() << "\r\n\r\n"
        << body;
    return oss.str();
}

std::string ExtractHeader(const std::string& request, const std::string& header_name) {
    const std::string pattern = header_name + ":";
    const auto pos = request.find(pattern);
    if (pos == std::string::npos) return "";
    auto value_start = pos + pattern.size();
    while (value_start < request.size() && std::isspace(static_cast<unsigned char>(request[value_start]))) {
        ++value_start;
    }
    const auto value_end = request.find("\r\n", value_start);
    if (value_end == std::string::npos) return "";
    return request.substr(value_start, value_end - value_start);
}

std::string GetRequestUsername(const std::string& request, const AuthService& auth) {
    const std::string authorization = ExtractHeader(request, "Authorization");
    const std::string bearer_prefix = "Bearer ";
    if (authorization.rfind(bearer_prefix, 0) != 0) return "";
    return auth.ParseUsernameFromToken(authorization.substr(bearer_prefix.size()));
}

bool CanAccessDevice(Database& database, const std::string& username, const std::string& device_id) {
    if (username.empty() || device_id.empty()) return false;
    const std::string role = database.GetUserRole(username);
    if (role == "admin") return true;
    const auto assigned = database.GetAssignedDevices(username);
    return std::find(assigned.begin(), assigned.end(), device_id) != assigned.end();
}

std::vector<std::string> GetAccessibleDeviceIds(Database& database, const std::string& username) {
    if (username.empty()) return {};
    if (database.GetUserRole(username) == "admin") {
        std::vector<std::string> ids;
        for (const auto& device : database.GetDevices()) {
            ids.push_back(device.device_id);
        }
        return ids;
    }
    return database.GetAssignedDevices(username);
}

std::string SensorsJson(const std::vector<SensorDataRecord>& items) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < items.size(); ++i) {
        if (i) oss << ",";
        oss << "{\"device_id\":\"" << JsonEscape(items[i].device_id) << "\",\"temperature\":" << items[i].temperature
            << ",\"humidity\":" << items[i].humidity << ",\"light\":" << items[i].light << ",\"timestamp\":" << items[i].timestamp << "}";
    }
    oss << "]";
    return oss.str();
}

std::string AnalysisJson(const std::vector<AnalysisRecord>& items) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < items.size(); ++i) {
        if (i) oss << ",";
        oss << "{\"device_id\":\"" << JsonEscape(items[i].device_id) << "\",\"timestamp\":" << items[i].timestamp
            << ",\"is_abnormal\":" << (items[i].is_abnormal ? "true" : "false") << ",\"reason\":\"" << JsonEscape(items[i].reason)
            << "\",\"suggestion\":\"" << JsonEscape(items[i].suggestion) << "\"}";
    }
    oss << "]";
    return oss.str();
}

std::string DevicesJson(const std::vector<DeviceRecord>& items) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < items.size(); ++i) {
        if (i) oss << ",";
        oss << "{\"device_id\":\"" << JsonEscape(items[i].device_id) << "\",\"device_name\":\"" << JsonEscape(items[i].device_name)
            << "\",\"location\":\"" << JsonEscape(items[i].location) << "\",\"online\":" << (items[i].online ? "true" : "false")
            << ",\"last_seen\":" << items[i].last_seen << "}";
    }
    oss << "]";
    return oss.str();
}

std::string AlertsJson(const std::vector<AlertRecord>& items) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < items.size(); ++i) {
        if (i) oss << ",";
        oss << "{\"id\":" << items[i].id << ",\"device_id\":\"" << JsonEscape(items[i].device_id) << "\",\"timestamp\":" << items[i].timestamp
            << ",\"alert_type\":\"" << JsonEscape(items[i].alert_type) << "\",\"value\":" << items[i].value
            << ",\"message\":\"" << JsonEscape(items[i].message) << "\",\"resolved\":" << items[i].resolved << "}";
    }
    oss << "]";
    return oss.str();
}

std::string RuleLogsJson(const std::vector<RuleLogRecord>& items) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < items.size(); ++i) {
        if (i) oss << ",";
        oss << "{\"device_id\":\"" << JsonEscape(items[i].device_id) << "\",\"timestamp\":" << items[i].timestamp
            << ",\"rule_name\":\"" << JsonEscape(items[i].rule_name) << "\",\"trigger_value\":" << items[i].trigger_value
            << ",\"action\":\"" << JsonEscape(items[i].action) << "\"}";
    }
    oss << "]";
    return oss.str();
}

std::string CropProfilesJson(const std::vector<CropProfileRecord>& items) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < items.size(); ++i) {
        if (i) oss << ",";
        oss << "{\"crop_name\":\"" << JsonEscape(items[i].crop_name) << "\""
            << ",\"min_temperature\":" << items[i].min_temperature
            << ",\"max_temperature\":" << items[i].max_temperature
            << ",\"min_humidity\":" << items[i].min_humidity
            << ",\"max_humidity\":" << items[i].max_humidity
            << ",\"min_light\":" << items[i].min_light
            << ",\"max_light\":" << items[i].max_light << "}";
    }
    oss << "]";
    return oss.str();
}
}

RestServer::RestServer(Database& database) : database_(database) {}

bool RestServer::Start(int port) {
    WSADATA wsa_data{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        Logger::Error("[CloudServer] WSAStartup failed.");
        return false;
    }

    SOCKET listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket == INVALID_SOCKET) {
        Logger::Error("[CloudServer] Failed to create HTTP socket.");
        WSACleanup();
        return false;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(static_cast<u_short>(port));

    if (bind(listen_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == SOCKET_ERROR) {
        Logger::Error("[CloudServer] HTTP bind failed.");
        closesocket(listen_socket);
        WSACleanup();
        return false;
    }

    if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR) {
        Logger::Error("[CloudServer] HTTP listen failed.");
        closesocket(listen_socket);
        WSACleanup();
        return false;
    }

    Logger::Info("[CloudServer] HTTP REST API listening on 0.0.0.0:" + std::to_string(port));
    AuthService auth;

    while (true) {
        SOCKET client_socket = accept(listen_socket, nullptr, nullptr);
        if (client_socket == INVALID_SOCKET) continue;

        char buffer[8192] = {0};
        const int received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (received <= 0) {
            closesocket(client_socket);
            continue;
        }

        std::string request(buffer, received);
        std::istringstream request_stream(request);
        std::string method, path, version;
        request_stream >> method >> path >> version;

        const std::string body = GetBody(request);
        const std::string clean_path = PathWithoutQuery(path);
        const auto query = ParseQuery(path);
        const std::string request_username = GetRequestUsername(request, auth);
        std::string response_body = "[]";
        std::string response_status = "200 OK";

        if (method == "OPTIONS") {
            response_body = "{}";
        } else if (method == "POST" && clean_path == "/api/login") {
            const std::string username = ExtractJsonField(body, "username");
            const std::string password = ExtractJsonField(body, "password");
            std::string role;
            if (database_.ValidateUser(username, auth.HashPassword(password), role)) {
                database_.UpdateLastLogin(username);
                response_body = "{\"token\":\"" + JsonEscape(auth.GenerateToken(username)) + "\",\"role\":\"" + JsonEscape(role) + "\"}";
            } else {
                response_body = "{\"token\":\"\",\"role\":\"\"}";
                response_status = "401 Unauthorized";
            }
        } else if (method == "POST" && clean_path == "/api/register") {
            const std::string username = ExtractJsonField(body, "username");
            const std::string password = ExtractJsonField(body, "password");
            if (!username.empty() && !password.empty() && database_.CreateUser(username, auth.HashPassword(password), "viewer")) {
                response_body = "{\"success\":true}";
            } else {
                response_body = "{\"success\":false}";
                response_status = "400 Bad Request";
            }
        } else if (method == "GET" && clean_path == "/api/users") {
            std::ostringstream oss;
            const auto users = database_.GetUsers();
            oss << "[";
            for (size_t i = 0; i < users.size(); ++i) {
                if (i) oss << ",";
                oss << "{\"username\":\"" << JsonEscape(users[i].username)
                    << "\",\"role\":\"" << JsonEscape(users[i].role)
                    << "\",\"last_login\":" << users[i].last_login << "}";
            }
            oss << "]";
            response_body = oss.str();
        } else if (method == "GET" && clean_path == "/api/user_devices") {
            const std::string username = query.count("username") ? query.at("username") : "";
            const auto device_ids = database_.GetAssignedDevices(username);
            std::ostringstream oss;
            oss << "[";
            for (size_t i = 0; i < device_ids.size(); ++i) {
                if (i) oss << ",";
                oss << "\"" << JsonEscape(device_ids[i]) << "\"";
            }
            oss << "]";
            response_body = oss.str();
        } else if (method == "POST" && clean_path == "/api/user_devices") {
            const std::string username = ExtractJsonField(body, "username");
            const std::string device_ids_csv = ExtractJsonField(body, "device_ids");
            std::vector<std::string> device_ids;
            std::stringstream ss(device_ids_csv);
            std::string item;
            while (std::getline(ss, item, ',')) {
                if (!item.empty()) device_ids.push_back(item);
            }
            if (!username.empty() && database_.SetAssignedDevices(username, device_ids)) {
                response_body = "{\"success\":true}";
            } else {
                response_body = "{\"success\":false}";
                response_status = "400 Bad Request";
            }
        } else if (method == "POST" && clean_path == "/api/ingest") {
            SensorDataRecord data;
            data.device_id = ExtractJsonField(body, "device_id");
            data.location = ExtractJsonField(body, "location");
            const std::string temperature = ExtractJsonField(body, "temperature");
            const std::string humidity = ExtractJsonField(body, "humidity");
            const std::string light = ExtractJsonField(body, "light");
            const std::string timestamp = ExtractJsonField(body, "timestamp");
            try {
                data.temperature = std::stod(temperature);
                data.humidity = std::stod(humidity);
                data.light = std::stod(light);
                data.timestamp = std::stoll(timestamp);
            } catch (...) {
                data.device_id.clear();
            }
            if (!data.device_id.empty() && database_.IngestSensorData(data)) {
                response_body = "{\"success\":true}";
            } else {
                response_body = "{\"success\":false}";
                response_status = "400 Bad Request";
            }
        } else if (method == "POST" && clean_path == "/api/crop/validate") {
            const std::string crop_name = ExtractJsonField(body, "crop_name");
            if (!crop_name.empty()) {
                QwenClient client;
                const CropValidationResult result = client.ValidateCrop(crop_name);
                if (result.success) {
                    response_body = "{\"success\":" + std::string(result.success ? "true" : "false") +
                                    ",\"exists\":" + std::string(result.exists ? "true" : "false") +
                                    ",\"crop_name\":\"" + JsonEscape(result.crop_name.empty() ? crop_name : result.crop_name) + "\"" +
                                    ",\"message\":\"" + JsonEscape(result.message) + "\"" +
                                    ",\"min_temperature\":" + std::to_string(result.min_temperature) +
                                    ",\"max_temperature\":" + std::to_string(result.max_temperature) +
                                    ",\"min_humidity\":" + std::to_string(result.min_humidity) +
                                    ",\"max_humidity\":" + std::to_string(result.max_humidity) +
                                    ",\"min_light\":" + std::to_string(result.min_light) +
                                    ",\"max_light\":" + std::to_string(result.max_light) + "}";
                } else {
                    response_body = "{\"success\":false,\"exists\":false,\"crop_name\":\"" + JsonEscape(crop_name) + "\",\"message\":\"作物校验失败，请稍后重试\"}";
                    response_status = "502 Bad Gateway";
                }
            } else {
                response_body = "{\"success\":false,\"exists\":false,\"crop_name\":\"\",\"message\":\"crop_name 不能为空\"}";
                response_status = "400 Bad Request";
            }
        } else if (method == "POST" && clean_path == "/api/crop") {
            const std::string device_id = ExtractJsonField(body, "device_id");
            const std::string crop_name = ExtractJsonField(body, "crop_name");
            if (!device_id.empty() && !crop_name.empty()) {
                database_.SetCurrentCrop(device_id, crop_name, "frontend");
                CropProfileRecord profile;
                profile.crop_name = crop_name;
                const std::string min_temperature = ExtractJsonField(body, "min_temperature");
                const std::string max_temperature = ExtractJsonField(body, "max_temperature");
                const std::string min_humidity = ExtractJsonField(body, "min_humidity");
                const std::string max_humidity = ExtractJsonField(body, "max_humidity");
                const std::string min_light = ExtractJsonField(body, "min_light");
                const std::string max_light = ExtractJsonField(body, "max_light");
                if (!min_temperature.empty() && !max_temperature.empty() && !min_humidity.empty() && !max_humidity.empty() && !min_light.empty() && !max_light.empty()) {
                    profile.min_temperature = std::stod(min_temperature);
                    profile.max_temperature = std::stod(max_temperature);
                    profile.min_humidity = std::stod(min_humidity);
                    profile.max_humidity = std::stod(max_humidity);
                    profile.min_light = std::stod(min_light);
                    profile.max_light = std::stod(max_light);
                    database_.UpsertCropProfile(profile);
                }
                response_body = "{\"success\":true,\"device_id\":\"" + JsonEscape(device_id) + "\",\"crop_name\":\"" + JsonEscape(crop_name) + "\"}";
            } else {
                response_body = "{\"success\":false}";
                response_status = "400 Bad Request";
            }
        } else if (method == "GET" && clean_path == "/api/crop_profile") {
            const std::string crop_name = query.count("crop_name") ? query.at("crop_name") : "default";
            const auto profile = database_.GetCropProfile(crop_name);
            response_body = "{\"crop_name\":\"" + JsonEscape(profile.crop_name) + "\"" +
                            ",\"min_temperature\":" + std::to_string(profile.min_temperature) +
                            ",\"max_temperature\":" + std::to_string(profile.max_temperature) +
                            ",\"min_humidity\":" + std::to_string(profile.min_humidity) +
                            ",\"max_humidity\":" + std::to_string(profile.max_humidity) +
                            ",\"min_light\":" + std::to_string(profile.min_light) +
                            ",\"max_light\":" + std::to_string(profile.max_light) + "}";
        } else if (method == "GET" && clean_path == "/api/crop_profiles") {
            response_body = CropProfilesJson(database_.GetAllCropProfiles());
        } else if (method == "GET" && clean_path == "/api/crop") {
            const std::string device_id = query.count("device_id") ? query.at("device_id") : "";
            if (!device_id.empty()) {
                const auto crop = database_.GetCurrentCrop(device_id);
                response_body = "{\"device_id\":\"" + JsonEscape(crop.device_id) + "\",\"crop_name\":\"" + JsonEscape(crop.crop_name) + "\",\"source\":\"" + JsonEscape(crop.source) + "\",\"updated_at\":" + std::to_string(crop.updated_at) + "}";
            } else {
                const auto items = database_.GetAllCurrentCrops();
                std::ostringstream oss;
                oss << "[";
                for (size_t i = 0; i < items.size(); ++i) {
                    if (i) oss << ",";
                    oss << "{\"device_id\":\"" << JsonEscape(items[i].device_id) << "\",\"crop_name\":\"" << JsonEscape(items[i].crop_name)
                        << "\",\"source\":\"" << JsonEscape(items[i].source) << "\",\"updated_at\":" << items[i].updated_at << "}";
                }
                oss << "]";
                response_body = oss.str();
            }
        } else if (method == "GET" && clean_path == "/api/latest") {
            response_body = SensorsJson(database_.GetLatestData());
        } else if (method == "GET" && clean_path == "/api/history") {
            const std::string device_id = query.count("device_id") ? query.at("device_id") : "device_001";
            const int limit = query.count("limit") ? std::max(1, std::stoi(query.at("limit"))) : 100;
            if (!CanAccessDevice(database_, request_username, device_id)) {
                response_body = "{\"error\":\"forbidden\"}";
                response_status = "403 Forbidden";
            } else {
                response_body = SensorsJson(database_.GetHistoryData(device_id, limit));
            }
        } else if (method == "GET" && clean_path == "/api/analysis") {
            const std::string device_id = query.count("device_id") ? query.at("device_id") : "device_001";
            if (!CanAccessDevice(database_, request_username, device_id)) {
                response_body = "{\"error\":\"forbidden\"}";
                response_status = "403 Forbidden";
            } else {
                response_body = AnalysisJson(database_.GetAnalysis(device_id));
            }
        } else if (method == "GET" && clean_path == "/api/devices") {
            const std::string username = query.count("username") ? query.at("username") : "";
            auto devices = database_.GetDevices();
            if (!username.empty()) {
                const std::string role = database_.GetUserRole(username);
                if (role != "admin") {
                    const auto assigned = database_.GetAssignedDevices(username);
                    devices.erase(std::remove_if(devices.begin(), devices.end(), [&](const DeviceRecord& item) {
                        return std::find(assigned.begin(), assigned.end(), item.device_id) == assigned.end();
                    }), devices.end());
                }
            }
            response_body = DevicesJson(devices);
        } else if (method == "GET" && clean_path == "/api/alerts") {
            const std::string device_id = query.count("device_id") ? query.at("device_id") : "";
            if (!device_id.empty() && !CanAccessDevice(database_, request_username, device_id)) {
                response_body = "{\"error\":\"forbidden\"}";
                response_status = "403 Forbidden";
            } else {
                auto items = database_.GetAlerts(device_id);
                if (device_id.empty()) {
                    const auto accessible_ids = GetAccessibleDeviceIds(database_, request_username);
                    items.erase(std::remove_if(items.begin(), items.end(), [&](const AlertRecord& item) {
                        return std::find(accessible_ids.begin(), accessible_ids.end(), item.device_id) == accessible_ids.end();
                    }), items.end());
                }
                response_body = AlertsJson(items);
            }
        } else if (method == "GET" && clean_path == "/api/rule_logs") {
            const std::string device_id = query.count("device_id") ? query.at("device_id") : "";
            response_body = RuleLogsJson(database_.GetRuleLogs(device_id));
        } else if (method == "GET" && clean_path == "/api/control_commands") {
            const std::string device_id = query.count("device_id") ? query.at("device_id") : "";
            const std::string status = query.count("status") ? query.at("status") : "";
            if (!device_id.empty() && !CanAccessDevice(database_, request_username, device_id)) {
                response_body = "{\"error\":\"forbidden\"}";
                response_status = "403 Forbidden";
            } else {
                auto items = database_.GetControlCommands(device_id, status);
                if (device_id.empty()) {
                    const auto accessible_ids = GetAccessibleDeviceIds(database_, request_username);
                    items.erase(std::remove_if(items.begin(), items.end(), [&](const ControlCommandRecord& item) {
                        return std::find(accessible_ids.begin(), accessible_ids.end(), item.device_id) == accessible_ids.end();
                    }), items.end());
                }
                std::ostringstream oss;
                oss << "[";
                for (size_t i = 0; i < items.size(); ++i) {
                    if (i) oss << ",";
                    oss << "{\"id\":" << items[i].id
                        << ",\"device_id\":\"" << JsonEscape(items[i].device_id) << "\",\"timestamp\":" << items[i].timestamp
                        << ",\"crop_name\":\"" << JsonEscape(items[i].crop_name) << "\",\"command\":\"" << JsonEscape(items[i].command)
                        << "\",\"reason\":\"" << JsonEscape(items[i].reason) << "\",\"status\":\"" << JsonEscape(items[i].status) << "\"}";
                }
                oss << "]";
                response_body = oss.str();
            }
        } else if (method == "POST" && clean_path.rfind("/api/control_commands/", 0) == 0 && clean_path.find("/status") != std::string::npos) {
            const std::string prefix = "/api/control_commands/";
            const std::string suffix = "/status";
            const std::string id_text = clean_path.substr(prefix.size(), clean_path.size() - prefix.size() - suffix.size());
            const int id = std::stoi(id_text);
            const std::string status = ExtractJsonField(body, "status");
            if (!status.empty() && database_.UpdateControlCommandStatus(id, status)) {
                response_body = "{\"success\":true}";
            } else {
                response_body = "{\"success\":false}";
                response_status = "400 Bad Request";
            }
        } else if (method == "POST" && clean_path == "/api/alerts/resolve_latest") {
            const std::string device_id = ExtractJsonField(body, "device_id");
            if (!device_id.empty() && database_.ResolveLatestAlertByDevice(device_id)) {
                response_body = "{\"success\":true}";
            } else {
                response_body = "{\"success\":false}";
                response_status = "400 Bad Request";
            }
        } else if (method == "POST" && clean_path.rfind("/api/alerts/", 0) == 0 && clean_path.find("/resolve") != std::string::npos) {
            const std::string prefix = "/api/alerts/";
            const std::string suffix = "/resolve";
            const std::string id_text = clean_path.substr(prefix.size(), clean_path.size() - prefix.size() - suffix.size());
            const int id = std::stoi(id_text);
            if (database_.ResolveAlert(id)) {
                response_body = "{\"success\":true}";
            } else {
                response_body = "{\"success\":false}";
                response_status = "500 Internal Server Error";
            }
        } else {
            response_body = "{\"error\":\"not found\"}";
            response_status = "404 Not Found";
        }

        const std::string response = HttpResponse(response_body, response_status);
        send(client_socket, response.c_str(), static_cast<int>(response.size()), 0);
        closesocket(client_socket);
    }
}
