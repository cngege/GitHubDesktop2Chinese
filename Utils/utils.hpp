#ifndef UTILS_IS_EXITES
#define UTILS_IS_EXITES

#include <string>
#include <optional>
#include <vector>
#include <fstream>
#include <sstream>
#include <limits>
#include <locale>
#include <filesystem>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <stdexcept>    // std::runtime_error
#include <utility>      // std::pair / std::make_pair
#include <atomic>       // std::atomic（WriteFile 临时文件序号）

//////////////////////////////////////////////////////////////////////////
// 平台相关头文件
//////////////////////////////////////////////////////////////////////////
#if defined(_WIN32) || defined(_WIN64)
    #ifndef _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
        #define _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
    #endif
    #ifndef _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
        #define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING   //消除 converter.to_bytes的警告
    #endif
    #ifndef _CRT_SECURE_NO_WARNINGS
        #define _CRT_SECURE_NO_WARNINGS                             //消除 sprintf的警告
    #endif

    #include <tchar.h>
    #include <windows.h>
    #include <shellapi.h>
    #include <winhttp.h>
#else
    #include <unistd.h>     // access, STDERR_FILENO
    #include <string.h>     // strlen
#endif

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "http/httplib.h"
#include "Utils/net_parse.hpp"      // 代理地址解析（纯字符串，便于单测）
#include "Utils/sha256.hpp"         // 自更新下载完整性校验用的 SHA-256 实现
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

class utils {
public:
    //////////////////////////////////////////////////////////////////////////
    // HTTPS 证书校验开关
    // 默认开启。自更新流程会下载并执行可执行文件，关闭校验等于把执行权交给
    // 链路上的任何人；只有在企业中间人代理等确实无法通过校验的环境下，
    // 才由 --insecure-tls 显式关闭。
    //////////////////////////////////////////////////////////////////////////
    static bool& tls_verification_enabled() {
        static bool enabled = true;
        return enabled;
    }

    // 代理解析的实现在 Utils/net_parse.hpp（纯字符串逻辑，便于单测），这里只做转发
    static auto parse_proxy_address(const std::string& raw) -> std::optional<std::pair<std::string, int>> {
        return netparse::parse_proxy_address(raw);
    }

    /**
     * @brief 证书校验失败时给出一次性的可操作提示
     *
     * 校验失败最常见的原因是链路上存在中间人证书（企业代理/自签证书/抓包工具）。
     * 只在第一次失败时提示，避免每次请求都刷屏。
     */
    static void hint_if_certificate_failed(httplib::Error err) {
        if (err != httplib::Error::SSLServerVerification) {
            return;
        }
        static bool hinted = false;
        if (hinted) {
            return;
        }
        hinted = true;
        spdlog::warn("HTTPS 证书校验失败(常见于企业代理或自签证书)。确认网络可信后可加 --insecure-tls 重试");
    }

    static std::string GetEnvVar(const std::string& varName) {
#if defined(_WIN32) || defined(_WIN64)
        // Windows API 优先使用宽字符版本，避免编码问题
        std::wstring wVarName(varName.begin(), varName.end());
        wchar_t* wValue = _wgetenv(wVarName.c_str());

        if(wValue == nullptr) return "";

        // 转换 UTF-16 到 UTF-8
        int len = WideCharToMultiByte(CP_UTF8, 0, wValue, -1, nullptr, 0, nullptr, nullptr);
        std::string value(len, 0);
        WideCharToMultiByte(CP_UTF8, 0, wValue, -1, &value[0], len, nullptr, nullptr);
        value.pop_back(); // 移除末尾的空字符
        return value;
#else
        // POSIX 环境变量本就以 UTF-8 编码（macOS 终端默认 UTF-8）
        const char* value = std::getenv(varName.c_str());
        return (value == nullptr) ? std::string() : std::string(value);
#endif
    }

    static std::optional<std::pair<std::string, int>> get_proxy_env() {
        auto p1 = GetEnvVar("HTTPS_PROXY");
        if(p1.empty()) p1 = GetEnvVar("https_proxy");

        if(p1.empty()) {
            return {};
        }
        auto parsed = parse_proxy_address(p1);
        if(!parsed) {
            spdlog::warn("环境变量中的代理地址无法解析(需要 host:port 形式): {}", p1);
        }
        return parsed;
    }

    static std::optional<std::pair<std::string, int>> GetSystemProxySettings() {
#if defined(_WIN32) || defined(_WIN64)
        // Windows 实现
        WINHTTP_CURRENT_USER_IE_PROXY_CONFIG ieProxyConfig = {};
        std::optional<std::pair<std::string, int>> result;

        if(WinHttpGetIEProxyConfigForCurrentUser(&ieProxyConfig)) {
            if(ieProxyConfig.lpszProxy) {
                // 宽字符转 UTF-8：直接按字节截断会把非 ASCII 主机名破坏掉。
                // 值可能是 "host:port"，也可能是 "http=host:port;https=host:port"，
                // 后者按字节找第一个 ':' 会把 "http=host" 当成主机名。
                result = parse_proxy_address(to_byte_string(std::wstring(ieProxyConfig.lpszProxy)));
            }

            // 清理资源
            if(ieProxyConfig.lpszProxy) GlobalFree(ieProxyConfig.lpszProxy);
            if(ieProxyConfig.lpszProxyBypass) GlobalFree(ieProxyConfig.lpszProxyBypass);
            if(ieProxyConfig.lpszAutoConfigUrl) GlobalFree(ieProxyConfig.lpszAutoConfigUrl);
        }
        return result;
#elif defined(__APPLE__) && defined(__MACH__)
        // macOS 实现：解析 scutil --proxy 输出
        //   HTTPEnable : 1
        //   HTTPProxy : 127.0.0.1
        //   HTTPPort : 8080
        //   HTTPSEnable : 1
        //   HTTPSProxy : 127.0.0.1
        //   HTTPSPort : 8080
        std::string address;
        int port = 0;
        std::string out;
        if(!run_command("scutil --proxy", out)) {
            return {};
        }

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
            // 去除首尾空白
            size_t b = value.find_first_not_of(" \t\r\n");
            size_t e = value.find_last_not_of(" \t\r\n");
            if(b == std::string::npos) {
                return {};
            }
            return value.substr(b, e - b + 1);
        };

        // 优先使用 HTTPS 代理，其次 HTTP 代理
        if(extract("HTTPSEnable") == "1") {
            std::string host = extract("HTTPSProxy");
            std::string portStr = extract("HTTPSPort");
            if(!host.empty() && !portStr.empty()) {
                try {
                    int p = std::stoi(portStr);
                    if(p > 0) {
                        address = host;
                        port = p;
                    }
                }
                catch(...) {}
            }
        }
        if(port == 0 && extract("HTTPEnable") == "1") {
            std::string host = extract("HTTPProxy");
            std::string portStr = extract("HTTPPort");
            if(!host.empty() && !portStr.empty()) {
                try {
                    int p = std::stoi(portStr);
                    if(p > 0) {
                        address = host;
                        port = p;
                    }
                }
                catch(...) {}
            }
        }
#else
        // Linux 实现：解析 gsettings（GNOME 系统代理）
        //   gsettings read org.gnome.system.proxy mode        -> 'manual' / 'none' / 'auto'
        //   gsettings read org.gnome.system.proxy.http host   -> '127.0.0.1'
        //   gsettings read org.gnome.system.proxy.http port   -> 8080
        // gsettings 字符串输出带单引号包裹，需清洗；不可用时（无 GNOME/非交互会话）
        // run_command 返回 false，安静返回 nullopt，不影响主流程
        std::string address;
        int port = 0;
        std::string mode_out;
        if(!run_command("gsettings read org.gnome.system.proxy mode", mode_out)) {
            return {};
        }

        // 去除 gsettings 输出的单引号与首尾空白
        auto clean = [](const std::string& s) -> std::string {
            size_t b = s.find_first_not_of(" \t\r\n'");
            size_t e = s.find_last_not_of(" \t\r\n'");
            if(b == std::string::npos) {
                return {};
            }
            return s.substr(b, e - b + 1);
        };

        if(clean(mode_out) != "manual") {
            // 'none' 或 'auto' 时无手动代理配置
            return {};
        }

        std::string host_out;
        std::string port_out;
        if(run_command("gsettings read org.gnome.system.proxy.http host", host_out)
           && run_command("gsettings read org.gnome.system.proxy.http port", port_out)) {
            std::string host = clean(host_out);
            std::string portStr = clean(port_out);
            if(!host.empty() && !portStr.empty()) {
                try {
                    int p = std::stoi(portStr);
                    if(p > 0) {
                        address = host;
                        port = p;
                    }
                }
                catch(...) {
                    // 端口非法：返回 nullopt，不崩溃
                }
            }
        }
#endif
#if !defined(_WIN32) && !defined(_WIN64)
        if(port) {
            return std::make_pair(address, port);
        }
#endif
        return {};
    }

#if !defined(_WIN32) && !defined(_WIN64)
private:
    // 执行命令并捕获标准输出（文本）
    static bool run_command(const std::string& cmd, std::string& out) {
        FILE* pipe = ::popen(cmd.c_str(), "r");
        if(pipe == nullptr) {
            return false;
        }
        std::string buffer(4096, '\0');
        size_t n = 0;
        while((n = ::fread(&buffer[0], 1, buffer.size(), pipe)) > 0) {
            out.append(buffer.data(), n); // 按 n 字节追加，避免内部 '\0' 截断
        }
        int status = ::pclose(pipe);
        return status == 0;
    }

public:
#endif

    static auto ansiToUtf8(const std::string& ansiString) -> std::string {
#if defined(_WIN32) || defined(_WIN64)
        int ansiSize = static_cast<int>(ansiString.size());
        int utf8Size = MultiByteToWideChar(CP_ACP, 0, ansiString.c_str(), ansiSize, nullptr, 0);
        std::vector<wchar_t> wideString(utf8Size);
        MultiByteToWideChar(CP_ACP, 0, ansiString.c_str(), ansiSize, wideString.data(), utf8Size);
        utf8Size = WideCharToMultiByte(CP_UTF8, 0, wideString.data(), utf8Size, nullptr, 0, nullptr, nullptr);
        std::vector<char> utf8String(utf8Size);
        WideCharToMultiByte(CP_UTF8, 0, wideString.data(), utf8Size, utf8String.data(), utf8Size, nullptr, nullptr);
        return std::string(utf8String.begin(), utf8String.end());
#else
        // macOS 终端默认即为 UTF-8，不存在 ANSI/UTF-8 转换需求，原样返回
        return ansiString;
#endif
    }

    static auto utf8ToAnsi(const std::string& utf8String) -> std::string {
#if defined(_WIN32) || defined(_WIN64)
        int utf8Size = static_cast<int>(utf8String.size());
        int ansiSize = MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), utf8Size, nullptr, 0);
        std::vector<wchar_t> wideString(ansiSize);
        MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), utf8Size, wideString.data(), ansiSize);
        ansiSize = WideCharToMultiByte(CP_ACP, 0, wideString.data(), ansiSize, nullptr, 0, nullptr, nullptr);
        std::vector<char> ansiString(ansiSize);
        WideCharToMultiByte(CP_ACP, 0, wideString.data(), ansiSize, ansiString.data(), ansiSize, nullptr, nullptr);
        return std::string(ansiString.begin(), ansiString.end());
#else
        // macOS 终端默认即为 UTF-8，不存在 ANSI/UTF-8 转换需求，原样返回
        return utf8String;
#endif
    }

    // UTF-8 <-> UTF-16 转换。
    // 早先用 std::wstring_convert<std::codecvt_utf8<wchar_t>>，但 <codecvt> 已被弃用，
    // 新版 AppleClang/libc++ 甚至不再提供该头文件，导致 macOS 构建直接失败。
    // 这两个函数只在 Windows 分支被调用，这里改用等价的 Win32 API；
    // 非 Windows 平台保留接口但不再依赖 codecvt。
    inline static auto to_wide_string(const std::string& input)-> std::wstring
    {
#if defined(_WIN32) || defined(_WIN64)
        if(input.empty()) return {};
        const int len = ::MultiByteToWideChar(CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()), nullptr, 0);
        if(len <= 0) return {};
        std::wstring out(static_cast<size_t>(len), L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()), out.data(), len);
        return out;
#else
        return std::wstring(input.begin(), input.end());
#endif
    }

    inline static auto to_byte_string(const std::wstring& input)->std::string
    {
#if defined(_WIN32) || defined(_WIN64)
        if(input.empty()) return {};
        const int len = ::WideCharToMultiByte(CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()), nullptr, 0, nullptr, nullptr);
        if(len <= 0) return {};
        std::string out(static_cast<size_t>(len), '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()), out.data(), len, nullptr, nullptr);
        return out;
#else
        return std::string(input.begin(), input.end());
#endif
    }

    /**
     * @brief 读取文件全部内容（二进制）
     * @param filename 目标文件路径
     * @param ok 可选输出：是否成功打开并完整读取。失败时返回空串，
     *           调用方必须据此判断，避免把空串当成真实内容写回磁盘。
     */
    static auto ReadFile(const std::string& filename, bool* ok = nullptr)->std::string {
        if (ok) *ok = false;
        std::ifstream fin(filename, std::ios::binary);
        if (!fin) {
            return {};
        }
        const std::istreambuf_iterator<char> begin(fin);
        const std::istreambuf_iterator<char> end;
        std::string content(begin, end);
        if (fin.bad()) {
            return {};
        }
        if (ok) *ok = true;
        return content;
    }

    /**
     * @brief 把系统错误码翻译成可读文本, 并对常见错误补充可能原因
     * @param code 系统错误码(Windows 下为 GetLastError 的返回值, 其他平台为 errno)
     */
    static auto SysErrorText(unsigned long code) -> std::string {
#if defined(_WIN32) || defined(_WIN64)
        std::string msg;
        LPSTR buf = nullptr;
        const DWORD len = ::FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPSTR>(&buf), 0, nullptr);
        if (len && buf != nullptr) {
            msg.assign(buf, len);
        }
        if (buf != nullptr) {
            ::LocalFree(buf);
        }
        while (!msg.empty() && (msg.back() == '\r' || msg.back() == '\n' || msg.back() == ' ')) {
            msg.pop_back();
        }
        if (msg.empty()) {
            msg = "系统未提供描述";
        }

        const char* hint = "";
        switch (code) {
        case 2:   // ERROR_FILE_NOT_FOUND
        case 3:   hint = "路径不存在, 请确认 GitHub Desktop 安装目录是否正确"; break;
        case 5:   hint = "权限不足, 或被杀毒/安全软件(如火绒、Defender)拦截"; break;
        case 19:  hint = "介质写保护"; break;
        case 32:  hint = "文件被占用, 请完全退出 GitHub Desktop 后重试"; break;
        case 112: hint = "磁盘空间不足"; break;
        case 145: hint = "目标目录非空或路径指向了目录"; break;
        default:  break;
        }
        if (hint[0] != '\0') {
            msg += std::string(" -- ") + hint;
        }
        return msg;
#else
        return std::string(std::strerror(static_cast<int>(code)));
#endif
    }

    /**
     * @brief 将内容写入文件
     * @param filename 目标文件路径
     * @param txt 要写入的内容
     * @return 是否写入成功. 失败时会输出确切的系统错误码及可能原因
     */
    // 内容参数统一为 const：若这里写成非 const 引用，在 libc++ 下
    // fs::path 可隐式转换为 std::string，调用 WriteFile(fs::path, std::string)
    // 时两个重载会各胜一个参数而判为二义，macOS 编译直接失败。
    static auto WriteFile(const std::string& filename, const std::string& txt) -> bool {
        return WriteFile(fs::path(filename), txt);
    }

    /**
     * @brief 将内容写入文件（原子替换）
     * @param filename 目标文件路径
     * @param txt 要写入的内容
     * @return 是否写入成功. 失败时会输出确切的系统错误码及可能原因
     *
     * 先写入同目录下的临时文件，成功后再原子替换目标：
     * 写入过程中目标文件始终保持原有完整内容，进程被中断或写入失败时不会留下半个文件
     * （原实现直接 CREATE_ALWAYS / ofstream 截断目标，中断即留下残缺内容）。
     * 临时文件必须与目标同目录，才能保证替换是同一文件系统内的原子 rename。
     *
     * 使用 fs::path 而不是 fs::path::string()：在 Windows 上 string() 返回的是
     * 本地代码页(GBK 等)窄字节，若再按 UTF-8 转宽字符会在中文等非 ASCII 安装
     * 路径下抛 std::range_error。
     */
    static auto WriteFile(const fs::path& filename, const std::string& txt) -> bool {
        // 临时文件名带进程号与自增序号，避免并发/多进程写同一目标时互相覆盖
        static std::atomic<unsigned long> seq{ 0 };
#if defined(_WIN32) || defined(_WIN64)
        const auto pid = static_cast<unsigned long>(::GetCurrentProcessId());
#else
        const auto pid = static_cast<unsigned long>(::getpid());
#endif
        fs::path tmp = filename;
        tmp += ".tmp-" + std::to_string(pid) + "-" + std::to_string(seq.fetch_add(1));

        if(!WriteFileRaw(tmp, txt)) {
            // 写入失败：清掉半成品临时文件，目标文件保持原样
            std::error_code rmec;
            fs::remove(tmp, rmec);
            return false;
        }

        // 目标已存在时把原有权限带到临时文件上，替换后权限不变
        std::error_code ec;
        if(fs::exists(filename, ec)) {
            const auto st = fs::status(filename, ec);
            if(!ec) {
                std::error_code pec;
                fs::permissions(tmp, st.permissions(), fs::perm_options::replace, pec);
            }
        }

        if(!ReplaceFileAtomic(tmp, filename)) {
            std::error_code rmec;
            fs::remove(tmp, rmec);
            return false;
        }
        return true;
    }

    /**
     * @brief 实际写入内容（不负责原子替换，供 WriteFile 内部使用）
     */
    static auto WriteFileRaw(const fs::path& filename, const std::string& txt) -> bool {
#if defined(_WIN32) || defined(_WIN64)
        // 直接使用 Win32 API: 写入失败时才拿得到确切的错误码, 而不是笼统的一句"失败"
        HANDLE hFile = ::CreateFileW(
            filename.c_str(),
            GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,    // 与原 ofstream 的共享模式保持一致
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

        if (hFile == INVALID_HANDLE_VALUE) {
            const DWORD err = ::GetLastError();
            spdlog::error("打开并写入目标文件:{} 时失败 - 错误码 {} ({})", filename.string(), err, SysErrorText(err));
            return false;
        }

        DWORD written = 0;
        const BOOL ok = ::WriteFile(hFile, txt.data(), static_cast<DWORD>(txt.size()), &written, nullptr);
        const DWORD err = ok ? 0 : ::GetLastError();
        ::CloseHandle(hFile);

        if (!ok || static_cast<std::size_t>(written) != txt.size()) {
            spdlog::error("打开并写入目标文件:{} 时失败 - 错误码 {} ({})", filename.string(), err, SysErrorText(err));
            return false;
        }
        return true;
#else
        std::ofstream out(filename, std::ios::binary);
        out.write(txt.data(), static_cast<std::streamsize>(txt.size()));
        if (!out) {
            const int err = errno;
            spdlog::error("打开并写入目标文件:{} 时失败 - 错误码 {} ({})", filename.string(), err, SysErrorText(err));
            return false;
        }
        out.close();
        return true;
#endif
    }

    /**
     * @brief 用 from 原子替换 to（同一文件系统内）
     * @return 替换是否成功
     *
     * Windows 的 std::filesystem::rename 在目标已存在时会失败，因此改用
     * MoveFileExW + MOVEFILE_REPLACE_EXISTING；MOVEFILE_WRITE_THROUGH 保证
     * 替换在落盘后才返回。
     * POSIX 的 rename(2) 本身就是原子替换，直接使用。
     */
    static auto ReplaceFileAtomic(const fs::path& from, const fs::path& to) -> bool {
#if defined(_WIN32) || defined(_WIN64)
        if(::MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            return true;
        }
        const DWORD err = ::GetLastError();
        spdlog::error("替换目标文件:{} 时失败 - 错误码 {} ({})", to.string(), err, SysErrorText(err));
        return false;
#else
        std::error_code ec;
        fs::rename(from, to, ec);
        if(ec) {
            spdlog::error("替换目标文件:{} 时失败 - {}", to.string(), ec.message());
            return false;
        }
        return true;
#endif
    }

    /**
     * @brief 从网络上读取数据
     * @param url_host 网络主机
     * @param params 请求链接后缀
     * @param out 成功后输出读取到的数据
     * @return 是否成功
     */
    static auto ReadHttpDataString(std::string url_host, std::string params, std::string& out, std::pair<std::string, int> proxy = {}) -> bool {
        httplib::Client cli(url_host);
        if(proxy.second) {
            cli.set_proxy(proxy.first, proxy.second);
        }
        // 证书校验默认开启；仅 --insecure-tls 显式关闭（见 tls_verification_enabled）
        cli.enable_server_certificate_verification(tls_verification_enabled());
        // 显式设置超时：httplib 默认连接/读取超时是 300 秒，断网时界面会像卡死
        cli.set_connection_timeout(10, 0);
        cli.set_read_timeout(30, 0);
        cli.set_follow_location(true);                          //https://raw.github.com 会要求301重定向
        auto res = cli.Get(params);
        if(!res) {
            for(int i = 0; i < 3; i++) {
                res = cli.Get(params);
                if(res) break;
            }
        }
        if (!res) {
            spdlog::debug("请求 {} 失败: {}", params, httplib::to_string(res.error()));
            hint_if_certificate_failed(res.error());
            return false;
        }
        if (res->status == httplib::StatusCode::OK_200) {
            out = res->body;
            return true;
        }
        spdlog::debug("请求 {} 返回状态码 {}", params, res->status);
        return false;
    }

    //static auto DownloadHttpResource(std::string url_host, std::string params);
    /**
     * @brief 计算文件的 SHA-256（小写十六进制）
     * @param path 文件路径
     * @param out 成功后输出摘要
     * @return 是否计算成功
     */
    static auto sha256_file_hex(const fs::path& path, std::string& out) -> bool {
        std::ifstream in(path, std::ios::binary);
        if(!in) return false;
        sha256::Sha256 h;
        std::vector<char> buf(64 * 1024);
        while(in) {
            in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
            const std::streamsize got = in.gcount();
            if(got > 0) {
                h.update(reinterpret_cast<const unsigned char*>(buf.data()), static_cast<std::size_t>(got));
            }
        }
        if(in.bad()) return false;
        out = h.final_hex();
        return true;
    }

    static auto UpdateProgram(std::string url_host, std::string params, fs::path Self, int64_t max_size, std::pair<std::string, int> proxy, const std::string& expected_sha256 = "") ->bool{
        fs::path parent_dir = Self.parent_path();   // 文件所在目录
        fs::path exe_name = Self.filename();                // 文件名 包含扩展名,但不包含路径

        fs::path new_file = Self;
        fs::path tmp_file = Self;
        tmp_file += ".new"; // 下载临时文件

            // 1. 如果临时文件已存在，获取已下载大小（用于断点续传）
        uint64_t downloaded_bytes = 0;
        {
            std::error_code ec;
            if(fs::exists(tmp_file, ec) && !ec) {
                const auto size = fs::file_size(tmp_file, ec);
                if(!ec) downloaded_bytes = static_cast<uint64_t>(size);
            }
        }

        const uint64_t total_size = (max_size > 0) ? static_cast<uint64_t>(max_size) : 0;
        if(downloaded_bytes < total_size) {
            // 以 二进制追加模式 打开文件（断点续传关键）
            std::ofstream downfile(tmp_file, std::ios::binary | std::ios::out | std::ios::app);
            if(!downfile.is_open()) {
                spdlog::error("无法打开临时文件写入新版本");
                return false;
            }

            httplib::Client cli(url_host);
            if(proxy.second) {
                cli.set_proxy(proxy.first, proxy.second);
            }
            cli.enable_server_certificate_verification(tls_verification_enabled());
            cli.set_connection_timeout(15, 0);
            cli.set_read_timeout(60, 0);
            cli.set_follow_location(true);                          //https://raw.github.com 会要求301重定向
            httplib::Headers headers;
            if(downloaded_bytes > 0) {
                headers.emplace("Range", "bytes=" + std::to_string(downloaded_bytes) + "-");
            }
            headers.emplace("Accept", "application/octet-stream");

            auto res = cli.Get(params, headers,
            [&](const char* data, size_t data_length) {
                if(data_length > 0 && downfile.is_open()) {
                    downfile.write(data, data_length);
                    downfile.flush(); // 立即刷入磁盘，不缓存
                }
                return downfile.is_open();
            },
            [&](uint64_t len, uint64_t total) {
                uint64_t total_ = downloaded_bytes + total;  // 文件总大小
                uint64_t now_ = downloaded_bytes + len;
                // 服务器未返回 Content-Length 且无续传偏移时 total_ 可能为 0, 避免除零
                int percent_ = (total_ > 0) ? static_cast<int>(now_ * 100 / total_) : 0;

                std::printf("\r %s %d%% ==>  %lld / %lld", (downloaded_bytes > 0) ? "[续传]" : "[下载]", percent_, (unsigned long long)now_, (unsigned long long)total_);
                return true;
            });
            std::printf("\n");
            downfile.close();

            if(!res) {
                spdlog::error("网络请求失败:{}", httplib::to_string(res.error()));
                hint_if_certificate_failed(res.error());
                return false;
            }
            // 请求了断点续传却收到 200（服务器不支持 Range）：此时文件已被追加了整份内容，
            // 继续执行会得到一个损坏的可执行文件，直接清理临时文件后让用户重试。
            if(downloaded_bytes > 0 && res->status == httplib::StatusCode::OK_200) {
                spdlog::warn("服务器未按断点续传响应, 已清理临时文件, 请重新执行更新");
                std::error_code rm_ec;
                fs::remove(tmp_file, rm_ec);
                return false;
            }
            if(res->status != httplib::StatusCode::OK_200 && res->status != httplib::StatusCode::PartialContent_206) {
                spdlog::error("更新错误, 服务器返回错误的状态码: {}", res->status);
                return false;
            }
        }

        // 下载完整性校验：大小不符时绝不执行，避免运行被截断的二进制
        {
            std::error_code size_ec;
            const uint64_t actual = fs::file_size(tmp_file, size_ec);
            if(size_ec || actual != static_cast<uint64_t>(max_size)) {
                spdlog::error("更新文件大小校验失败: 期望 {} 字节, 实际 {} 字节, 已删除临时文件",
                              max_size, size_ec ? 0ULL : actual);
                std::error_code rm_ec;
                fs::remove(tmp_file, rm_ec);
                return false;
            }
        }

        // 下载完整性校验：SHA-256 不符时绝不执行，避免运行被替换/损坏的二进制。
        // 期望值取自 Release 中与产物同名的 "<产物名>.sha256" 附属文件；
        // 该附属文件不存在时（发布端尚未提供）跳过此校验，保持与旧版一致的更新行为。
        if(!expected_sha256.empty()) {
            std::string actual_sha256;
            if(!sha256_file_hex(tmp_file, actual_sha256)) {
                spdlog::error("更新文件 SHA-256 计算失败, 已删除临时文件");
                std::error_code rm_ec;
                fs::remove(tmp_file, rm_ec);
                return false;
            }
            if(actual_sha256 != expected_sha256) {
                spdlog::error("更新文件 SHA-256 校验失败: 期望 {}, 实际 {}, 已删除临时文件, 不会执行该文件",
                              expected_sha256, actual_sha256);
                std::error_code rm_ec;
                fs::remove(tmp_file, rm_ec);
                return false;
            }
            spdlog::info("更新文件 SHA-256 校验通过: {}", actual_sha256);
        }


        spdlog::info("下载完成, 请稍等, 随后自动完成并(无参)重启..");
        // 完成后创建进程
#if defined(_WIN32) || defined(_WIN64)
        // 用宽字符构造：路径含空格（如 Program Files）、中文或 & 时，不加引号会被 cmd 拆断
        std::wstring p = L"/c ping 127.0.0.1 -n 6 > nul & move /Y \"";
        p += tmp_file.wstring();
        p += L"\" \"";
        p += new_file.wstring();
        p += L"\" & start \"\" /d \"";
        p += parent_dir.wstring();
        p += L"\" \"";
        p += exe_name.wstring();
        p += L"\"";
        const HINSTANCE shell_result = ShellExecuteW(
            NULL,                   // 父窗口句柄
            L"open",                // 操作
            L"cmd.exe",             // 应用程序
            p.c_str(),              // 参数
            parent_dir.c_str(),     // 工作目录
            SW_SHOW);               // 显示方式
        if(reinterpret_cast<std::intptr_t>(shell_result) <= 32) {
            spdlog::error("启动更新进程失败, 请手动替换: {} -> {}", tmp_file.string(), new_file.string());
            return false;
        }

        return true;
#else
        // macOS：校验目标目录可写
        // Linux 同样走该分支：shell 脚本自更新（nohup 后台执行）在 POSIX 下通用
        if(::access(parent_dir.string().c_str(), W_OK) != 0) {
            spdlog::error("程序所在目录不可写, 无法自动更新: {}", parent_dir.string());
            spdlog::info("请手动替换下载的临时文件: {}", tmp_file.string());
            return false;
        }

        // 生成更新脚本：等待旧进程退出后替换并重启
        // 单引号包裹路径；路径本身含单引号时按 POSIX 规则转义为 '\''
        auto sh_quote = [](const std::string& s) -> std::string {
            std::string out = "'";
            for(char c : s) {
                if(c == '\'') {
                    out += "'\\''";
                }
                else {
                    out += c;
                }
            }
            out += "'";
            return out;
        };

        fs::path script_file = tmp_file.string() + ".update.sh";
        std::string script;
        script += "#!/bin/sh\n";
        script += "# GitHubDesktop2Chinese 自动更新脚本（自动生成, 可删除）\n";
        script += "sleep 2\n";                                     // 等待旧进程退出
        script += "mv -f " + sh_quote(tmp_file.string()) + " " + sh_quote(Self.string()) + "\n";
        script += "chmod +x " + sh_quote(Self.string()) + "\n";
        script += "cd " + sh_quote(parent_dir.string()) + "\n";
        script += "exec ./" + exe_name.string() + " --nopause\n";

        std::ofstream sf(script_file, std::ios::binary | std::ios::trunc);
        if(!sf.is_open()) {
            spdlog::error("无法创建更新脚本: {}", script_file.string());
            return false;
        }
        sf.write(script.c_str(), script.size());
        sf.close();
        std::error_code perm_ec;
        fs::permissions(script_file, fs::perms::owner_all | fs::perms::group_read | fs::perms::others_read
                        | fs::perms::group_exec | fs::perms::others_exec,
                        fs::perm_options::replace, perm_ec);
        if(perm_ec) {
            spdlog::warn("设置更新脚本执行权限失败: {}", perm_ec.message());
        }

        // 后台启动脚本，主进程随即退出
        std::string cmd = "nohup sh " + sh_quote(script_file.string()) + " > /dev/null 2>&1 &";
        int rc = std::system(cmd.c_str());
        if(rc != 0) {
            spdlog::error("启动更新脚本失败");
            return false;
        }
        return true;
#endif
    }

    static auto ReadUserInput_string(std::vector<std::string> input, int defaultval = -1) -> std::string {

        while (true)
        {
            std::string instr;
            if (!(std::cin >> instr)) {
                // 标准输入已结束（管道/重定向）：返回默认项，避免死循环占满 CPU
                return input.empty() ? std::string() : input[defaultval < 0 ? 0 : defaultval];
            }

            for (std::string& item : input)
            {
                if (item == instr) {
                    return item;
                }
            }

            if (defaultval == -1) {
                // 循环
                continue;
            }
            else {
                return input[defaultval];
            }
        }
    }

    static auto ReadUserInput_bool(std::vector<std::string> input = {"false", "true"}, int defaultval = -1) -> bool {
        if (input.size() != 2) throw std::runtime_error("读取 bool 类型值时 input 数组长度必须为两个");
        // 不用 std::format：部分 macOS runner 的 libc++ 尚未提供 <format>，
        // 这里只拼一个错误码字符串，保持错误信息内容不变。
        if (defaultval > (int)input.size() - 1 || defaultval < -1) throw std::runtime_error(std::string("defaultval 必须能够指向 input数组，或者为 -1, defaultval:") + std::to_string(defaultval));
        while (true)
        {
            // 输出提示
            if(defaultval == -1)
                spdlog::info("请输入一个表示bool的值({}/{})", input[0], input[1]);
            else
                spdlog::info("请输入一个表示bool的值({}/{} 默认 {})", input[0], input[1], input[defaultval]);

            std::string instr;
            std::cin.clear();
            if(std::cin.rdbuf()->in_avail() > 0) std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::getline(std::cin, instr);

            if (!std::cin && instr.empty()) {
                // 标准输入已结束（管道/重定向）：返回默认值，避免死循环
                return defaultval == 1;
            }

            if (instr == input[0]) {
                return false;
            }
            else if (instr == input[1]) {
                return true;
            }
            else {
                if (defaultval == -1) {
                    // 循环
                    continue;
                }
                else if (defaultval == 0){
                    return false;
                }
                else {
                    return true;
                }
            }
        }
    }
};


#endif // !UTILS_IS_EXITES
