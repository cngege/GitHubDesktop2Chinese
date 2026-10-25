#ifndef UTILS_PLATFORM_HPP
#define UTILS_PLATFORM_HPP

// 跨平台抽象层
// 将平台相关的控制台、安装发现等能力抽象为统一接口，供主程序使用。
//   platform.hpp       平台标识宏与接口声明（不直接包含任何平台专有头文件）
//   platform_win.hpp   Windows 平台实现
//   platform_mac.hpp   macOS 平台实现
//   platform_linux.hpp Linux 平台实现
//
// 约定：实现文件中所有函数均保持与原有 Windows 行为等价，
//       主程序只通过 platform:: 接口访问平台能力，自身不再出现平台专有调用。

#include <string>
#include <optional>
#include <filesystem>

namespace fs = std::filesystem;

//////////////////////////////////////////////////////////////////////////
// 平台标识宏
//////////////////////////////////////////////////////////////////////////
#if defined(_WIN32) || defined(_WIN64)
    #ifndef PLATFORM_WINDOWS
        #define PLATFORM_WINDOWS 1
    #endif
#elif defined(__APPLE__) && defined(__MACH__)
    #ifndef PLATFORM_MACOS
        #define PLATFORM_MACOS 1
    #endif
    #ifndef PLATFORM_UNIX
        #define PLATFORM_UNIX 1
    #endif
#elif defined(__linux__) || defined(__linux)
    #ifndef PLATFORM_LINUX
        #define PLATFORM_LINUX 1
    #endif
    #ifndef PLATFORM_UNIX
        #define PLATFORM_UNIX 1
    #endif
#endif

namespace platform {

    //////////////////////////////////////////////////////////////////////
    // 控制台
    //////////////////////////////////////////////////////////////////////

    // 初始化控制台（Windows 下设置 UTF-8 代码页与虚拟终端处理；macOS 下为 no-op）
    void console_init();

    // 设置控制台窗口标题
    void set_console_title(const std::string& title);

    // 清空控制台屏幕（等价于 Windows 下的 cls）
    void clear_screen();

    // 阻塞等待用户按下任意键（等价于 Windows 下的 pause）
    void pause_any_key();

    // 检测当前 Shift 键是否处于按下状态（用于启动时进入开发者选项）
    bool is_shift_pressed();

    // 注册“关闭窗口前二次确认”的处理。
    // Windows 下注册控制台关闭处理器并弹出确认对话框；返回是否注册成功。
    // macOS 下无法拦截窗口关闭，仅注册信号处理打印提示信息；始终返回 true。
    bool register_close_confirmation();

    //////////////////////////////////////////////////////////////////////
    // GitHub Desktop 安装发现
    //////////////////////////////////////////////////////////////////////

    struct GitHubDesktopInstall {
        fs::path    resource_dir;       // main.js / renderer.js / index.html 所在目录
        fs::path    install_location;   // 安装根目录（Windows 为注册表 InstallLocation，macOS 为 .app 路径）
        std::string version;            // 本地 GitHub Desktop 版本号
        std::string source;             // 发现来源描述（用于日志输出，如 "注册表" / "应用程序目录"）
    };

    // 自动发现本地 GitHub Desktop 安装。
    //  - 返回 nullopt：未发现安装（应回退为手动输入目录）。
    //  - 抛出 std::runtime_error：发现了安装条目但读取其信息失败（致命错误，主程序应中止）。
    std::optional<GitHubDesktopInstall> find_github_desktop();

} // namespace platform


//////////////////////////////////////////////////////////////////////////
// 按平台包含实现
//////////////////////////////////////////////////////////////////////////
#if defined(PLATFORM_WINDOWS)
    #include "Utils/platform_win.hpp"
#elif defined(PLATFORM_MACOS)
    #include "Utils/platform_mac.hpp"
#elif defined(PLATFORM_LINUX)
    #include "Utils/platform_linux.hpp"
#endif

#endif // UTILS_PLATFORM_HPP
