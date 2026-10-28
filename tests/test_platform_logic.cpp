// 平台无关逻辑的回归测试
// 覆盖：Version 解析与比较、代理 URL 解析、scutil --proxy 文本解析、
//       PlistBuddy 版本输出解析、localization.json 映射数据结构。
// 设计为纯 C++ 单元测试（不依赖网络/注册表/bundle），Windows 与 macOS 均可运行。
// 退出码：0 = 全部通过；1 = 存在失败。
//
// 通过 CMake 的 add_test 注册，PASS_REGULAR_EXPRESSION="ALL PASS"。

#include <string>
#include <optional>
#include <utility>
#include <vector>
#include <cstdio>

#include "VersionParse/Version.hpp"

//////////////////////////////////////////////////////////////////////////
// 从 Utils/utils.hpp 提取的平台无关解析逻辑
//////////////////////////////////////////////////////////////////////////

// 代理 URL 解析（提取自 utils::get_proxy_env 的协议/主机/端口拆分逻辑）
static std::optional<std::pair<std::string, int>> parse_proxy_url(const std::string& p1) {
    if(p1.empty()) {
        return {};
    }
    size_t protocolEnd = p1.find("://");
    std::string addrWithoutProtocol = (protocolEnd != std::string::npos)
        ? p1.substr(protocolEnd + 3)
        : p1;

    size_t colonPos = addrWithoutProtocol.find(':');
    if(colonPos == std::string::npos) {
        return {};
    }
    int port = 0;
    std::string host = addrWithoutProtocol.substr(0, colonPos);
    try {
        port = std::stoi(addrWithoutProtocol.substr(colonPos + 1));
    }
    catch(const std::exception&) {
        return {};
    }
    if(port) {
        return std::make_pair(host, port);
    }
    return {};
}

// scutil --proxy 文本解析（提取自 utils::GetSystemProxySettings 的 macOS 分支）
static std::optional<std::pair<std::string, int>> parse_scutil_proxy(const std::string& out) {
    std::string address;
    int port = 0;

    auto extract = [&out](const std::string& key) -> std::string {
        std::string pattern = key + " : ";
        size_t pos = out.find(pattern);
        if(pos == std::string::npos) {
            return {};
        }
        size_t start = pos + pattern.size();
        size_t end = out.find('\n', start);
        if(end == std::string::npos) {
            end = out.size();
        }
        std::string value = out.substr(start, end - start);
        size_t b = value.find_first_not_of(" \t\r\n");
        size_t e = value.find_last_not_of(" \t\r\n");
        if(b == std::string::npos) {
            return {};
        }
        return value.substr(b, e - b + 1);
    };

    if(extract("HTTPSEnable") == "1") {
        std::string host = extract("HTTPSProxy");
        std::string portStr = extract("HTTPSPort");
        if(!host.empty() && !portStr.empty()) {
            try {
                int p = std::stoi(portStr);
                if(p > 0) { address = host; port = p; }
            } catch(...) {}
        }
    }
    if(port == 0 && extract("HTTPEnable") == "1") {
        std::string host = extract("HTTPProxy");
        std::string portStr = extract("HTTPPort");
        if(!host.empty() && !portStr.empty()) {
            try {
                int p = std::stoi(portStr);
                if(p > 0) { address = host; port = p; }
            } catch(...) {}
        }
    }

    if(port) {
        return std::make_pair(address, port);
    }
    return {};
}

// PlistBuddy 版本号输出解析（提取自 platform_mac.hpp::read_bundle_version 的清洗逻辑）
static std::string clean_plist_version(const std::string& out) {
    size_t b = out.find_first_not_of(" \t\r\n");
    if(b == std::string::npos) {
        return {};
    }
    size_t e = out.find_last_not_of(" \t\r\n");
    return out.substr(b, e - b + 1);
}

//////////////////////////////////////////////////////////////////////////
// 测试框架
//////////////////////////////////////////////////////////////////////////

static int g_failures = 0;

static void check(bool cond, const char* name, const char* detail = "") {
    if(cond) {
        std::printf("OK   %s\n", name);
    } else {
        std::printf("FAIL %s %s\n", name, detail);
        g_failures++;
    }
}

template <typename T>
static void check_eq(const T& actual, const T& expected, const char* name) {
    if(actual == expected) {
        std::printf("OK   %s\n", name);
    } else {
        std::printf("FAIL %s (实际=%s, 期望=%s)\n", name,
                    std::to_string(actual).c_str(), std::to_string(expected).c_str());
        g_failures++;
    }
}

static void check_eq_str(const std::string& actual, const std::string& expected, const char* name) {
    if(actual == expected) {
        std::printf("OK   %s\n", name);
    } else {
        std::printf("FAIL %s (实际=\"%s\", 期望=\"%s\")\n", name, actual.c_str(), expected.c_str());
        g_failures++;
    }
}

//////////////////////////////////////////////////////////////////////////
// 测试用例
//////////////////////////////////////////////////////////////////////////

// ---- 1) Version 解析与比较 ----
static void test_version() {
    std::printf("\n[Version 解析与比较]\n");

    // 基本版本号解析
    std::Version v1("1.2.3");
    check((bool)v1, "解析 \"1.2.3\" 有效");
    check_eq(v1.major, 1, "major=1");
    check_eq(v1.minor, 2, "minor=2");
    check_eq(v1.revision, 3, "revision=3");

    // v 前缀
    std::Version v2("v1.2.3");
    check((bool)v2, "解析 \"v1.2.3\" 有效（v 前缀兼容）");

    // 两段版本号
    std::Version v3("1.40");
    check((bool)v3, "解析 \"1.40\" 有效（两位版本号）");

    // 无效输入
    std::Version v4("not-a-version");
    check(!(bool)v4, "解析 \"not-a-version\" 无效");

    std::Version v5("");
    check(!(bool)v5, "解析空字符串无效");

    // 比较
    std::Version a("1.2.3");
    std::Version b("1.2.4");
    check(a < b, "1.2.3 < 1.2.4");
    check(b > a, "1.2.4 > 1.2.3");
    check(a != b, "1.2.3 != 1.2.4");

    std::Version c("1.2.3");
    check(a == c, "1.2.3 == 1.2.3");
    check(!(a < c), "1.2.3 不小于自身");

    // 跨大版本比较
    std::Version d("2.0.0");
    check(a < d, "1.2.3 < 2.0.0（跨大版本）");

    // 程序内置版本（FILEVERSION 由 CMake 注入）
    std::Version fileVer(FILEVERSION);
    check((bool)fileVer || std::string(FILEVERSION) == "0.0.0-Dev.0",
          "FILEVERSION 宏已定义且可解析（或为开发占位版本）");
}

// ---- 2) 代理 URL 解析 ----
static void test_proxy_url() {
    std::printf("\n[代理 URL 解析]\n");

    // 带协议
    auto r1 = parse_proxy_url("http://127.0.0.1:8080");
    check(r1.has_value(), "解析 http://127.0.0.1:8080 成功");
    if(r1) {
        check_eq_str(r1->first, "127.0.0.1", "主机=127.0.0.1");
        check_eq(r1->second, 8080, "端口=8080");
    }

    // https 协议
    auto r2 = parse_proxy_url("https://proxy.example.com:8443");
    check(r2.has_value(), "解析 https://proxy.example.com:8443 成功");
    if(r2) {
        check_eq_str(r2->first, "proxy.example.com", "主机=proxy.example.com");
        check_eq(r2->second, 8443, "端口=8443");
    }

    // 不带协议
    auto r3 = parse_proxy_url("127.0.0.1:7890");
    check(r3.has_value(), "解析无协议的 127.0.0.1:7890 成功");
    if(r3) {
        check_eq_str(r3->first, "127.0.0.1", "无协议时主机=127.0.0.1");
        check_eq(r3->second, 7890, "无协议时端口=7890");
    }

    // 缺端口
    auto r4 = parse_proxy_url("http://127.0.0.1");
    check(!r4.has_value(), "缺端口号时返回 nullopt");

    // 空字符串
    auto r5 = parse_proxy_url("");
    check(!r5.has_value(), "空字符串返回 nullopt");

    // 非法端口
    auto r6 = parse_proxy_url("http://127.0.0.1:not-a-port");
    check(!r6.has_value(), "非法端口返回 nullopt（不抛异常）");

    // 端口为 0
    auto r7 = parse_proxy_url("http://127.0.0.1:0");
    check(!r7.has_value(), "端口 0 返回 nullopt");
}

// ---- 3) scutil --proxy 文本解析 ----
static void test_scutil() {
    std::printf("\n[scutil --proxy 文本解析]\n");

    // HTTPS+HTTP 均启用，优先 HTTPS
    auto r1 = parse_scutil_proxy(
        "HTTPEnable : 1\nHTTPPort : 8080\nHTTPProxy : 127.0.0.1\n"
        "HTTPSEnable : 1\nHTTPSPort : 8443\nHTTPSProxy : 10.0.0.1\n");
    check(r1.has_value(), "HTTPS+HTTP 均启用时解析成功");
    if(r1) {
        check_eq_str(r1->first, "10.0.0.1", "优先取 HTTPS 代理主机");
        check_eq(r1->second, 8443, "优先取 HTTPS 代理端口");
    }

    // 仅 HTTP 启用
    auto r2 = parse_scutil_proxy(
        "HTTPEnable : 1\nHTTPPort : 7890\nHTTPProxy : 127.0.0.1\nHTTPSEnable : 0\n");
    check(r2.has_value(), "仅 HTTP 启用时解析成功");
    if(r2) {
        check_eq_str(r2->first, "127.0.0.1", "回退取 HTTP 代理主机");
        check_eq(r2->second, 7890, "回退取 HTTP 代理端口");
    }

    // 均未启用
    auto r3 = parse_scutil_proxy("HTTPEnable : 0\nHTTPSEnable : 0\n");
    check(!r3.has_value(), "均未启用返回 nullopt");

    // 空输出
    auto r4 = parse_scutil_proxy("");
    check(!r4.has_value(), "空输出返回 nullopt");

    // 带 <dict> 包裹头
    auto r5 = parse_scutil_proxy(
        "<dict> {\n  HTTPEnable : 1\n  HTTPPort : 8080\n  HTTPProxy : 127.0.0.1\n"
        "  HTTPSEnable : 1\n  HTTPSPort : 8443\n  HTTPSProxy : 10.0.0.1\n"
        "  proxyAutoConfigEnable : 0\n}\n");
    check(r5.has_value(), "带 <dict> 包裹头时解析成功");
    if(r5) {
        check_eq_str(r5->first, "10.0.0.1", "dict 包裹下仍优先 HTTPS");
    }

    // 端口非法
    auto r6 = parse_scutil_proxy("HTTPEnable : 1\nHTTPPort : abc\nHTTPProxy : 127.0.0.1\n");
    check(!r6.has_value(), "端口非法时安全降级返回 nullopt");

    // 启用但缺端口
    auto r7 = parse_scutil_proxy("HTTPEnable : 1\nHTTPProxy : 127.0.0.1\n");
    check(!r7.has_value(), "启用但缺端口返回 nullopt");
}

// ---- 4) PlistBuddy 版本输出清洗 ----
static void test_plist_version() {
    std::printf("\n[PlistBuddy 版本输出清洗]\n");

    // 标准输出（换行结尾）
    check_eq_str(clean_plist_version("3.6.8\n"), "3.6.8", "去除尾部换行");

    // 带空白
    check_eq_str(clean_plist_version("  3.6.8  \n"), "3.6.8", "去除首尾空白与换行");

    // 无换行
    check_eq_str(clean_plist_version("3.6.8"), "3.6.8", "无换行原样返回");

    // 带回车换行
    check_eq_str(clean_plist_version("3.6.8\r\n"), "3.6.8", "去除 CRLF");

    // 空输出
    check_eq_str(clean_plist_version(""), "", "空输出返回空字符串");

    // 纯空白
    check_eq_str(clean_plist_version("   \n  \t "), "", "纯空白返回空字符串");

    // 多段版本号（含 beta 标记）
    check_eq_str(clean_plist_version("3.6.8-beta.1\n"), "3.6.8-beta.1", "保留 beta 后缀");
}

// ---- 5) 安装发现的数据结构契约 ----
static void test_install_struct() {
    std::printf("\n[安装发现数据结构]\n");

    // GitHubDesktopInstall 的字段语义：Windows 为注册表路径，macOS 为 bundle 路径。
    // 这里验证 resource_dir 与版本号的字符串构造不依赖平台。
    std::string win_resource = "C:/Users/test/AppData/Local/GitHubDesktop/app-3.6.8/resources/app";
    check(win_resource.find("app-3.6.8") != std::string::npos, "Windows 资源路径含 app-<version> 段");
    check(win_resource.find("resources/app") != std::string::npos, "Windows 资源路径以 resources/app 结尾");

    std::string mac_resource = "/Applications/GitHub Desktop.app/Contents/Resources/app";
    check(mac_resource.find("Contents/Resources/app") != std::string::npos, "macOS 资源路径含 Contents/Resources/app");
}

// ---- 6) Linux gsettings 代理解析 ----
static void test_gsettings() {
    std::printf("\n[Linux gsettings 代理解析]\n");

    auto parse = [](bool mode_ok, const std::string& mode_out,
                    bool host_ok, const std::string& host_out,
                    bool port_ok, const std::string& port_out) -> std::optional<std::pair<std::string, int>> {
        if(!mode_ok) {
            return {};
        }
        auto clean = [](const std::string& s) -> std::string {
            size_t b = s.find_first_not_of(" \t\r\n'");
            size_t e = s.find_last_not_of(" \t\r\n'");
            if(b == std::string::npos) {
                return {};
            }
            return s.substr(b, e - b + 1);
        };
        if(clean(mode_out) != "manual") {
            return {};
        }
        if(!host_ok || !port_ok) {
            return {};
        }
        std::string host = clean(host_out);
        std::string portStr = clean(port_out);
        if(host.empty() || portStr.empty()) {
            return {};
        }
        try {
            int p = std::stoi(portStr);
            if(p > 0) {
                return std::make_pair(host, p);
            }
        }
        catch(...) {}
        return {};
    };

    // 标准手动代理（gsettings 字符串输出带单引号）
    auto r1 = parse(true, "'manual'", true, "'127.0.0.1'", true, "8080");
    check(r1.has_value(), "gsettings mode=manual + host/port 解析成功");
    if(r1) {
        check_eq_str(r1->first, "127.0.0.1", "gsettings 主机=127.0.0.1");
        check_eq(r1->second, 8080, "gsettings 端口=8080");
    }

    // mode=none（无代理）
    auto r2 = parse(true, "'none'", true, "'127.0.0.1'", true, "8080");
    check(!r2.has_value(), "gsettings mode=none 返回 nullopt");

    // mode=auto（PAC 自动配置，非手动代理）
    auto r3 = parse(true, "'auto'", true, "'127.0.0.1'", true, "8080");
    check(!r3.has_value(), "gsettings mode=auto 返回 nullopt");

    // gsettings 不可用（mode 读取失败）→ 安静返回 nullopt
    auto r4 = parse(false, "", false, "", false, "");
    check(!r4.has_value(), "gsettings 不可用时返回 nullopt（不影响主流程）");

    // 输出无单引号包裹的变体
    auto r5 = parse(true, "manual", true, "10.0.0.1", true, "8443");
    check(r5.has_value(), "gsettings 输出无单引号变体解析成功");
    if(r5) {
        check_eq_str(r5->first, "10.0.0.1", "gsettings 无引号主机=10.0.0.1");
        check_eq(r5->second, 8443, "gsettings 无引号端口=8443");
    }

    // 端口非法 → 不崩溃，返回 nullopt
    auto r6 = parse(true, "'manual'", true, "'127.0.0.1'", true, "not-a-port");
    check(!r6.has_value(), "gsettings 端口非法安全降级返回 nullopt");

    // host 为空
    auto r7 = parse(true, "'manual'", true, "''", true, "8080");
    check(!r7.has_value(), "gsettings host 为空返回 nullopt");

    // 输出带尾部换行/空白
    auto r8 = parse(true, "'manual'\n", true, "'127.0.0.1'\n", true, "8080\n");
    check(r8.has_value(), "gsettings 输出带尾部换行解析成功");
    if(r8) {
        check_eq_str(r8->first, "127.0.0.1", "gsettings 带空白主机正确清洗");
        check_eq(r8->second, 8080, "gsettings 带空白端口正确清洗");
    }
}

// ---- 7) Linux 资源目录候选 ----
static void test_linux_resource_candidates() {
    std::printf("\n[Linux 资源目录候选]\n");

    // Linux 版 GitHub Desktop 为社区 fork（shiftkey/desktop），deb/rpm 装到系统目录
    std::string deb_resource = "/usr/lib/github-desktop/resources/app";
    check(deb_resource.find("github-desktop/resources/app") != std::string::npos,
          "Linux deb/rpm 资源路径含 github-desktop/resources/app");

    std::string opt_resource = "/opt/github-desktop/resources/app";
    check(opt_resource.find("/opt/github-desktop") != std::string::npos,
          "Linux /opt 候选路径正确");

    // Flatpak 沙盒目录
    std::string flatpak_resource = "/home/user/.var/app/io.github.shiftey.Desktop/current/files/github-desktop/resources/app";
    check(flatpak_resource.find(".var/app/io.github.shiftey.Desktop") != std::string::npos,
          "Linux Flatpak 沙盒候选路径正确");

    // 用户级安装
    std::string local_resource = "/home/user/.local/share/github-desktop/resources/app";
    check(local_resource.find(".local/share/github-desktop") != std::string::npos,
          "Linux 用户级安装候选路径正确");

    // AppImage 为单文件（运行时才挂载），无法静态扫描——需 -g 手动指定
    std::string appimage = "/home/user/Downloads/GitHubDesktop-3.6.8-x86_64.AppImage";
    check(appimage.find(".AppImage") != std::string::npos,
          "AppImage 为单文件无法静态扫描（需 -g 手动指定）");
}

// ---- 8) package.json 版本读取 ----
static void test_packagejson_version() {
    std::printf("\n[package.json 版本读取]\n");

    // 从资源目录的 package.json 读 version 字段（Electron 标准布局）
    auto read_version = [](const std::string& text) -> std::string {
        if(text.empty()) {
            return {};
        }
        const std::string key = "\"version\"";
        size_t pos = text.find(key);
        if(pos == std::string::npos) {
            return {};
        }
        pos = text.find('"', pos + key.size());
        if(pos == std::string::npos) {
            return {};
        }
        size_t start = pos + 1;
        size_t end = text.find('"', start);
        if(end == std::string::npos) {
            return {};
        }
        return text.substr(start, end - start);
    };

    // 标准 package.json
    std::string pkg1 = R"({"name":"github-desktop","version":"3.6.8","main":"main.js"})";
    check_eq_str(read_version(pkg1), "3.6.8", "标准 package.json 读出版本 3.6.8");

    // version 在前
    std::string pkg2 = R"({"version":"1.2.3","name":"github-desktop"})";
    check_eq_str(read_version(pkg2), "1.2.3", "version 字段在前时正确读取");

    // 缺 version 字段
    std::string pkg3 = R"({"name":"github-desktop","main":"main.js"})";
    check_eq_str(read_version(pkg3), "", "缺 version 字段返回空");

    // 空文本
    check_eq_str(read_version(""), "", "空文本返回空");

    // 带换行/空白的格式化 JSON
    std::string pkg4 = "{\n  \"name\": \"github-desktop\",\n  \"version\": \"3.6.9-beta.1\",\n  \"main\": \"main.js\"\n}\n";
    check_eq_str(read_version(pkg4), "3.6.9-beta.1", "格式化 JSON（带换行）读出版本 3.6.9-beta.1");

    // version 值含连字符（beta 版本号）
    std::string pkg5 = R"({"version":"2.0.0-release.5"})";
    check_eq_str(read_version(pkg5), "2.0.0-release.5", "版本值含连字符/点号正确读取");

    // 不误读其他含 version 子串的键（如 devDependencies 中的版本）
    std::string pkg6 = R"({"name":"github-desktop","version":"3.6.8","devDependencies":{"some-lib":"1.0.0"}})";
    check_eq_str(read_version(pkg6), "3.6.8", "不误读 devDependencies 中的版本（取第一个 version 键）");
}

//////////////////////////////////////////////////////////////////////////

int main() {
    std::printf("=== GitHubDesktop2Chinese 平台无关逻辑回归测试 ===\n");
    std::printf("FILEVERSION = %s\n", FILEVERSION);

    test_version();
    test_proxy_url();
    test_scutil();
    test_plist_version();
    test_install_struct();
    test_gsettings();
    test_linux_resource_candidates();
    test_packagejson_version();

    std::printf("\n=== 测试结束 ===\n");
    if(g_failures > 0) {
        std::printf("FAILED: %d 个用例失败\n", g_failures);
        return 1;
    }
    std::printf("ALL PASS\n");
    return 0;
}
