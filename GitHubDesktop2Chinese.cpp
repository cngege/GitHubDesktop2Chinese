// GitHubDesktop2Chinese.cpp: 定义应用程序的入口点。


#define _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING   //消除 converter.to_bytes的警告
#define _CRT_SECURE_NO_WARNINGS                             //消除 sprintf的警告

// 按任意键暂停（Windows 下 pause，macOS 下 termios 单键读取）
#define PAUSE if(!no_pause) { spdlog::info("按任意键继续..."); platform::pause_any_key(); }

#include "GitHubDesktop2Chinese.h"
#include <string>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cctype>

#include <regex>

#include "spdlog/spdlog.h"          // 日志式输出库
#include "nlohmann/json.hpp"        // JSON读取本地配置库
#include "Utils/platform.hpp"       // 跨平台抽象层（控制台 / 安装发现 / Shift 检测 / 关窗确认）

#if defined(PLATFORM_WINDOWS)
    #include "WinReg/WinReg.hpp"    // 注册表操作库（Windows 安装发现由 platform 层封装，此头仅供历史依赖保留）
#endif

#include <CLI/CLI.hpp>              // 参数管理器:   https://github.com/CLIUtils/CLI11
#include "Utils/utils.hpp"
#include "Utils/localization_text.hpp"
#include "VersionParse/Version.hpp"

#if defined(PLATFORM_WINDOWS)
    #pragma comment(lib, "winhttp.lib")
#endif

std::Version FileVer{0,0,0};


using json = nlohmann::json;

// 设置一个路径的全局变量  指向要修改JS的目录
fs::path Base;
fs::path LocalizationJSON;

bool no_pause;                                  // 程序在结束前是否暂停
bool only_read_from_remote;                     // 仅从远程url中读取本地化文件
bool rollback;                                  // 从备份中还原汉化前的文件
bool enable_proxy;                              // 使用代理访问
bool insecure_tls;                              // 跳过 HTTPS 证书校验（仅企业中间人代理等场景）

json localization = R"(
                        {
                            "main": [
                                ["",""]
                            ],
                            "main_dev": [
                                ["",""]
                            ],
                            "renderer": [
                                ["",""]
                            ],
                            "renderer_dev": [
                                ["",""]
                            ]
                        }
                    )"_json;
bool _debug_goto_devoptions;

bool _debug_error_check_mode_main = false;
bool _debug_error_check_mode_renderer = false;
bool _debug_invalid_check_mode = false;
bool _debug_no_replace_res = false;
bool _debug_translation_from_bak = false;		// 直接从备份文件中翻译到目标文件中
bool _debug_dev_replace = false;				// 开发模式替换
bool _debug_dev_setversion = false;				// 开发模式可以指定当前版本

std::optional<std::string> formatTime(std::string time_str);

namespace {

// 处理单个 js 文件时需要的描述信息与日志标签。
// 标签逐项给出是为了保持 main.js / renderer.js 两侧原有的日志措辞完全不变
// （历史上 renderer 侧的第三项检测就写作 "[renderer]" 而不是 "[renderer 3]"）。
struct JsFileTags {
    const char* file_name;      // "main.js"
    const char* section;        // "main"
    const char* section_dev;    // "main_dev"
    const char* item_tag;       // "[main]"
    const char* item3_tag;      // "[main 3]"
    const char* select_tag;     // "[select main]"
    const char* select3_tag;    // "[select main 3]"
    bool error_check_mode;      // 错误检查模式：逐条写入并询问写多少条
};

// 复制文件并覆盖目标。返回错误码而不是抛异常：
// 备份/还原属于"尽力而为"的辅助流程，不应因单个文件失败把程序打崩。
std::error_code copy_file_overwrite(const fs::path& from, const fs::path& to) {
    std::error_code ec;
    fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
    return ec;
}

// 解析远程下发的 localization.json。
// 远程内容可能被代理/门户替换成 HTML 或截断，格式非法时必须返回失败而不是抛出。
bool parse_localization_from_remote(const std::string& text) {
    try {
        json parsed = json::parse(text);
        if(!parsed.is_object()) {
            return false;
        }
        localization = std::move(parsed);
        return true;
    }
    catch(const std::exception&) {
        return false;
    }
}

/**
 * @brief 对单个 js 文件应用 localization 中的替换规则
 * @param file_path 目标文件（写回位置）
 * @param tags 文件相关的日志标签与差异项
 * @param failure_count 失效项/失败计数（与原有退出码语义一致）
 * @return 是否成功（读取失败或写入失败返回 false，文件保持原样）
 */
bool process_js_file(const fs::path& file_path, const JsFileTags& tags, int& failure_count) {
    const fs::path bak_path = fs::path(file_path).concat(".bak");

    // 开发者选项：优先从备份文件读取（保证多次替换都基于原始内容）
    std::error_code exists_ec;
    const bool has_bak = fs::exists(bak_path, exists_ec) && !exists_ec;
    const bool prefer_bak = (_debug_translation_from_bak || _debug_invalid_check_mode) && has_bak;
    const fs::path source_path = prefer_bak ? bak_path : file_path;

    bool read_ok = false;
    std::string text = utils::ReadFile(source_path.string(), &read_ok);
    if(!read_ok || text.empty()) {
        // 读不到内容时绝不能继续：把空串写回等于直接销毁 main.js
        spdlog::error("{} 读取失败或内容为空({}), 已跳过本次处理, 文件保持原样.",
                      tags.file_name, source_path.string());
        return false;
    }

    const char* section = _debug_dev_replace ? tags.section_dev : tags.section;
    int out = 0;

    try {
        // 普通替换项
        if(localization.contains(section) && localization.at(section).is_array()) {
            for(auto& item : localization[section].items()) {
                if(!item.value().is_array() || item.value().size() < 2) {
                    spdlog::warn("{} 规则格式无效(需要 [匹配, 替换] 两项), 已跳过", tags.item_tag);
                    continue;
                }
                std::string rege = item.value()[0].get<std::string>();
                if(rege.empty() || rege == "\"\"") {
                    continue;
                }
                std::regex pattern(rege);

                // 开发者选项 失效检测
                if(_debug_invalid_check_mode) {
                    if(!std::regex_search(text, pattern)) {
                        spdlog::warn("{} 检测到失效项: {}", tags.item_tag, rege);
                        ++failure_count;
                    }
                    if(item.value().size() >= 3) {
                        std::regex pattern3(item.value()[2].get<std::string>());
                        if(!std::regex_search(text, pattern3)) {
                            spdlog::warn("{} 检测到失效项: {}", tags.item3_tag, item.value()[2].get<std::string>());
                            ++failure_count;
                        }
                    }
                    continue;
                }

                if(item.value().size() >= 3) {
                    // 用数组第三项定位关键字，把捕获组填进 #{n} 占位符
                    const std::string regex_str = item.value()[2].get<std::string>();
                    std::regex pattern3(regex_str);
                    std::sregex_iterator it(text.begin(), text.end(), pattern3);
                    if(it != std::sregex_iterator()) {
                        item.value()[1] = localization_text::substitute_capture_groups(item.value()[1].get<std::string>(), *it);
                    }
                    else {
                        // 找不到关键字就跳过，避免把错误的字符插进文件导致程序打不开
                        spdlog::warn("{} 出现一处失效项,此项将跳过: {}", tags.item_tag, regex_str);
                        continue;
                    }
                }

                // 先 regex_search 预判：不匹配时 regex_replace 也会整段复制，
                // 900+ 条规则逐条复制整个文件是本程序最耗时的地方。
                if(std::regex_search(text, pattern)) {
                    text = std::regex_replace(text, pattern, item.value()[1].get<std::string>());
                }
                if(tags.error_check_mode) {
                    spdlog::info("{}[out:{}]已经替换:{}->{}", tags.item_tag, out, rege, utils::utf8ToAnsi(item.value()[1].get<std::string>()));
                    --out;
                    if(out <= 0) {
                        utils::WriteFile(file_path, text);
                        spdlog::info("已写入. 你希望下次替换多少条后写入:");
                        std::cin >> out;
                    }
                }
            }
        }

        // select 分组（需要用户确认的可选项）
        if(!_debug_dev_replace && localization.contains("select") && localization.at("select").is_array()) {
            for(auto& item_select : localization["select"].items()) {
                const auto& sel = item_select.value();
                if(!sel.is_object() || !sel.contains("replaceFile") || !sel.contains("enable")) {
                    spdlog::warn("{} select 条目缺少 replaceFile/enable 字段, 已跳过", tags.select_tag);
                    continue;
                }
                // 判断此项是否是当前 js，并且 enable 项是否开启
                if(sel["replaceFile"].get<std::string>() != tags.file_name || !sel["enable"].get<bool>()) {
                    continue;
                }
                if(!sel.contains("replace") || !sel["replace"].is_array()) {
                    spdlog::warn("{} select 条目缺少 replace 数组, 已跳过", tags.select_tag);
                    continue;
                }
                std::vector<std::vector<std::string>> replaces = sel["replace"].get<std::vector<std::vector<std::string>>>();
                if(_debug_invalid_check_mode) {
                    for(auto& v_item : replaces) {
                        if(v_item.empty()) {
                            continue;
                        }
                        std::string rege = v_item[0];
                        if(rege.empty() || rege == "\"\"") {
                            continue;
                        }
                        std::regex pattern(rege);
                        if(!std::regex_search(text, pattern)) {
                            spdlog::warn("{} 检测到失效项: {}", tags.select_tag, rege);
                            ++failure_count;
                        }
                        // 二层数组字符串有三项时，第三项也要检测
                        if(v_item.size() >= 3) {
                            std::regex pattern3(v_item[2]);
                            if(!std::regex_search(text, pattern3)) {
                                spdlog::warn("{} 检测到失效项: {}", tags.select3_tag, v_item[2]);
                                ++failure_count;
                            }
                        }
                    }
                }
                else {
                    // 询问提示 输出 json 中的提示字符串
                    spdlog::info(">>>>>> {}", sel.value("tooltip", std::string()).c_str());
                    if(utils::ReadUserInput_bool({ "n","y" }, 1)) {
                        for(auto& v_item : replaces) {
                            if(v_item.empty()) {
                                continue;
                            }
                            std::string rege = v_item[0];
                            if(rege.empty() || rege == "\"\"") {
                                continue;
                            }
                            std::regex pattern(rege);
                            if(v_item.size() >= 3) {
                                std::regex pattern3(v_item[2]);
                                std::sregex_iterator it(text.begin(), text.end(), pattern3);
                                if(it != std::sregex_iterator()) {
                                    v_item[1] = localization_text::substitute_capture_groups(v_item[1], *it);
                                }
                                else {
                                    spdlog::warn("{} 出现一处失效项,此项将跳过: {}", tags.select3_tag, v_item[2]);
                                    continue;
                                }
                            }
                            if(std::regex_search(text, pattern)) {
                                text = std::regex_replace(text, pattern, v_item[1]);
                            }
                        }
                    }
                }
            }
        }
    }
    catch(const std::regex_error& re) {
        spdlog::error("处理{}时匹配正则表达式时出现错误 code:{}, message:{}, LINE: {}", tags.file_name, re.code(), re.what(), __LINE__);
    }
    catch(const std::exception& e) {
        // 包含 nlohmann::json 的 type_error/out_of_range（它们不继承 std::runtime_error）
        spdlog::error("处理{}时出现错误 message:{}, LINE: {}", tags.file_name, e.what(), __LINE__);
    }

    if(_debug_invalid_check_mode || _debug_no_replace_res) {
        spdlog::info("{} 文件汉化结束.", tags.file_name);
        return true;
    }
    if(!utils::WriteFile(file_path, text)) {
        spdlog::error("{} 汉化未完成: 写入失败, 该文件保持原样.", tags.file_name);
        return false;
    }
    spdlog::info("{} 文件汉化结束.", tags.file_name);
    return true;
}

} // namespace


// argv[0] 是程序路径
int main(int argc, char* argv[])
{
    // 初始化控制台（Windows: 设置 UTF-8 代码页与虚拟终端处理；macOS: 默认 UTF-8，无需设置）
    platform::console_init();


    FileVer = std::Version(FILEVERSION);
    // 设置控制台打印日志输出等级
    if(FileVer.status == std::Version::Dev) {
        spdlog::set_level(spdlog::level::debug);
    }
    // 注册命令行
    {
        CLI::App app{ "汉化GitHub Desktop管理、替换资源" };
        //app.require_subcommand(1);
        // 子命令 开发者选项
        auto dev_cmd = app.add_subcommand("dev", "开发者选项");
        dev_cmd->add_flag("-d,--dev", _debug_goto_devoptions,                       "进入开发者选项调整功能(可在开启程序时按住shift直接进入)");
        dev_cmd->add_flag("--mainerrorcheck", _debug_error_check_mode_main,         "[错误检查模式]对main.js以错误检查模式进行排查");
        dev_cmd->add_flag("--rendererrorcheck", _debug_error_check_mode_renderer,   "[错误检查模式]对renderer.js以错误检查模式进行排查");
        dev_cmd->add_flag("--invalidcheck", _debug_invalid_check_mode,              "[检测失效项]对本地化文件中的失效替换项进行检测");
        dev_cmd->add_flag("--noreplaceres", _debug_no_replace_res,                  "[资源不替换]开启后不会对资源进行替换,但不会阻止[错误检查模式]");
        dev_cmd->add_flag("--translationfrombak", _debug_translation_from_bak,      "[从备份文件中读取替换]优先从备份文件中读取js文件内容进行替换,开启[检测失效项]时建议开启此项");
        dev_cmd->add_flag("--devreplace", _debug_dev_replace,                       "[开发模式替换]仅替换指定映射以节约汉化时间(会影响其他项)");
        if(FileVer.major == 0 && FileVer.minor == 0 && FileVer.revision == 0) {
            dev_cmd->add_flag("--devsetver", _debug_dev_setversion,                "[指定当前程序版本]输入一个版本信息可以指定当前程序版本");
        }

        app.add_flag("--nopause", no_pause,                         "程序在结束前不再暂停等待");
        app.add_option("-g,--githubdesktoppath", Base,              "指定GitHubDesktop要汉化的资源所在目录(js所在目录)");
        app.add_option("-j,--json", LocalizationJSON,               "指定本地化JSON文件的本地路径");
        //app.add_flag("-p,--enableproxy", enable_proxy,              "开启代理访问GitHub");
        app.add_flag("-r,--onlyfromremote", only_read_from_remote,  "仅从远程url中读取本地化文件");
        app.add_flag("--insecure-tls", insecure_tls,                "跳过HTTPS证书校验(仅在无法通过校验的企业代理环境下使用)");
        app.add_flag("--rollback", rollback,                        "从备份文件中还原汉化前的文件");
        
        app.callback([&]() {
            // 手动指定了本地化文件目录
            if (!LocalizationJSON.string().empty()) {
                if (!LocalizationJSON.string().ends_with(".json")) {
                    throw CLI::ValidationError("(-j,--json) 指定的本地化文件路径必须以.json结尾");
                }
                if (!fs::exists(LocalizationJSON)) {
                    std::ofstream io(LocalizationJSON);
                    io << std::setw(4) << localization << std::endl;
                    io.close();
                    throw CLI::ValidationError("(-j,--json) 指定的本地化文件不存在,已在指定位置创建");
                }
            }
            // 手动指定了GitHubDesktop资源文件目录
            if (!Base.string().empty()) {
                if (!fs::exists(Base)) {
                    throw CLI::ValidationError("(-g,--githubdesktoppath) 指定的资源文件目录不存在");
                }
                if (!fs::exists(Base / "index.html")) {
                    throw CLI::ValidationError("(-g,--githubdesktoppath) 指定的资源文件目录无效,该目录下应该是存放main.js和renderer.js文件的");
                }
            }
        });


        CLI11_PARSE(app, argc, argv);
    }

    // HTTPS 证书校验默认开启（自更新会下载并执行二进制）。
    // 只有显式指定 --insecure-tls 时才关闭，并留下明确告警。
    utils::tls_verification_enabled() = !insecure_tls;
    if(insecure_tls) {
        spdlog::warn("已跳过 HTTPS 证书校验(--insecure-tls): 传输内容可能被中间人篡改, 请在确认网络环境后使用");
    }
    if(platform::is_shift_pressed() || _debug_goto_devoptions) {
        // 如果Shift按下, 则进入开发者选项
        platform::set_console_title("开发者模式");
        spdlog::info("您已进入开发者模式");
        DeveloperOptions();
    }

    if(_debug_dev_setversion) {
        for(;;) {
            spdlog::info("请输入版本 如 1.2.3 (exit强制跳出):");
            std::string instr;
            std::cin >> instr;
            if(instr == "exit") {
                break;
            }
            auto ver = std::Version(instr.c_str());
            if(!ver) {
                spdlog::error("你输入的版本无效");
            }
            else {
                FileVer = ver;
                platform::clear_screen();
                break;
            }
        }
    }
    
    // 注册“关闭窗口前二次确认”处理器
    // Windows: 注册控制台关闭处理器并弹出确认对话框
    // macOS:   无法拦截窗口关闭，仅注册 SIGINT/SIGTERM 打印提示信息
    if(!platform::register_close_confirmation()) {
        spdlog::error("注册关闭二次确认弹窗处理失败！");
    }

    // 开发者声明
    spdlog::info("开发者：CNGEGE > 2024/04/13");
    spdlog::info("按程序提示流程走，完成后自动会退出，{}请勿手动关闭{}程序，手动关闭可能导致汉化失败", "\033[1;33m", "\033[0m");

    std::pair<std::string, int> proxy = { "", 0 };
    {
        auto p1 = utils::get_proxy_env();
        if(p1) {
            proxy = *p1;
        }
        else {
            auto p2 = utils::GetSystemProxySettings();
            if(p2) {
                proxy = *p2;
            }
        }
    }
    if(proxy.second) {
        spdlog::info("检测到已开启代理：{}:{}", proxy.first, proxy.second);
    }


    // 打印构建平台与版本
#ifdef CURRENT_PLATFORM_ISX64
    std::string arch_str("x64");
#else
    std::string arch_str("x86");
#endif // CURRENT_PLATFORM_ISX64

    if(FileVer) {
        spdlog::info("程序架构：- {}  版本: - {}", arch_str,  FileVer.toString(true));
    }
    else {
        spdlog::warn("程序架构：- {}  程序版本解析失败... at {}", arch_str, FILEVERSION);
    }
    


    platform::set_console_title(FileVer.toString(true).c_str());

    if (LocalizationJSON.empty()) {
        LocalizationJSON = "localization.json";
    }

    if(!rollback && !fs::exists(LocalizationJSON)) {
        // 读取项目更新时间
        try {
            std::string repoinfo;
            if(utils::ReadHttpDataString("https://api.github.com", "/repos/cngege/GitHubDesktop2Chinese", repoinfo, proxy)) {
                auto infojson = json::parse(repoinfo);
                std::optional<std::string> info = formatTime(infojson["updated_at"]);
                if(info) {
                    spdlog::info("仓库最新更新时间: {}{}{}", "\033[32m", *info, "\033[0m");
                }
            }
        }
        catch(...) {}
    }

    // 检查更新
    // https://api.github.com/repos/cngege/GitHubDesktop2Chinese/releases/latest
    {
        // 还原操作不应该被"检查更新/自动更新"打断
        if(FileVer.status != std::Version::Dev && !rollback) {
            spdlog::info("检查更新中..");
            try {
                std::string repoinfo;
                if(utils::ReadHttpDataString("https://api.github.com" , "/repos/cngege/GitHubDesktop2Chinese/releases/latest", repoinfo, proxy)) {
                    auto infojson = json::parse(repoinfo);
                    auto tag_name = infojson["tag_name"].get<std::string>();
                    std::Version remoteVer(tag_name.c_str());
                    if(!remoteVer) {
                        spdlog::warn("远程仓库中的版本号解析失败, ({})", tag_name);
                    }
                    else {
                        if(FileVer < remoteVer) {
                            spdlog::info("发现新版本: {}", remoteVer.toString());
                            std::string browser_download_url = infojson["assets"][0]["browser_download_url"].get<std::string>();
                            std::string downlink = infojson["assets"][0]["url"].get<std::string>();
                            int download_count = infojson["assets"][0]["download_count"].get<int>();
                            size_t max_size = infojson["assets"][0]["size"].get<int64_t>();
                            spdlog::info("下载链接({}次下载): {}", download_count, browser_download_url);
                            spdlog::info("是否自动更新:");
                            bool autoupdate = utils::ReadUserInput_bool({ "n", "y" }, 0);
                            if(autoupdate) {
                                //https://github.com/cngege/GitHubDesktop2Chinese/releases/download/v1.0.14/GitHubDesktop2Chinese.exe
                                //https://api.github.com/repos/cngege/GitHubDesktop2Chinese/releases/assets/376558079
                                std::regex url_regex(R"(^((?:https?://)[^/]+)(/.*)?$)");
                                std::smatch matches;
                                if(std::regex_match(downlink, matches, url_regex)) {
                                    auto result = utils::UpdateProgram(matches[1].str(), matches[2].str(), fs::path(argv[0]), max_size, proxy);
                                    if(!result) {
                                        spdlog::error("失败 更新过程出现异常");
                                    }
                                    else {
                                        // 立马退出 等待替换
                                        return 0;
                                    }
                                }
                                else {
                                    spdlog::error("下载地址可能变更, 正则表达式无法捕获");
                                }
                            }
                        }
                        else {
                            spdlog::info("当前版本已经是最新版..");
                        }
                    }
                }
                else {
                    spdlog::warn("远程信息读取失败..");
                }
            }
            catch(...) {
                spdlog::warn("检查更新时出现异常");
            }
        }
    }

    // 还原(rollback)只需要资源目录，不需要汉化映射；因此整段跳过，
    // 保证离线、json 缺失或被改坏时仍然能回滚。
    if(!rollback) {
        // 如果是仅从远程仓库读取汉化文件
        if (only_read_from_remote) {
            spdlog::info("尝试从远程仓库中获取");
            std::string httpjson;
            if (utils::ReadHttpDataString("https://raw.githubusercontent.com" , "/cngege/GitHubDesktop2Chinese/master/json/localization.json", httpjson, proxy)) {
                if(parse_localization_from_remote(httpjson)) {
                    spdlog::info("远程读取成功");
                }
                else {
                    spdlog::warn("远程返回的本地化文件格式异常,请稍后再试");
                    PAUSE
                    return 1;
                }
            }
            else {
                spdlog::warn("远程获取失败,请检查网络和代理,并稍后再试");
                PAUSE
                return 1;
            }
        }
        // 没有指定仅从远程仓库获取汉化文件
        else {
            // 判断汉化映射文件是否存在, 不存在则创建一个
            if (!fs::exists(LocalizationJSON)) {
                // 没有发现json文件,尝试从远程开源项目中获取
                spdlog::warn("没有指定,或从指定位置没有发现 {} 文件", "localization.json");
                spdlog::info("尝试从远程仓库中获取");
                std::string httpjson;
                if (utils::ReadHttpDataString("https://raw.githubusercontent.com" , "/cngege/GitHubDesktop2Chinese/master/json/localization.json", httpjson, proxy)) {
                    if(parse_localization_from_remote(httpjson)) {
                        spdlog::info("远程读取成功");
                    }
                    else {
                        spdlog::warn("远程返回的本地化文件格式异常,请稍后再试");
                        PAUSE
                        return 1;
                    }
                }
                else {
                    spdlog::warn("远程获取失败 - 请{}重试", !proxy.second ? "尝试开启代理或":"");
                    PAUSE
                    return 1;
                }
            }
            else
            {
                // 本地读取汉化文件到json中
                std::ifstream config(LocalizationJSON);
                if (!config) {
                    spdlog::error("localization.json 打开失败,无法读取");
                    PAUSE
                    return 1;
                }
                try
                {
                    config >> localization;
                }
                catch (const std::exception& e)
                {
                    spdlog::error("{} at line {}", e.what(), __LINE__);
                    PAUSE
                    return 1;
                }
            }
        }
    }

    // 读取映射文件中的提示信息
    if (localization.contains("tip") && localization.at("tip").is_array() && !localization.empty()) {
        for(auto& it : localization.at("tip")) {
            if (it.is_string()) {
                spdlog::info(" **通知** {}", it.get<std::string>());
            }
        }
    }


    // Github Desktop 存在目录没有提前设置
    if (!fs::exists(Base) || !fs::exists(Base / "index.html")) {
        // 自动发现本地 GitHub Desktop 安装（Windows 读取注册表，macOS 扫描 .app bundle）
        try {
            auto install = platform::find_github_desktop();
            if(!install) {
                // 未发现安装条目时，platform 层已打印对应来源的错误信息（Windows: 注册表 / macOS: 应用程序目录）
                spdlog::warn("你可能没有安装GithubDesktop，请先安装然后打开此程序或者手动指定main.js所在的文件夹目录");
                Base = LoopGetBasePath();
            }
            else {
                Base = install->resource_dir;
                std::string desktop_local_ver_str = install->version;


                spdlog::info("正在读取GitHubDesktop最新版...");
                std::string httpjson_str;
                // 远程响应可能被代理/门户劫持成 HTML：解析异常不能掀翻整个流程
                try {
                    if(utils::ReadHttpDataString("https://central.github.com", "/deployments/desktop/desktop/changelog.json", httpjson_str, proxy)) {
                        json httpjson = json::parse(httpjson_str);
                        if(httpjson.is_array() && !httpjson.empty()
                           && httpjson[0].contains("version") && httpjson[0]["version"].is_string()) {
                            std::string v = httpjson[0]["version"].get<std::string>();
                            std::Version desktop_remote_ver(v.c_str());
                            std::Version desktop_local_ver(desktop_local_ver_str.c_str());
                            spdlog::info("已读取到远程GitHubDesktop最新版:{} {}", v, desktop_remote_ver > desktop_local_ver ? "(\033[1;33m需更新\033[0m)" : "");
                        }
                        else {
                            spdlog::warn("远程GitHubDesktop版本信息格式异常.");
                        }
                    }
                    else {
                        spdlog::warn("远程GitHubDesktop版本读取失败.");
                    }
                }
                catch(const std::exception& e) {
                    spdlog::warn("远程GitHubDesktop版本解析失败: {}", e.what());
                }

                spdlog::info("已从{}中读取本地GitHubDesktop信息:", install->source);
                spdlog::info("本地GitHubDesktop版本: {}", desktop_local_ver_str);
                spdlog::info("安装目录: {}", install->install_location.string());
                spdlog::info("最后拼接完整目录: {}", Base.string());


                if (!fs::exists(Base)) {
                    spdlog::warn("{}最终获取到的目录不存在,请手动指定main.js所在的文件夹目录", install->source);
                    Base = LoopGetBasePath();
                }

            }
        }
        catch(const std::runtime_error& err) {
            spdlog::error("runtime_error {} at line: {}", err.what(), __LINE__);
            PAUSE
            return 1;
        }
    }

    fs::path mainjs = "main.js";
    fs::path mainjsbak = "main.js.bak";

    fs::path rendererjs = "renderer.js";
    fs::path rendererjsbak = "renderer.js.bak";

    // 指定还原
    if(rollback) {
        if(fs::exists(Base / mainjsbak)) {
            std::error_code rm_ec;
            fs::remove(Base / mainjs, rm_ec);
            if(auto copy_ec = copy_file_overwrite(Base / mainjsbak, Base / mainjs)) {
                spdlog::error("{} 还原失败: {}", mainjs.string(), copy_ec.message());
            }
            else {
                spdlog::info("{} 还原完成", mainjs.string().c_str());
            }
        }
        else {
            spdlog::warn("{} 回滚失败, {} 文件不存在", mainjs.string().c_str(), mainjsbak.string().c_str());
        }

        if(fs::exists(Base / rendererjsbak)) {
            std::error_code rm_ec;
            fs::remove(Base / rendererjs, rm_ec);
            if(auto copy_ec = copy_file_overwrite(Base / rendererjsbak, Base / rendererjs)) {
                spdlog::error("{} 还原失败: {}", rendererjs.string(), copy_ec.message());
            }
            else {
                spdlog::info("{} 还原完成", rendererjs.string().c_str());
            }
        }
        else {
            spdlog::warn("{} 回滚失败, {} 文件不存在", rendererjs.string().c_str(), rendererjsbak.string().c_str());
        }
        PAUSE
        return 0;
    }


    // 如果没有js文件却有备份文件 则从备份恢复
    if (!fs::exists(Base / mainjs)) {
        if (!fs::exists(Base / mainjsbak)) {
            spdlog::warn("目录有误，找不到目录下的main.js. ");
            PAUSE
            return 1;
        }
        if(auto copy_ec = copy_file_overwrite(Base / "main.js.bak", Base / "main.js")) {
            spdlog::error("main.js 从备份还原失败: {}", copy_ec.message());
            PAUSE
            return 1;
        }
        spdlog::warn("main.js 未找到, 但已从备份main.js.bak中还原");
    }

    if (!fs::exists(Base / rendererjs)) {
        if (!fs::exists(Base / rendererjsbak)) {
            spdlog::warn("目录有误，找不到目录下的renderer.js. ");
            PAUSE
            return 1;
        }
        if(auto copy_ec = copy_file_overwrite(Base / "renderer.js.bak", Base / "renderer.js")) {
            spdlog::error("renderer.js 从备份还原失败: {}", copy_ec.message());
            PAUSE
            return 1;
        }
        spdlog::warn("renderer.js 未找到, 但已从备份renderer.js.bak中还原");
    }

    // 仅在备份文件不存在时备份
    if (!fs::exists(Base / "main.js.bak")) {
        if(auto copy_ec = copy_file_overwrite(Base / "main.js", Base / "main.js.bak")) {
            spdlog::error("新建备份 main.js -> main.js.bak 失败: {}", copy_ec.message());
        }
        else {
            spdlog::info("已新建备份 main.js -> main.js.bak");
        }
    }

    if (!fs::exists(Base / "renderer.js.bak")) {
        if(auto copy_ec = copy_file_overwrite(Base / "renderer.js", Base / "renderer.js.bak")) {
            spdlog::error("新建备份 renderer.js -> renderer.js.bak 失败: {}", copy_ec.message());
        }
        else {
            spdlog::info("已新建备份 renderer.js -> renderer.js.bak");
        }
    }

    // 判断版本（minversion 缺失或不是字符串时按"无要求"处理，不能直接 get<string> 抛异常）
    const bool has_min_version = localization.contains("minversion")
                                 && localization.at("minversion").is_string()
                                 && !localization.at("minversion").get<std::string>().empty();
    if(FileVer.status != std::Version::Dev && has_min_version) {
        const std::string min_version_str = localization.at("minversion").get<std::string>();
        std::Version JsonVer(min_version_str.c_str());
        if(!JsonVer) {
            spdlog::warn("映射文件中 minversion 解析失败... at {}", min_version_str.c_str());
            PAUSE
        }
        else {
            if(FileVer < JsonVer) {
                // 不符合要求
                spdlog::warn("文件要求加载器版本至少为: {}, 但加载器版本为: {}", JsonVer.toString(), FileVer.toString());
                spdlog::info("请更新：{}", "https://github.com/cngege/GitHubDesktop2Chinese/releases");
                // 询问是否强制执行

                if(!no_pause) {
                    spdlog::info("输入(f)强制执行替换(可能会导致无法打开), 其他退出..");
                    std::string input;
                    std::cin >> input;
                    if(input != "f" && input != "F") {
                        return 1;
                    }
                }
            }
            else {
                PAUSE
            }
        }
    }
    else {
        PAUSE
    }
    
    int ret_num = 0;
    // 是否有文件写入失败(写不进去时界面不会变化, 必须让用户知道并返回非零退出码)
    bool write_failed = false;

    // 处理 main[_dev].json -> main.js
    {
        spdlog::info("正在处理{}文件,{}请勿关闭...{}", "main.js", "\033[33m", "\033[0m");
        const JsFileTags tags{
            "main.js", "main", "main_dev",
            "[main]", "[main 3]", "[select main]", "[select main 3]",
            _debug_error_check_mode_main
        };
        if(!process_js_file(Base / "main.js", tags, ret_num)) {
            write_failed = true;
            ++ret_num;
        }
    }

    // 处理 renderer[_dev].json -> renderer.js
    {
        spdlog::info("正在处理{}文件,{}请勿关闭...{}", "renderer.js", "\033[33m", "\033[0m");
        const JsFileTags tags{
            "renderer.js", "renderer", "renderer_dev",
            "[renderer]", "[renderer]", "[select renderer]", "[select renderer 3]",
            _debug_error_check_mode_renderer
        };
        if(!process_js_file(Base / "renderer.js", tags, ret_num)) {
            write_failed = true;
            ++ret_num;
        }
    }


    // 获取项目参与者:
    // https://api.github.com/repos/cngege/GitHubDesktop2Chinese/contributors
    try {
        spdlog::info("正在获取项目参与者");
        std::string contributors;
        if(utils::ReadHttpDataString("https://api.github.com" , "/repos/cngege/GitHubDesktop2Chinese/contributors", contributors, proxy)) {
            auto contributorsjson = json::parse(contributors);
            spdlog::info("人数: {}", contributorsjson.size());
            int num = 0;
            for(auto& item : contributorsjson.items()) {
                num++;
                if(num > 10) break;
                spdlog::info("{}/10 名称:{} 贡献:{}{}{} 主页:{}", num, item.value()["login"].get<std::string>(),"\033[37m\033[5m", item.value()["contributions"].get<int>(),"\033[0m", item.value()["html_url"].get<std::string>());
            }
        }
    }
    catch(...) {
        spdlog::warn("读取解析数据出现异常.");
    }

    // 处理失败必须显式告警: 否则界面仍是原语言, 而日志里却写着"汉化结束"
    if (write_failed) {
        spdlog::error("汉化失败: 有文件未能处理或写入, GitHub Desktop 界面不会发生变化.");
        spdlog::info("排查建议:");
        spdlog::info("  1) 完全退出 GitHub Desktop(托盘也要退出)后重试");
        spdlog::info("  2) 把本程序加入杀毒/安全软件的白名单(如火绒的信任区), 或临时关闭其文件保护");
        spdlog::info("  3) 右键以管理员身份运行本程序");
        spdlog::info("  4) 确认安装目录可写: {}", fs::path(Base).string());
    }

    PAUSE
    // 进程退出码只有低 8 位有效：失效项达到 256 的整数倍时会被截断成 0，
    // 看起来像"成功"。这里钳制到 255，保持"有失败就一定非零"。
    return (ret_num > 255) ? 255 : ret_num;
}

bool GetBasePath(std::string& out) {
    if(!std::getline(std::cin, out)) {
        // 标准输入已结束（管道/重定向），继续循环询问只会变成死循环
        return false;
    }
    if (!fs::exists(out)) {
        spdlog::warn("你输入的目录不存在. ");
        return false;
    }
    fs::path base = out;
    fs::path mainjs = "main.js";
    fs::path mainjsbak = "main.js.bak";
    if (!fs::exists(base / mainjs)) {
        if (!fs::exists(base / mainjsbak)) {
            spdlog::warn("目录有误，找不到目录下的main.js. ");
            return false;
        }
        if(auto copy_ec = copy_file_overwrite(base / "main.js.bak", base / "main.js")) {
            spdlog::warn("main.js 从备份还原失败: {}", copy_ec.message());
            return false;
        }
        spdlog::warn("main.js 未找到, 但已从备份main.js.bak中还原");
    }

    fs::path rendererjs = "renderer.js";
    fs::path rendererjsbak = "renderer.js.bak";
    if (!fs::exists(base / rendererjs)) {
        if (!fs::exists(base / rendererjsbak)) {
            spdlog::warn("目录有误，找不到目录下的renderer.js. ");
            return false;
        }
        if(auto copy_ec = copy_file_overwrite(base / "renderer.js.bak", base / "renderer.js")) {
            spdlog::warn("renderer.js 从备份还原失败: {}", copy_ec.message());
            return false;
        }
        spdlog::warn("renderer.js 未找到, 但已从备份renderer.js.bak中还原");
    }
    return true;
}

std::string LoopGetBasePath() {
    spdlog::info("请输入目录.");
    std::string tempDir;
    while (true)
    {
        if (GetBasePath(tempDir)) {
            return tempDir;
        }
        if (!std::cin) {
            // 输入流已结束：再问也拿不到目录，交给调用方的"目录有误"分支处理
            spdlog::warn("没有可读取的输入, 结束目录询问.");
            return {};
        }
        spdlog::info("重新输入.");
    }
}

void DeveloperOptions() {
    while (true)
    {
        platform::clear_screen();
        spdlog::info("选择你要修改的功能");
        spdlog::info("0) 跳出.");
        spdlog::info("1) [{}] main崩溃调试.", _debug_error_check_mode_main);
        spdlog::info("2) [{}] renderer崩溃调试.", _debug_error_check_mode_renderer);
        spdlog::info("3) [{}] 翻译项失效检测.", _debug_invalid_check_mode);
        spdlog::info("4) [{}] 不替换资源.不干预其他开发者选项.", _debug_no_replace_res);
        spdlog::info("5) [{}] 优先从备份文件中汉化(会直接改变资源文件的来源,影响其他选项).", _debug_translation_from_bak);
        spdlog::info("6) [{}] 仅替换指定映射项，以优化汉化作者替换时间", _debug_dev_replace);
        if(FileVer.major == 0 && FileVer.minor == 0 && FileVer.revision == 0) {
            spdlog::info("20) [{}] 手动指定程序版本仅限于调试", _debug_dev_setversion);
        }
        std::cout << std::endl;

        int sys = 0;
        spdlog::info("请输入你要修改的功能:");
        std::cin >> sys;
        switch (sys)
        {
        case 0:
            return;
        case 1:
            _debug_error_check_mode_main = utils::ReadUserInput_bool({ "false", "true" });
            break;
        case 2:
            _debug_error_check_mode_renderer = utils::ReadUserInput_bool({ "false", "true" });
            break;
        case 3:
            _debug_invalid_check_mode = utils::ReadUserInput_bool({ "false", "true" });
            break;
        case 4:
            _debug_no_replace_res = utils::ReadUserInput_bool({ "false", "true" });
            break;
        case 5:
            _debug_translation_from_bak = utils::ReadUserInput_bool({ "false", "true" });
            break;
        case 6:
            _debug_dev_replace = utils::ReadUserInput_bool({ "false", "true" });
            break;
        case 20:
            _debug_dev_setversion = utils::ReadUserInput_bool({ "false", "true" });
            break;
        }
    }
    
}


std::optional<std::string> formatTime(std::string time_str) {
    using namespace std::chrono;
    std::optional<std::string> ret;
#if defined(_WIN32) || defined(_WIN64)
    // MSVC STL 支持 C++23 的 std::chrono::parse，保留原实现
    std::istringstream ss{ time_str };

    sys_seconds tp;
    ss >> parse("%FT%TZ", tp);
    if(!ss.fail()) {
        auto sctp = time_point_cast<seconds>(tp);
        std::time_t cftime = decltype(sctp)::clock::to_time_t(sctp);
        std::tm* tm = std::localtime(&cftime);
        if(tm != nullptr) {
            std::ostringstream oss;
            oss << std::put_time(tm, "%Y年%m月%d日 %H时%M分%S秒");
            ret = oss.str();
        }
    }
#else
    // libc++ 对 chrono parse 的支持不完整，改用 POSIX strptime 解析 UTC 时间
    // 输入格式: 2024-04-13T12:34:56Z
    std::tm tm = {};
    if(::strptime(time_str.c_str(), "%Y-%m-%dT%H:%M:%SZ", &tm) != nullptr) {
        std::time_t cftime = ::timegm(&tm);
        std::tm* local = std::localtime(&cftime);
        if(local != nullptr) {
            std::ostringstream oss;
            oss << std::put_time(local, "%Y年%m月%d日 %H时%M分%S秒");
            ret = oss.str();
        }
    }
#endif
    return ret;
}