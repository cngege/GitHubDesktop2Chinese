#ifndef UTILS_PLATFORM_WIN_HPP
#define UTILS_PLATFORM_WIN_HPP

// Windows 平台实现
// 行为与原代码完全一致：控制台 UTF-8、虚拟终端、窗口标题、cls、pause、
// Shift 检测、关闭窗口前的二次确认弹窗、注册表安装发现。

#ifndef _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
    #define _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
#endif
#ifndef _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
    #define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
    #define _CRT_SECURE_NO_WARNINGS
#endif

#include <string>
#include <optional>
#include <filesystem>
#include <vector>
#include <cctype>
#include <algorithm>
#include <system_error>

#include <windows.h>
#include <winhttp.h>        // GetSystemProxySettings 需要（沿用原实现）
#include <shellapi.h>       // ShellExecuteW

#include "WinReg/WinReg.hpp"        // 注册表操作库（Windows 安装发现需要）
#include "Utils/utils.hpp"  // GetSystemProxySettings / GetEnvVar 等（Windows 侧）

namespace platform {

    ////////////////////////////////////////////////////////////////////
    // 控制台
    ////////////////////////////////////////////////////////////////////

    inline void console_init() {
        // 设置控制台的输入 输出编码：
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);

        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if(hOut != INVALID_HANDLE_VALUE && hOut != NULL) {
            DWORD mode = 0;
            if(GetConsoleMode(hOut, &mode)) {
                SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
            }
        }
    }

    inline void set_console_title(const std::string& title) {
        SetConsoleTitleA(title.c_str());
    }

    inline void clear_screen() {
        system("cls");
    }

    inline void pause_any_key() {
        system("pause >nul");
    }

    inline bool is_shift_pressed() {
        return (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    }

    ////////////////////////////////////////////////////////////////////
    // 关闭窗口二次确认
    ////////////////////////////////////////////////////////////////////

    namespace detail {
        inline BOOL WINAPI ConsoleHandler(DWORD dwCtrlType) {
            // 仅拦截窗口关闭事件
            if(dwCtrlType == CTRL_CLOSE_EVENT) {
                // 弹出确认对话框（使用 Unicode 字符串）
                int result = MessageBoxW(NULL,
                                         L"程序中止， 汉化可能失败， 请重新运行后按照程序的流程走，汉化完成后会自动关闭，不建议手动关闭窗口\n开发者:“按任意键继续”是继续的意思，不是已完成的意思",
                                         L"汉化程序已关闭，本提示五秒后自动关闭",
                                         MB_OK | MB_ICONQUESTION | MB_DEFBUTTON2);

                if(result == IDOK || result == IDYES) {
                    // 用户确认：执行清理并退出
                    // 注意：ExitProcess 会立即终止进程，不会返回
                    ExitProcess(0);
                }
                else {
                    // 用户取消：阻止默认关闭行为
                    return TRUE;
                }
            }
            // 其他事件（Ctrl+C、Ctrl+Break、关机等）不处理，让系统默认执行
            return FALSE;
        }
    } // namespace detail

    inline bool register_close_confirmation() {
        return SetConsoleCtrlHandler(detail::ConsoleHandler, TRUE) != FALSE;
    }

    ////////////////////////////////////////////////////////////////////
    // GitHub Desktop 安装发现（注册表 + 本地安装目录回退）
    ////////////////////////////////////////////////////////////////////

    namespace detail {

        // 版本目录名按数值比较，避免 "3.6.10" 在字典序上小于 "3.6.8"
        inline std::vector<unsigned long long> version_parts(const std::string& v) {
            std::vector<unsigned long long> parts;
            size_t i = 0;
            while (i < v.size()) {
                if (std::isdigit(static_cast<unsigned char>(v[i]))) {
                    unsigned long long n = 0;
                    while (i < v.size() && std::isdigit(static_cast<unsigned char>(v[i]))) {
                        n = n * 10 + static_cast<unsigned>(v[i] - '0');
                        ++i;
                    }
                    parts.push_back(n);
                }
                else {
                    ++i;
                }
            }
            return parts;
        }

        inline bool version_less(const std::string& a, const std::string& b) {
            const auto pa = version_parts(a);
            const auto pb = version_parts(b);
            const size_t n = std::max(pa.size(), pb.size());
            for (size_t i = 0; i < n; ++i) {
                const auto x = i < pa.size() ? pa[i] : 0ULL;
                const auto y = i < pb.size() ? pb[i] : 0ULL;
                if (x != y) return x < y;
            }
            return a < b;   // 数字段相同时用字典序保证结果稳定
        }

        // 主程序校验 index.html，汉化还需要 main.js / renderer.js
        inline bool is_valid_resource_dir(const fs::path& dir) {
            std::error_code ec;
            return fs::exists(dir / "index.html", ec)
                && fs::exists(dir / "main.js", ec)
                && fs::exists(dir / "renderer.js", ec);
        }

        // 用宽字符环境变量，避免中文用户名路径在窄字符转换时损坏
        inline std::wstring local_app_data() {
            wchar_t buf[32768] = {};
            const DWORD n = ::GetEnvironmentVariableW(L"LOCALAPPDATA", buf,
                                                      static_cast<DWORD>(sizeof(buf) / sizeof(buf[0])));
            if (n == 0 || n >= sizeof(buf) / sizeof(buf[0])) return {};
            return std::wstring(buf, n);
        }

        // 注册表不可用时的回退：扫描 %LOCALAPPDATA%\GitHubDesktop\app-<版本>\resources\app
        // 选择版本号最新且资源完整的目录（Squirrel 会保留旧版本目录，不能随便取第一个）
        inline std::optional<GitHubDesktopInstall> scan_local_app_dirs() {
            const std::wstring local = local_app_data();
            if (local.empty()) return std::nullopt;

            const fs::path root = fs::path(local) / L"GitHubDesktop";
            std::error_code ec;
            if (!fs::is_directory(root, ec)) return std::nullopt;

            std::string best_ver;
            fs::path best_dir;
            for (fs::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
                const fs::path entry = it->path();
                const std::string name = entry.filename().string();
                if (name.rfind("app-", 0) != 0) continue;
                if (!fs::is_directory(entry, ec)) continue;

                const fs::path resource = entry / "resources" / "app";
                if (!is_valid_resource_dir(resource)) continue;

                const std::string ver = name.substr(4);
                if (best_dir.empty() || version_less(best_ver, ver)) {
                    best_ver = ver;
                    best_dir = resource;
                }
            }
            if (best_dir.empty()) return std::nullopt;

            GitHubDesktopInstall install;
            install.resource_dir     = best_dir;
            install.install_location = root;
            install.version          = best_ver;
            install.source           = "本地程序目录";
            return install;
        }

    } // namespace detail

    inline std::optional<GitHubDesktopInstall> find_github_desktop() {
        // 这里只读取 DisplayVersion / InstallLocation，不需要写权限。
        // WinReg::TryOpen 默认请求 KEY_READ|KEY_WRITE|KEY_WOW64_64KEY，
        // 在受限令牌或被安全策略拦截时会返回「拒绝访问」，使明明存在的
        // 安装条目被误判为未安装。显式改为只读。
        winreg::RegKey key;
        winreg::RegResult result = key.TryOpen(
            HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\GitHubDesktop",
            KEY_READ | KEY_WOW64_64KEY);

        if (result) {
            GitHubDesktopInstall install;
            install.source = "注册表";
            try {
                // 首先要从注册表中拿到GithubDesktop相关信息
                std::wstring ver = key.GetStringValue(L"DisplayVersion");
                std::wstring path = key.GetStringValue(L"InstallLocation");
                install.install_location = path;
                install.resource_dir = fs::path(path + L"\\" + L"app-" + ver + L"\\resources\\app");
                install.version = utils::to_byte_string(ver);
            }
            catch(const winreg::RegException& regerr) {
                // 发现了安装条目但读取失败，属于致命错误，向上抛出由主程序中止
                // 保留原有 Windows 错误提示内容（键名 + RegException 描述 + 错误码）
                key.Close();
                throw std::runtime_error(std::string("可能注册表key DisplayVersion 或 InstallLocation 不存在,请检查注册表目录 Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\GitHubDesktop ; RegException ")
                    + regerr.what() + " ,ErrCode:" + std::to_string(regerr.code().value()));
            }
            key.Close();

            if (detail::is_valid_resource_dir(install.resource_dir)) {
                return install;
            }
            spdlog::warn("注册表指向的资源目录无效: {}", install.resource_dir.string());
        }
        else {
            spdlog::warn("注册表中未发现GitHubDesktop相关条目, Reg ErrorMessage: {}",
                         utils::to_byte_string(result.ErrorMessage()));
        }

        // 注册表读不到、或拼出的目录无效时，回退扫描本地安装目录，避免要求手动输入
        auto scanned = detail::scan_local_app_dirs();
        if (scanned) {
            spdlog::info("已从本地程序目录发现 GitHub Desktop {}: {}",
                         scanned->version, scanned->resource_dir.string());
            return scanned;
        }
        return std::nullopt;
    }

} // namespace platform

#endif // UTILS_PLATFORM_WIN_HPP
