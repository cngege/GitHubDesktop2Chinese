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
#include <vector>

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
    //  - Windows：优先读注册表；只读方式打开，失败或拼出的资源目录无效时
    //    回退扫描 %LOCALAPPDATA%\GitHubDesktop\app-<版本>\resources\app。
    //  - macOS / Linux：扫描 .app bundle / 系统安装目录的多个候选位置。
    //  - 返回 nullopt：未发现安装，或发现的安装没有可用资源目录
    //    （两种情况都应由主程序回退为手动输入目录）。
    //  - 抛出 std::runtime_error：仅表示"发现了安装条目但读取其信息失败"这类
    //    致命错误（例如 Windows 注册表键缺少必要值），主程序应中止。
    std::optional<GitHubDesktopInstall> find_github_desktop();

    //////////////////////////////////////////////////////////////////////
    // 管理员提权（Windows UAC）
    //////////////////////////////////////////////////////////////////////

    // 提权重启时需要传递给子进程的信息
    struct ElevationRequest {
        fs::path base_dir;         // 已解析出的资源目录，透传给子进程，避免重复发现/重复询问
        int parent_pid = 0;        // 父进程 PID：子进程据此复用父进程的控制台窗口
        std::string dev_options;   // 父进程的开发者选项(逗号分隔)，子进程直接沿用，不再进一次菜单
        int update_decision = -1;  // 父进程对"是否自动更新"的选择(-1=未询问 0=不更新)
    };

    // 当前进程是否已经以管理员（提升）身份运行。
    // Windows: 进程令牌已提升（TokenElevation），管理员用户但未提升时为 false；
    // macOS/Linux: euid == 0。
    bool is_elevated();

    // 当前平台是否支持程序内自动提权。
    // Windows: 支持（UAC 弹窗，用户确认即可）；macOS/Linux: 不支持（需要 sudo 手动执行）。
    bool supports_auto_elevation();

    // 以管理员身份重新启动当前程序：
    //   - 保留原始命令行参数，并追加提权相关参数；
    //   - 等待提权后的进程执行完毕，返回它的退出码；
    //   - 无法提权（平台不支持 / 用户取消 UAC）时返回 std::nullopt。
    std::optional<int> relaunch_elevated(const ElevationRequest& req);

    // 让提权后的子进程接管父进程的控制台（仅 Windows 有意义，其它平台为空实现）。
    // 接管后子进程的输出会显示在父进程的原窗口中，不会弹出第二个黑窗口。
    void attach_parent_console(int parent_pid);

    // 在解析命令行之前，从原始命令行中提前取出父进程 PID。
    // 提权子进程需要在初始化控制台之前就接管控制台，因此不能只依赖 CLI 解析结果。
    int peek_elevated_parent_pid();

    // 当前进程 PID
    int current_process_id();

    // 提权失败时给用户的操作提示（各平台措辞不同）。
    std::string elevation_hint();

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
