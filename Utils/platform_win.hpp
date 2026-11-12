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
#include <conio.h>          // _getch：不经过 cmd.exe 的按键等待
#include <winhttp.h>        // GetSystemProxySettings 需要（沿用原实现）
#include <shellapi.h>       // ShellExecuteW

#include "WinReg/WinReg.hpp"        // 注册表操作库（Windows 安装发现需要）
#include "Utils/utils.hpp"  // GetSystemProxySettings / GetEnvVar 等（Windows 侧）
#include "Utils/elevate.hpp" // 提权相关的纯逻辑（命令行参数转义）

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
        // 直接用控制台 API 清屏：system("cls") 会额外拉起 cmd.exe（慢，且常被杀软/策略拦截）
        HANDLE hOut = ::GetStdHandle(STD_OUTPUT_HANDLE);
        if(hOut == INVALID_HANDLE_VALUE || hOut == nullptr) {
            return;
        }
        CONSOLE_SCREEN_BUFFER_INFO info{};
        if(!::GetConsoleScreenBufferInfo(hOut, &info)) {
            return;     // 输出被重定向（无控制台缓冲区）时静默跳过
        }
        const DWORD cell_count =
            static_cast<DWORD>(info.dwSize.X) * static_cast<DWORD>(info.dwSize.Y);
        const COORD home{ 0, 0 };
        DWORD written = 0;
        ::FillConsoleOutputCharacterW(hOut, L' ', cell_count, home, &written);
        ::FillConsoleOutputAttribute(hOut, info.wAttributes, cell_count, home, &written);
        ::SetConsoleCursorPosition(hOut, home);
    }

    inline void pause_any_key() {
        // 直接读控制台（不经过 cmd.exe）。
        // 注意：_getch 会绕过 stdin 重定向，若在管道/CI 场景下调用会永久阻塞，
        // 因此 stdin 不是控制台时立即返回（等价于 pause 读到 EOF 的行为）。
        HANDLE hIn = ::GetStdHandle(STD_INPUT_HANDLE);
        if(hIn == INVALID_HANDLE_VALUE || hIn == nullptr) {
            return;
        }
        DWORD mode = 0;
        if(::GetConsoleMode(hIn, &mode) == 0) {
            return;
        }
        ::_getch();
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

    ////////////////////////////////////////////////////////////////////
    // 管理员提权（UAC）
    ////////////////////////////////////////////////////////////////////

    namespace detail {

        // 当前进程令牌是否已提升（UAC 提升后的管理员令牌）。
        // 用 TokenElevation 而不是 CheckTokenMembership：
        // 后者在"管理员用户但未提升"（UAC 过滤后的令牌）时同样返回 TRUE，
        // 会把"已提权"误判成"没提权"，从而反复弹 UAC。
        inline bool token_is_elevated() {
            HANDLE token = nullptr;
            if(!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token)) {
                return false;
            }
            TOKEN_ELEVATION elevation{};
            DWORD returned = 0;
            const BOOL ok = ::GetTokenInformation(token, TokenElevation, &elevation,
                                                  sizeof(elevation), &returned);
            ::CloseHandle(token);
            return ok && elevation.TokenIsElevated != 0;
        }

        // 当前可执行文件的完整路径（宽字符）
        inline std::wstring current_module_path() {
            std::vector<wchar_t> buf(MAX_PATH, L'\0');
            for(;;) {
                const DWORD len = ::GetModuleFileNameW(nullptr, buf.data(),
                                                      static_cast<DWORD>(buf.size()));
                if(len == 0) {
                    return {};
                }
                if(len < buf.size()) {
                    return std::wstring(buf.data(), len);
                }
                // 路径超出当前缓冲区：扩容后重试（最长 32767）
                if(buf.size() >= 32768) {
                    return {};
                }
                buf.resize(buf.size() * 2);
            }
        }

        // 当前工作目录（宽字符）
        inline std::wstring current_working_directory() {
            std::vector<wchar_t> buf(MAX_PATH, L'\0');
            for(;;) {
                const DWORD len = ::GetCurrentDirectoryW(static_cast<DWORD>(buf.size()), buf.data());
                if(len == 0) {
                    return {};
                }
                if(len < buf.size()) {
                    return std::wstring(buf.data(), len);
                }
                if(buf.size() >= 32768) {
                    return {};
                }
                buf.resize(buf.size() * 2);
            }
        }

        // 把原始命令行中的参数（不含 argv[0]）逐个转义后重新拼接。
        // 用 CommandLineToArgvW 而不是手工截取字符串：手工截取无法正确处理
        // 参数里的引号/反斜杠组合，会把用户传入的路径改坏。
        inline std::vector<std::wstring> original_arguments() {
            std::vector<std::wstring> args;
            int argc = 0;
            LPWSTR* argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
            if(argv == nullptr) {
                return args;
            }
            for(int i = 1; i < argc; ++i) {
                args.emplace_back(argv[i]);
            }
            ::LocalFree(argv);
            return args;
        }

        // 从原始命令行中取出 "--elevated-parent-pid <pid>" 的值。
        // 提权子进程需要在初始化控制台之前就接管父进程的控制台，
        // 那时 CLI11 还没解析参数，只能自己扫一遍原始命令行。
        inline int peek_elevated_parent_pid() {
            const auto args = detail::original_arguments();
            for(std::size_t i = 0; i + 1 < args.size(); ++i) {
                if(args[i] != L"--elevated-parent-pid") {
                    continue;
                }
                try {
                    const int pid = std::stoi(args[i + 1]);
                    return (pid > 0) ? pid : 0;
                }
                catch(...) {
                    return 0;
                }
            }
            return 0;
        }

        // 以 "runas" 触发 UAC 提权重新启动自身，并等待子进程结束
        inline std::optional<int> run_as_admin(const ElevationRequest& req) {
            const std::wstring exe_path = detail::current_module_path();
            if(exe_path.empty()) {
                spdlog::error("无法获取当前程序路径, 提权失败");
                return std::nullopt;
            }

            // 提权相关参数放在最前面：CLI11 中子命令之后的参数会被子命令吞掉，
            // 放在前面才能保证根级参数被子进程正确解析。
            std::vector<std::wstring> params;
            // 标记这是提权后的子进程：避免再次尝试提权造成无限重启
            params.push_back(L"--elevated-child");
            // 把父进程已解析出的资源目录传给子进程，省去重复发现/重复询问
            // 路径走十六进制编码：命令行参数会经过 ANSI 代码页转换，中文路径会被破坏
            if(!req.base_dir.empty()) {
                params.push_back(L"--elevated-base");
                // 十六进制文本是纯 ASCII，转宽字符不会丢信息
                params.push_back(utils::to_wide_string(utils::encode_path_hex(req.base_dir)));
            }
            // 父进程 PID：子进程据此接管父进程的控制台窗口
            if(req.parent_pid > 0) {
                params.push_back(L"--elevated-parent-pid");
                params.push_back(std::to_wstring(req.parent_pid));
            }
            // 父进程在"开发者选项"菜单里交互出来的结果只存在内存中，
            // 提权重启会丢失，因此编码后传给子进程直接沿用
            if(!req.dev_options.empty()) {
                params.push_back(L"--elevated-dev-options");
                params.push_back(utils::to_wide_string(req.dev_options));
            }
            // 父进程对"是否自动更新"的选择：子进程不再重复询问
            if(req.update_decision >= 0) {
                params.push_back(L"--elevated-update");
                params.push_back(std::to_wstring(req.update_decision));
            }
            // 其余原始参数原样保留（--nopause / -g / -j / dev 子命令等）
            for(const auto& arg : detail::original_arguments()) {
                params.push_back(arg);
            }

            std::wstring cmdline;
            for(const auto& p : params) {
                if(!cmdline.empty()) {
                    cmdline += L' ';
                }
                cmdline += elevate::quote_windows_arg(p);
            }

            const std::wstring work_dir = detail::current_working_directory();

            SHELLEXECUTEINFOW sei{};
            sei.cbSize = sizeof(sei);
            // NOCLOSEPROCESS: 需要等待提权后的进程结束并取回退出码
            sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
            sei.lpVerb = L"runas";
            sei.lpFile = exe_path.c_str();
            sei.lpParameters = cmdline.empty() ? nullptr : cmdline.c_str();
            // 保持工作目录不变：localization.json 等相对路径的查找依赖它
            sei.lpDirectory = work_dir.empty() ? nullptr : work_dir.c_str();
            // 隐藏子进程自己那个控制台窗口：它随后会 FreeConsole 并接管父进程的控制台，
            // 避免用户看到一闪而过的第二个黑窗口（接管失败时会重新分配可见控制台）
            sei.nShow = SW_HIDE;

            if(!::ShellExecuteExW(&sei)) {
                const DWORD err = ::GetLastError();
                if(err == ERROR_CANCELLED) {
                    spdlog::warn("用户取消了管理员权限请求(UAC)");
                }
                else {
                    spdlog::error("以管理员身份重新启动失败, 错误码 {} ({})",
                                   err, utils::SysErrorText(err));
                }
                return std::nullopt;
            }

            if(sei.hProcess == nullptr) {
                return std::nullopt;
            }

            // 等待提权后的进程执行完毕，把它的退出码带回给调用方
            ::WaitForSingleObject(sei.hProcess, INFINITE);
            DWORD exit_code = 0;
            const BOOL got_code = ::GetExitCodeProcess(sei.hProcess, &exit_code);
            ::CloseHandle(sei.hProcess);
            if(!got_code) {
                return std::nullopt;
            }
            return static_cast<int>(exit_code);
        }

    } // namespace detail

    inline bool is_elevated() {
        return detail::token_is_elevated();
    }

    inline bool supports_auto_elevation() {
        return true;
    }

    inline std::optional<int> relaunch_elevated(const ElevationRequest& req) {
        return detail::run_as_admin(req);
    }

    inline void attach_parent_console(int parent_pid) {
        if(parent_pid <= 0) {
            return;
        }
        // 提权子进程默认会被分配一个新的控制台窗口（父进程是 AppInfo 服务，不带控制台）。
        // 直接 AttachConsole 会因"进程已附着在控制台上"而失败，
        // 因此失败时先释放自己的控制台再接管父进程的，避免用户看到两个黑窗口。
        if(!::AttachConsole(static_cast<DWORD>(parent_pid))) {
            ::FreeConsole();
            if(!::AttachConsole(static_cast<DWORD>(parent_pid))) {
                // 父进程可能已经退出：重新申请一个控制台，保证输出不丢失
                if(::AllocConsole()) {
                    ::ShowWindow(::GetConsoleWindow(), SW_SHOW);
                }
                return;
            }
        }
        // 接管后需要重新绑定标准流到该控制台，否则仍写到原来的管道/重定向
        FILE* dummy = nullptr;
        ::freopen_s(&dummy, "CONOUT$", "w", stdout);
        ::freopen_s(&dummy, "CONOUT$", "w", stderr);
        ::freopen_s(&dummy, "CONIN$", "r", stdin);
    }

    inline int peek_elevated_parent_pid() {
        return detail::peek_elevated_parent_pid();
    }

    inline int current_process_id() {
        return static_cast<int>(::GetCurrentProcessId());
    }

    inline std::string elevation_hint() {
        return "右键以管理员身份运行本程序（或允许 UAC 提权请求）";
    }

} // namespace platform

#endif // UTILS_PLATFORM_WIN_HPP
