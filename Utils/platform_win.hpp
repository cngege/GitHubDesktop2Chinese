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
    // GitHub Desktop 安装发现（注册表）
    ////////////////////////////////////////////////////////////////////

    inline std::optional<GitHubDesktopInstall> find_github_desktop() {
        // 检查注册表中是否存在GithubDesktop
        winreg::RegKey key;
        winreg::RegResult result = key.TryOpen(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\GitHubDesktop");
        if(!result) {
            // 未发现安装条目：打印原有注册表错误信息，返回 nullopt 由主程序回退为手动输入
            spdlog::warn("注册表中未发现GitHubDesktop相关条目, Reg ErrorMessage: {}", utils::to_byte_string(result.ErrorMessage()));
            return std::nullopt;
        }
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
        return install;
    }

} // namespace platform

#endif // UTILS_PLATFORM_WIN_HPP
