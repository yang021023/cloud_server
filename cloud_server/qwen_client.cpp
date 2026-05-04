#include "qwen_client.h"

#include "logger.h"

#include <cctype>
#include <sstream>
#include <string>

#define NOMINMAX
#include <Windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

namespace {
std::wstring ToWide(const std::string& text) {
    if (text.empty()) return L"";
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (size <= 0) return L"";
    std::wstring out(static_cast<size_t>(size - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, out.data(), size);
    return out;
}

std::string EscapeJson(const std::string& input) {
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

std::string UnescapeJson(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        const char ch = input[i];
        if (ch == '\\' && i + 1 < input.size()) {
            const char next = input[++i];
            switch (next) {
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case '\\': out += '\\'; break;
            case '"': out += '"'; break;
            default: out += next; break;
            }
        } else {
            out += ch;
        }
    }
    return out;
}

std::string Trim(const std::string& input) {
    size_t start = 0;
    while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start]))) {
        ++start;
    }

    size_t end = input.size();
    while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1]))) {
        --end;
    }

    return input.substr(start, end - start);
}

std::string StripCodeFence(const std::string& input) {
    std::string text = Trim(input);
    if (text.rfind("```", 0) != 0) {
        return text;
    }

    const auto first_newline = text.find('\n');
    if (first_newline == std::string::npos) {
        return text;
    }

    const auto last_fence = text.rfind("```");
    if (last_fence == std::string::npos || last_fence <= first_newline) {
        return text;
    }

    return Trim(text.substr(first_newline + 1, last_fence - first_newline - 1));
}
}

std::string QwenClient::GetApiKey() const {
    return "sk-fa07d7845c2c4cdebf684a23f695319d";
}

std::string QwenClient::BuildRequestBody(const std::string& device_id, const std::string& crop_name, double temperature, double humidity, double light) const {
    std::ostringstream prompt;
    prompt << "You are an intelligent greenhouse environment analysis assistant. "
           << "The current crop is " << crop_name << ". "
           << "If this is a custom or uncommon crop, first infer its commonly suitable environmental ranges for temperature, humidity, and light based on agricultural knowledge. "
           << "Then compare those suitable ranges with the current sensor data and determine whether the environment is abnormal. "
           << "Return strict JSON only with exactly these fields: is_abnormal, reason, suggestion. "
           << "The reason must briefly explain the mismatch between the crop's suitable environment and the current readings. "
           << "The suggestion must be actionable for greenhouse control. "
           << "Device ID: " << device_id << ". "
           << "Current sensor data -> Temperature: " << temperature << " C, "
           << "Humidity: " << humidity << " %, "
           << "Light: " << light << " lux.";

    std::ostringstream body;
    body << "{"
         << "\"model\":\"qwen-turbo\","
         << "\"messages\":["
         << "{\"role\":\"system\",\"content\":\"You are an intelligent greenhouse environment analysis assistant that outputs strict JSON only. For unknown crops, first infer common suitable environmental ranges, then evaluate current sensor data, and finally return JSON only.\"},"
         << "{\"role\":\"user\",\"content\":\"" << EscapeJson(prompt.str()) << "\"}"
         << "]"
         << "}";
    return body.str();
}

std::string QwenClient::BuildCropValidationRequestBody(const std::string& crop_name) const {
    std::ostringstream prompt;
    prompt << "You are an agricultural crop validation assistant. "
           << "Determine whether the name '" << crop_name << "' is a real crop or a commonly recognized cultivated plant in greenhouse or agricultural scenarios. "
           << "Return strict JSON only with exactly these fields: exists, crop_name, message, min_temperature, max_temperature, min_humidity, max_humidity, min_light, max_light. "
           << "If it exists, normalize crop_name to a simple lowercase English name if possible, and infer suitable greenhouse ranges for temperature, humidity, and light. "
           << "If it does not exist or is clearly invalid, set exists to false and explain briefly in message.";

    std::ostringstream body;
    body << "{"
         << "\"model\":\"qwen-turbo\","
         << "\"messages\":["
         << "{\"role\":\"system\",\"content\":\"You are an agricultural crop validator that outputs strict JSON only.\"},"
         << "{\"role\":\"user\",\"content\":\"" << EscapeJson(prompt.str()) << "\"}"
         << "]"
         << "}";
    return body.str();
}

std::string QwenClient::PostJson(const std::string& body, const std::string& api_key) const {
    const std::wstring host = L"dashscope.aliyuncs.com";
    const std::wstring path = L"/compatible-mode/v1/chat/completions";

    HINTERNET session = WinHttpOpen(L"GreenhouseCloud/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        Logger::Error("[AI] qwen WinHttpOpen failed");
        return "";
    }

    HINTERNET connection = WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connection) {
        Logger::Error("[AI] qwen WinHttpConnect failed");
        WinHttpCloseHandle(session);
        return "";
    }

    HINTERNET request = WinHttpOpenRequest(connection, L"POST", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!request) {
        Logger::Error("[AI] qwen WinHttpOpenRequest failed");
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return "";
    }

    const std::wstring headers = L"Content-Type: application/json\r\nAuthorization: Bearer " + ToWide(api_key) + L"\r\n";
    const BOOL sent = WinHttpSendRequest(request, headers.c_str(), static_cast<DWORD>(-1), const_cast<char*>(body.data()), static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0);
    if (!sent) {
        Logger::Error("[AI] qwen WinHttpSendRequest failed");
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return "";
    }

    if (!WinHttpReceiveResponse(request, nullptr)) {
        Logger::Error("[AI] qwen WinHttpReceiveResponse failed");
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return "";
    }

    std::string response;
    DWORD available = 0;
    while (WinHttpQueryDataAvailable(request, &available) && available > 0) {
        std::string chunk(available, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(request, chunk.data(), available, &read) || read == 0) break;
        chunk.resize(read);
        response += chunk;
        available = 0;
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return response;
}

std::string QwenClient::ExtractJsonString(const std::string& body, const std::string& key) const {
    const std::string quoted_key = "\"" + key + "\"";
    const auto key_pos = body.find(quoted_key);
    if (key_pos == std::string::npos) return "";

    auto colon_pos = body.find(':', key_pos + quoted_key.size());
    if (colon_pos == std::string::npos) return "";
    ++colon_pos;
    while (colon_pos < body.size() && std::isspace(static_cast<unsigned char>(body[colon_pos]))) ++colon_pos;
    if (colon_pos >= body.size() || body[colon_pos] != '"') return "";

    std::string out;
    for (size_t i = colon_pos + 1; i < body.size(); ++i) {
        const char ch = body[i];
        if (ch == '"' && body[i - 1] != '\\') break;
        out += ch;
    }
    return out;
}

bool QwenClient::ExtractJsonBool(const std::string& body, const std::string& key, bool default_value) const {
    const std::string quoted_key = "\"" + key + "\"";
    const auto key_pos = body.find(quoted_key);
    if (key_pos == std::string::npos) return default_value;

    auto colon_pos = body.find(':', key_pos + quoted_key.size());
    if (colon_pos == std::string::npos) return default_value;
    ++colon_pos;
    while (colon_pos < body.size() && std::isspace(static_cast<unsigned char>(body[colon_pos]))) ++colon_pos;
    if (body.compare(colon_pos, 4, "true") == 0) return true;
    if (body.compare(colon_pos, 5, "false") == 0) return false;
    return default_value;
}

double QwenClient::ExtractJsonDouble(const std::string& body, const std::string& key, double default_value) const {
    const std::string quoted_key = "\"" + key + "\"";
    const auto key_pos = body.find(quoted_key);
    if (key_pos == std::string::npos) return default_value;

    auto colon_pos = body.find(':', key_pos + quoted_key.size());
    if (colon_pos == std::string::npos) return default_value;
    ++colon_pos;
    while (colon_pos < body.size() && std::isspace(static_cast<unsigned char>(body[colon_pos]))) ++colon_pos;
    const auto value_end = body.find_first_of(",}", colon_pos);
    if (value_end == std::string::npos) return default_value;
    try {
        return std::stod(body.substr(colon_pos, value_end - colon_pos));
    } catch (...) {
        return default_value;
    }
}

QwenAnalysisResult QwenClient::ParseResponse(const std::string& response) const {
    QwenAnalysisResult result;

    const std::string raw_content = ExtractJsonString(response, "content");
    if (raw_content.empty()) {
        return result;
    }

    const std::string unescaped_content = UnescapeJson(raw_content);
    const std::string cleaned_content = StripCodeFence(unescaped_content);
    const auto json_start = cleaned_content.find('{');
    const auto json_end = cleaned_content.rfind('}');
    if (json_start == std::string::npos || json_end == std::string::npos || json_end <= json_start) {
        return result;
    }

    const std::string model_json = cleaned_content.substr(json_start, json_end - json_start + 1);
    result.is_abnormal = ExtractJsonBool(model_json, "is_abnormal", false);
    result.reason = ExtractJsonString(model_json, "reason");
    result.suggestion = ExtractJsonString(model_json, "suggestion");
    result.success = !result.reason.empty() && !result.suggestion.empty();
    return result;
}

CropValidationResult QwenClient::ParseCropValidationResponse(const std::string& response) const {
    CropValidationResult result;

    const std::string raw_content = ExtractJsonString(response, "content");
    if (raw_content.empty()) {
        return result;
    }

    const std::string unescaped_content = UnescapeJson(raw_content);
    const std::string cleaned_content = StripCodeFence(unescaped_content);
    const auto json_start = cleaned_content.find('{');
    const auto json_end = cleaned_content.rfind('}');
    if (json_start == std::string::npos || json_end == std::string::npos || json_end <= json_start) {
        return result;
    }

    const std::string model_json = cleaned_content.substr(json_start, json_end - json_start + 1);
    result.exists = ExtractJsonBool(model_json, "exists", false);
    result.crop_name = ExtractJsonString(model_json, "crop_name");
    result.message = ExtractJsonString(model_json, "message");
    result.min_temperature = ExtractJsonDouble(model_json, "min_temperature", result.min_temperature);
    result.max_temperature = ExtractJsonDouble(model_json, "max_temperature", result.max_temperature);
    result.min_humidity = ExtractJsonDouble(model_json, "min_humidity", result.min_humidity);
    result.max_humidity = ExtractJsonDouble(model_json, "max_humidity", result.max_humidity);
    result.min_light = ExtractJsonDouble(model_json, "min_light", result.min_light);
    result.max_light = ExtractJsonDouble(model_json, "max_light", result.max_light);
    result.success = !result.message.empty();
    return result;
}

QwenAnalysisResult QwenClient::Analyze(const std::string& device_id, const std::string& crop_name, double temperature, double humidity, double light) const {
    const std::string api_key = GetApiKey();
    if (api_key.empty()) {
        return {};
    }

    const std::string body = BuildRequestBody(device_id, crop_name, temperature, humidity, light);
    const std::string response = PostJson(body, api_key);
    if (response.empty()) {
        return {};
    }

    return ParseResponse(response);
}

CropValidationResult QwenClient::ValidateCrop(const std::string& crop_name) const {
    const std::string api_key = GetApiKey();
    if (api_key.empty()) {
        return {};
    }

    const std::string body = BuildCropValidationRequestBody(crop_name);
    const std::string response = PostJson(body, api_key);
    if (response.empty()) {
        return {};
    }

    return ParseCropValidationResponse(response);
}
