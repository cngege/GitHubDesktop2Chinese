#ifndef UTILS_NET_PARSE_HPP
#define UTILS_NET_PARSE_HPP

// 纯字符串解析工具：把代理地址文本解析成 (主机, 端口)。
//
// 单独拆成 header 的原因：
//   1) 不依赖网络/系统 API（不 include httplib/Windows.h），可以被单元测试直接包含；
//   2) 三个平台共用同一份解析逻辑，避免 Windows/macOS/Linux 各自实现出现语义漂移。
//
// 支持的写法：
//   host:port
//   http://host:port
//   http[s]://user:pass@host:port/path
//   http=host:port;https=host:port      (Windows IE/WinHTTP 的按协议格式)
//   [::1]:8080                          (IPv6 字面量)

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace netparse {

inline std::string trim_whitespace(const std::string& s) {
    const size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    const size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

/**
 * @brief 解析单个代理条目
 * @return 主机与端口；无法解析时返回 nullopt（不抛异常）
 */
inline std::optional<std::pair<std::string, int>> parse_proxy_entry(const std::string& entry) {
    std::string item = trim_whitespace(entry);
    if (item.empty()) return {};

    // "https=host:port" 形式的协议前缀
    const size_t eq = item.find('=');
    const size_t first_colon = item.find(':');
    if (eq != std::string::npos && first_colon != std::string::npos && eq < first_colon) {
        item = item.substr(eq + 1);
    }
    // 去掉 scheme://
    const size_t scheme = item.find("://");
    if (scheme != std::string::npos) item = item.substr(scheme + 3);
    // 去掉路径/查询
    const size_t slash = item.find('/');
    if (slash != std::string::npos) item = item.substr(0, slash);
    // 去掉 user:pass@
    const size_t at = item.rfind('@');
    if (at != std::string::npos) item = item.substr(at + 1);
    if (item.empty()) return {};

    std::string host;
    std::string port_str;
    if (item.front() == '[') {                       // IPv6 字面量: [::1]:8080
        const size_t close = item.find(']');
        if (close == std::string::npos) return {};
        host = item.substr(1, close - 1);
        if (close + 1 < item.size() && item[close + 1] == ':') {
            port_str = item.substr(close + 2);
        }
    }
    else {
        const size_t colon = item.rfind(':');
        if (colon == std::string::npos) return {};
        host = item.substr(0, colon);
        port_str = item.substr(colon + 1);
    }
    if (host.empty() || port_str.empty()) return {};

    try {
        size_t consumed = 0;
        const int port = std::stoi(port_str, &consumed);
        if (consumed != port_str.size()) return {};   // 形如 "8080abc" 视为非法
        if (port <= 0 || port > 65535) return {};
        return std::make_pair(host, port);
    }
    catch(const std::exception&) {
        return {};
    }
}

/**
 * @brief 解析代理地址文本
 * @return 主机与端口；无法解析时返回 nullopt（不抛异常）
 *
 * 多条目时优先 https，其次 http，最后退回第一个可解析的条目。
 */
inline std::optional<std::pair<std::string, int>> parse_proxy_address(const std::string& raw) {
    // 拆成按 ';' 分隔的条目
    std::vector<std::string> entries;
    size_t start = 0;
    while (true) {
        const size_t semi = raw.find(';', start);
        entries.push_back(raw.substr(start,
            semi == std::string::npos ? std::string::npos : semi - start));
        if (semi == std::string::npos) break;
        start = semi + 1;
    }

    const char* preferred[] = { "https=", "http=" };
    for (const char* key : preferred) {
        for (const auto& entry : entries) {
            std::string lower = trim_whitespace(entry);
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (lower.rfind(key, 0) != 0) continue;
            if (auto parsed = parse_proxy_entry(entry)) return parsed;
        }
    }
    for (const auto& entry : entries) {
        if (auto parsed = parse_proxy_entry(entry)) return parsed;
    }
    return {};
}

} // namespace netparse

#endif // UTILS_NET_PARSE_HPP
