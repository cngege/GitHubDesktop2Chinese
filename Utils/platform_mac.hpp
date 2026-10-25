#ifndef UTILS_PLATFORM_MAC_HPP
#define UTILS_PLATFORM_MAC_HPP

// macOS 平台实现
// 用 POSIX / CoreGraphics / Foundation 能力提供与 Windows 版等价的控制台与安装发现能力。

#include <string>
#include <optional>
#include <filesystem>
#include <system_error>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <vector>

#include <termios.h>
#include <unistd.h>
#include <signal.h>

#include <CoreGraphics/CoreGraphics.h>
#include <Foundation/Foundation.h>

#include "Utils/utils.hpp"

namespace platform {

    ////////////////////////////////////////////////////////////////////
    // 控制台
    ////////////////////////////////////////////////////////////////////

    // macOS 终端默认即为 UTF-8，无需设置代码页
    inline void console_init() {}

    inline void set_console_title(const std::string& title) {
        // ANSI OSC 0/2 序列设置窗口标题
        std::printf("\033]0;%s\007", title.c_str());
        std::fflush(stdout);
    }

    inline void clear_screen() {
        // 先清屏并将光标移动到左上角
        std::printf("\033[2J\033[H");
        std::fflush(stdout);
    }

    inline void pause_any_key() {
        // termios 关闭回显与行缓冲，读取单个按键
        std::printf("按任意键继续...");
        std::fflush(stdout);

        struct termios oldt = {};
        struct termios newt = {};
        bool restored = false;
        if(tcgetattr(STDIN_FILENO, &oldt) == 0) {
            newt = oldt;
            newt.c_lflag &= ~(ICANON | ECHO);
            tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        }
        else {
            // 取不到终端属性（例如被重定向），直接按行读取，跳过恢复步骤
            restored = true;
        }

        char c = 0;
        ssize_t n = 0;
        do {
            n = ::read(STDIN_FILENO, &c, 1);
        } while(n == -1 && errno == EINTR);

        if(!restored) {
            tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        }

        std::printf("\n");
        std::fflush(stdout);
    }

    inline bool is_shift_pressed() {
        // CGEventSourceFlagsState 返回当前修饰键状态
        CGEventFlags flags = CGEventSourceFlagsState(kCGEventSourceStateCombinedSessionState);
        return (flags & kCGEventFlagMaskShift) != 0;
    }

    ////////////////////////////////////////////////////////////////////
    // 关闭二次确认
    ////////////////////////////////////////////////////////////////////

    namespace detail {

        inline void termination_signal_handler(int sig) {
            (void)sig;
            const char* msg =
                "\n程序被中止， 汉化可能失败， 请重新运行后按照程序的流程走，汉化完成后会自动关闭，不建议手动关闭窗口\n"
                "开发者:“按任意键继续”是继续的意思，不是已完成的意思\n";
            ssize_t ignored = ::write(STDERR_FILENO, msg, strlen(msg));
            (void)ignored;
            // 恢复默认处理并重新投递，让进程正常退出
            ::signal(sig, SIG_DFL);
            ::raise(sig);
        }

    } // namespace detail

    // macOS 无法像 Windows 那样拦截窗口关闭并弹窗确认，
    // 仅注册 SIGINT/SIGTERM 打印提示信息。
    inline bool register_close_confirmation() {
        ::signal(SIGINT,  detail::termination_signal_handler);
        ::signal(SIGTERM, detail::termination_signal_handler);
        return true;
    }

    ////////////////////////////////////////////////////////////////////
    // GitHub Desktop 安装发现（.app bundle）
    ////////////////////////////////////////////////////////////////////

    namespace detail {

        // 执行命令并捕获标准输出（文本）
        inline bool run_command(const std::string& cmd, std::string& out) {
            std::string buffer;
            buffer.resize(4096);

            FILE* pipe = ::popen(cmd.c_str(), "r");
            if(pipe == nullptr) {
                return false;
            }
            size_t n = 0;
            while((n = ::fread(&buffer[0], 1, buffer.size(), pipe)) > 0) {
                out.append(buffer.c_str(), n); // 按 n 字节追加，避免内部 '\0' 截断
            }
            int status = ::pclose(pipe);
            return status == 0;
        }

        // 从 Info.plist 读取版本号（CFBundleShortVersionString）
        inline bool read_bundle_version(const fs::path& app_path, std::string& version) {
            std::string out;
            std::string cmd = "/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' '"
                            + app_path.string() + "/Contents/Info.plist' 2>/dev/null";
            if(!run_command(cmd, out)) {
                return false;
            }
            // 去除首尾空白
            size_t b = out.find_first_not_of(" \t\r\n");
            if(b == std::string::npos) {
                return false;
            }
            size_t e = out.find_last_not_of(" \t\r\n");
            version = out.substr(b, e - b + 1);
            return !version.empty();
        }

        // 判断目录是否为 GitHub Desktop 的资源目录
        inline bool is_resource_dir(const fs::path& dir, std::error_code& ec) {
            return fs::exists(dir / "index.html", ec) && !ec;
        }

    } // namespace detail

    inline std::optional<GitHubDesktopInstall> find_github_desktop() {
        // 候选安装位置：/Applications 与 ~/Applications
        std::vector<fs::path> candidates = {
            "/Applications/GitHub Desktop.app",
            "/Applications/GitHubDesktop.app",
        };
        const char* home = ::getenv("HOME");
        if(home != nullptr) {
            candidates.push_back(fs::path(home) / "Applications" / "GitHub Desktop.app");
            candidates.push_back(fs::path(home) / "Applications" / "GitHubDesktop.app");
        }

        // 记录发现了 bundle 但资源目录均无效的位置, 全部候选遍历后再抛出
        std::vector<std::string> invalid_bundles;

        bool found_bundle = false;
        for(const fs::path& app : candidates) {
            std::error_code ec;
            if(!fs::is_directory(app, ec) || ec) {
                continue;
            }
            found_bundle = true;

            std::string version;
            if(!detail::read_bundle_version(app, version)) {
                // 发现了 bundle 但读取版本失败：属于致命错误, 立即向上抛出由主程序提示并中止
                throw std::runtime_error("无法读取 GitHub Desktop 的版本信息: " + app.string());
            }

            // 资源目录候选（按优先级回退）
            std::vector<fs::path> resource_candidates = {
                app / "Contents" / "Resources" / "app",
            };
            // 部分 electron 打包版本会在 Resources 下带版本子目录，做一次目录扫描回退
            fs::path resources = app / "Contents" / "Resources";
            if(fs::is_directory(resources, ec) && !ec) {
                for(const auto& entry : fs::directory_iterator(resources, ec)) {
                    if(ec) break;
                    if(entry.is_directory(ec) && !ec && entry.path().filename().string().rfind("app-", 0) == 0) {
                        resource_candidates.push_back(entry.path() / "resources" / "app");
                    }
                }
            }

            for(const fs::path& res : resource_candidates) {
                if(detail::is_resource_dir(res, ec)) {
                    GitHubDesktopInstall install;
                    install.resource_dir = res;
                    install.install_location = app;
                    install.version = version;
                    install.source = "应用程序目录";
                    return install;
                }
            }

            // 当前候选 bundle 未命中资源目录, 记录后继续尝试后续候选
            invalid_bundles.push_back(app.string());
        }

        if(found_bundle && !invalid_bundles.empty()) {
            // 全部候选均未找到可用资源目录：向上抛出由主程序回退为手动输入
            std::string msg = "发现 GitHub Desktop 安装，但未找到可用的资源目录: ";
            for(size_t i = 0; i < invalid_bundles.size(); i++) {
                if(i > 0) msg += ", ";
                msg += invalid_bundles[i];
            }
            throw std::runtime_error(msg);
        }

        if(!found_bundle) {
            // 与 Windows 侧"注册表中未发现GitHubDesktop相关条目"对应的 macOS 提示
            spdlog::warn("应用程序目录中未发现GitHubDesktop相关条目");
        }
        return std::nullopt;
    }

} // namespace platform

#endif // UTILS_PLATFORM_MAC_HPP
