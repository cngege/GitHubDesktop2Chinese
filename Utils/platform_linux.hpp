#ifndef UTILS_PLATFORM_LINUX_HPP
#define UTILS_PLATFORM_LINUX_HPP

// Linux 平台实现
// Linux 无官方 GitHub Desktop，社区 fork（shiftkey/desktop）以 deb / rpm / AppImage / Flatpak / AUR 分发，
// 资源目录（含 main.js / renderer.js / index.html）随安装方式而异：
//   deb / rpm : <安装根>/resources/app          （安装根如 /usr/lib/github-desktop）
//   Flatpak   : <flatpak app>/files/github-desktop/resources/app
//   AppImage  : 单文件，运行时才挂载出资源目录，无法静态发现，需用户通过 -g 手动指定。
// 因此按优先级做多候选扫描，找不到时返回 nullopt，由主程序回退为手动输入。
//
// 控制台能力与 macOS 同为 POSIX（termios / ANSI 转义），实现保持一致。

#include <string>
#include <optional>
#include <filesystem>
#include <system_error>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <vector>

#include <termios.h>
#include <unistd.h>
#include <signal.h>

#include "Utils/utils.hpp"
#include "nlohmann/json.hpp"

namespace platform {

    ////////////////////////////////////////////////////////////////////
    // 控制台
    ////////////////////////////////////////////////////////////////////

    // Linux 终端默认即为 UTF-8，无需设置代码页
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

    // Linux 没有可靠的无窗口修饰键查询接口（无 CoreGraphics 之类的全局事件 API），
    // 直接返回 false；开发者选项仍可通过 -d/--dev 命令行入口进入。
    inline bool is_shift_pressed() {
        return false;
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

    // Linux 同样无法像 Windows 那样拦截窗口关闭并弹窗确认，
    // 仅注册 SIGINT/SIGTERM 打印提示信息。
    inline bool register_close_confirmation() {
        ::signal(SIGINT,  detail::termination_signal_handler);
        ::signal(SIGTERM, detail::termination_signal_handler);
        return true;
    }

    ////////////////////////////////////////////////////////////////////
    // GitHub Desktop 安装发现（多候选扫描）
    ////////////////////////////////////////////////////////////////////

    namespace detail {

        // 安装根目录候选（按优先级）：覆盖 deb / rpm / 手动解压 / 用户级安装 / Flatpak
        inline std::vector<fs::path> install_root_candidates() {
            std::vector<fs::path> roots = {
                "/usr/lib/github-desktop",
                "/usr/lib64/github-desktop",
                "/usr/local/lib/github-desktop",
                "/usr/share/github-desktop",
                "/opt/github-desktop",
                "/var/lib/flatpak/app/io.github.shiftey.Desktop/current/active/files/github-desktop",
            };
            const char* home = ::getenv("HOME");
            if(home != nullptr) {
                fs::path h(home);
                roots.push_back(h / ".local" / "share" / "github-desktop");
                roots.push_back(h / ".local" / "share" / "flatpak" / "app" / "io.github.shiftey.Desktop" / "current" / "active" / "files" / "github-desktop");
            }
            return roots;
        }

        // 判断目录是否为 GitHub Desktop 的资源目录
        inline bool is_resource_dir(const fs::path& dir, std::error_code& ec) {
            return fs::exists(dir / "index.html", ec) && !ec;
        }

        // 从资源目录的 package.json 读取版本号（Electron 应用标准字段）
        inline bool read_app_version(const fs::path& resource_dir, std::string& version) {
            fs::path pkg = resource_dir / "package.json";
            std::error_code ec;
            if(!fs::exists(pkg, ec) || ec) {
                return false;
            }
            std::ifstream fin(pkg, std::ios::binary);
            if(!fin) {
                return false;
            }
            std::string content((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());
            try {
                nlohmann::json j = nlohmann::json::parse(content);
                if(!j.contains("version") || !j["version"].is_string()) {
                    return false;
                }
                version = j["version"].get<std::string>();
            }
            catch(...) {
                return false;
            }
            return !version.empty();
        }

        // 给定安装根，给出资源目录候选（按优先级回退）
        inline std::vector<fs::path> resource_candidates_of(const fs::path& root) {
            std::vector<fs::path> candidates = {
                root / "resources" / "app",
            };
            // 部分打包版本会在 resources 下带版本子目录，做一次目录扫描回退
            fs::path resources = root / "resources";
            std::error_code ec;
            if(fs::is_directory(resources, ec) && !ec) {
                for(const auto& entry : fs::directory_iterator(resources, ec)) {
                    if(ec) break;
                    if(entry.is_directory(ec) && !ec && entry.path().filename().string().rfind("app-", 0) == 0) {
                        candidates.push_back(entry.path() / "resources" / "app");
                    }
                }
            }
            return candidates;
        }

    } // namespace detail

    inline std::optional<GitHubDesktopInstall> find_github_desktop() {
        bool found_install = false; // 存在安装根，但资源目录可能无效
        // 记录发现了安装根但资源目录均无效的位置, 全部候选遍历后再抛出
        std::vector<std::string> invalid_roots;

        for(const fs::path& root : detail::install_root_candidates()) {
            std::error_code ec;
            if(!fs::is_directory(root, ec) || ec) {
                continue;
            }
            found_install = true;

            for(const fs::path& res : detail::resource_candidates_of(root)) {
                if(!detail::is_resource_dir(res, ec) || ec) {
                    continue;
                }

                // 版本号读取失败不视为致命：仍返回安装信息，version 留空
                std::string version;
                detail::read_app_version(res, version);

                GitHubDesktopInstall install;
                install.resource_dir = res;
                install.install_location = root;
                install.version = version;
                install.source = "系统安装目录";
                return install;
            }

            // 当前候选安装根未命中资源目录, 记录后继续尝试后续候选
            invalid_roots.push_back(root.string());
        }

        if(found_install && !invalid_roots.empty()) {
            // 全部候选均未找到可用资源目录：抛出让主程序回退为手动输入
            std::string msg = "发现 GitHub Desktop 安装，但未找到可用的资源目录: ";
            for(size_t i = 0; i < invalid_roots.size(); i++) {
                if(i > 0) msg += ", ";
                msg += invalid_roots[i];
            }
            throw std::runtime_error(msg);
        }

        if(!found_install) {
            // 与 Windows 侧"注册表中未发现GitHubDesktop相关条目"对应的 Linux 提示
            spdlog::warn("系统目录中未发现GitHubDesktop相关条目");
        }
        return std::nullopt;
    }

} // namespace platform

#endif // UTILS_PLATFORM_LINUX_HPP
