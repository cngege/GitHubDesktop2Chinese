// GitHubDesktop2Chinese.cpp: 定义应用程序的入口点。


#define _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING   //消除 converter.to_bytes的警告
#define _CRT_SECURE_NO_WARNINGS                             //消除 sprintf的警告

#define PAUSE if(!no_pause) system("pause")

#include "GitHubDesktop2Chinese.h"
#include <string>
#include <filesystem>

#include <regex>

#include "spdlog/spdlog.h"          // 日志式输出库
#include "nlohmann/json.hpp"        // JSON读取本地配置库
#include "WinReg/WinReg.hpp"        // 注册表操作库

#include <CLI/CLI.hpp>              // 参数管理器:   https://github.com/CLIUtils/CLI11
#include "Utils/utils.hpp"
#include "VersionParse/Version.hpp"

#if _DEBUG
#define NO_REPLACE 0
#endif // _DEBUG

std::Version FileVer{0,0,0};


namespace fs = std::filesystem;
using json = nlohmann::json;

// 设置一个路径的全局变量  指向要修改JS的目录
fs::path Base;
fs::path LocalizationJSON;

//fs::path Main_Json_Path;
//fs::path Renderer_Json_Path;

bool no_pause;                                  // 程序在结束前是否暂停
bool only_read_from_remote;                     // 仅从远程url中读取本地化文件

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

std::optional<std::string> formatTime(std::string time_str);

// argv[0] 是程序路径
int main(int argc, char* argv[])
{
    // 设置控制台的输入 输出编码：
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    // 设置控制台打印日志输出等级
#if _DEBUG
    spdlog::set_level(spdlog::level::debug);
#endif // _DEBUG
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

        //auto git_cmd = app.add_subcommand("action", "Github自动流程");
        //git_cmd->add_option("--main_json", Main_Json_Path,                          "手动指定main.json的文件位置,直接处理此文件");
        //git_cmd->add_option("--renderer_json", Renderer_Json_Path,                  "手动指定renderer.json的文件位置,直接处理此文件");

        app.add_flag("--nopause", no_pause,                         "程序在结束前不再暂停等待");
        app.add_flag("-r,--onlyfromremote", only_read_from_remote,  "仅从远程url中读取本地化文件");
        app.add_option("-j,--json", LocalizationJSON,               "指定本地化JSON文件的本地路径");
        app.add_option("-g,--githubdesktoppath", Base,              "指定GitHubDesktop要汉化的资源所在目录");
        
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
            //// 验证 Main_Json_Path 是否有效
            //if(!Main_Json_Path.string().empty()) {
            //    if(!fs::exists(Main_Json_Path)) {
            //        throw CLI::ValidationError("(action main_json) 指定的资源文件不存在");
            //    }
            //}
            //// 验证 Renderer_Json_Path 是否有效
            //if(!Renderer_Json_Path.string().empty()) {
            //    if(!fs::exists(Renderer_Json_Path)) {
            //        throw CLI::ValidationError("(action renderer_json) 指定的资源文件不存在");
            //    }
            //}
        });


        CLI11_PARSE(app, argc, argv);
    }
    FileVer = std::Version(FILEVERSION);
    
    // 开发者声明
    spdlog::info("开发者：CNGEGE > 2024/04/13");
    if(FileVer) {
        spdlog::info("版本: {}", FileVer.toString(true));
    }
    else {
        spdlog::warn("程序版本解析失败... at {}", FILEVERSION);
    }
    
    if (GetKeyState(VK_SHIFT) & 0x8000 || _debug_goto_devoptions) {
        // 如果Shift按下, 则进入开发者选项
        SetConsoleTitle("开发者模式");
        spdlog::info("您已进入开发者模式");
        DeveloperOptions();
    }

    SetConsoleTitle(FileVer.toString(true).c_str());

    if (LocalizationJSON.empty()) {
        LocalizationJSON = "localization.json";
    }

    if(!fs::exists(LocalizationJSON)) {
        // 读取项目更新时间
        try {
            std::string repoinfo;
            if(utils::ReadHttpDataString("https://api.github.com", "/repos/cngege/GitHubDesktop2Chinese", repoinfo)) {
                auto infojson = json::parse(repoinfo);
                std::optional<std::string> info = formatTime(infojson["updated_at"]);
                if(info) {
                    spdlog::info("仓库最新更新时间: {}{}{}", "\033[33m", *info, "\033[0m");
                    PAUSE;
                }
            }
        }
        catch(...) {}
    }

    // 检查更新
    // https://api.github.com/repos/cngege/GitHubDesktop2Chinese/releases/latest
    {
        if(FileVer.status != std::Version::Dev) {
            spdlog::info("检查更新中..");
            try {
                std::string repoinfo;
                if(utils::ReadHttpDataString("https://api.github.com", "/repos/cngege/GitHubDesktop2Chinese/releases/latest", repoinfo)) {
                    auto infojson = json::parse(repoinfo);
                    auto tag_name = infojson["tag_name"].get<std::string>();
                    std::Version remoteVer(tag_name.c_str());
                    if(!remoteVer) {
                        spdlog::warn("远程仓库中的版本号解析失败, ({})", tag_name);
                    }
                    else {
                        if(FileVer < remoteVer) {
                            spdlog::info("发现新版本: {}", remoteVer.toString());
                            std::string downlink = infojson["assets"][0]["browser_download_url"].get<std::string>();
                            spdlog::info("点击链接下载: {}", downlink);
                        }
                        else {
                            spdlog::info("当前版本已经是最新版..");
                        }
                    }
                    PAUSE;
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

    // 如果是仅从远程仓库读取汉化文件
    if (only_read_from_remote) {
        spdlog::info("尝试从远程开源项目中获取");
        std::string httpjson;
        if (utils::ReadHttpDataString("https://raw.kkgithub.com", "/cngege/GitHubDesktop2Chinese/master/json/localization.json", httpjson)) {
            localization = json::parse(httpjson);
            spdlog::info("远程读取成功");
        }
        else {
            spdlog::warn("远程获取失败,请检查网络和代理,并稍后再试");
            PAUSE;
            return 0;
        }
    }
    // 没有指定仅从远程仓库获取汉化文件
    else {
        // 判断汉化映射文件是否存在, 不存在则创建一个
        if (!fs::exists(LocalizationJSON)) {
            // 没有发现json文件,尝试从远程开源项目中获取
            spdlog::warn("没有指定,或从指定位置没有发现 {} 文件", "localization.json");
            spdlog::info("尝试从远程开源项目中获取");
            std::string httpjson;
            if (utils::ReadHttpDataString("https://raw.kkgithub.com", "/cngege/GitHubDesktop2Chinese/master/json/localization.json", httpjson)) {
                localization = json::parse(httpjson);
                spdlog::info("远程读取成功");
            }
            else {
                spdlog::warn("远程获取失败 - 是否创建json框架，手动编辑汉化映射");
                if (utils::ReadUserInput_bool({ "n","y" }, 0)) {
                    spdlog::warn("远程获取失败: {}, 已创建框架,请先编辑创建翻译映射", "localization.json");
                    std::ofstream io(LocalizationJSON);
                    io << std::setw(4) << localization << std::endl;
                    io.close();
                }
                PAUSE;
                return 0;
            }
        }
        else
        {
            // 本地读取汉化文件到json中
            std::ifstream config(LocalizationJSON);
            if (!config) {
                spdlog::error("localization.json 打开失败,无法读取");
            }
            try
            {
                config >> localization;
            }
            catch (const std::exception& e)
            {
                spdlog::error("{} at line {}", e.what(), __LINE__);
                PAUSE;
                return 0;
            }
        }
    }


    // Github Desktop 存在目录没有提前设置
    if (!fs::exists(Base) || !fs::exists(Base / "index.html")) {


        // 首先要能够成功读取注册表
        //	拿到当前用户sid
        std::string sid = GetCurrentUserSid();
        spdlog::debug("sid:{}", sid);

        //	检查注册表中是否存在GithubDesktop
        winreg::RegKey key;
        winreg::RegResult result = key.TryOpen(HKEY_USERS, utils::to_wide_string(sid) + L"\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\GitHubDesktop");
        if (!result)
        {
            spdlog::warn("你可能没有安装GithubDesktop，请先安装然后打开此程序或者手动指定main.js所在的文件夹目录");
            spdlog::info("请输入目录.");
            Base = LoopGetBasePath();
        }
        else {
            try
            {
                // 首先要从注册表中拿到GithubDesktop相关信息
                // 任务就是将Base 中写入路径
                std::wstring ver = key.GetStringValue(L"DisplayVersion");
                std::wstring path = key.GetStringValue(L"InstallLocation");
                Base = path + L"\\" + L"app-" + ver + L"\\resources\\app";

                spdlog::info("已从注册表中读取信息:");
                spdlog::info("Github Desktop 版本: {}", utils::to_byte_string(ver));
                spdlog::info("安装目录: {}", utils::to_byte_string(path));
                spdlog::info("最后拼接完整目录: {}", Base.string());

                if (!fs::exists(Base)) {
                    spdlog::warn("注册表最终获取到的目录不存在,请手动指定main.js所在的文件夹目录");
                    Base = LoopGetBasePath();
                }

            }
            catch (const winreg::RegException& regerr)
            {
                spdlog::error("RegException {} at line: {}", regerr.what(), __LINE__);
                PAUSE;
                return 1;
            }
            catch(const std::runtime_error& err) {
                spdlog::error("runtime_error {} at line: {}", err.what(), __LINE__);
                PAUSE;
                return 1;
            }
        }
    }


    // 如果没有js文件却有备份文件 则从备份恢复
    fs::path mainjs = "main.js";
    fs::path mainjsbak = "main.js.bak";
    if (!fs::exists(Base / mainjs)) {
        if (!fs::exists(Base / mainjsbak)) {
            spdlog::warn("目录有误，找不到目录下的main.js. ");
            PAUSE;
            return 1;
        }
        fs::copy_file(Base / "main.js.bak", Base / "main.js");
        spdlog::warn("main.js 未找到, 但已从备份main.js.bak中还原");
    }

    fs::path rendererjs = "renderer.js";
    fs::path rendererjsbak = "renderer.js.bak";
    if (!fs::exists(Base / rendererjs)) {
        if (!fs::exists(Base / rendererjsbak)) {
            spdlog::warn("目录有误，找不到目录下的renderer.js. ");
            PAUSE;
            return 1;
        }
        fs::copy_file(Base / "renderer.js.bak", Base / "renderer.js");
        spdlog::warn("renderer.js 未找到, 但已从备份renderer.js.bak中还原");
    }



    // TODO 备份main.js 和 renderer.js 文件
    // 仅在备份文件不存在时备份
    if (!fs::exists(Base / "main.js.bak")) {
        fs::copy_file(Base / "main.js", Base / "main.js.bak");
        spdlog::info("已新建备份 main.js -> main.js.bak");
    }

    if (!fs::exists(Base / "renderer.js.bak")) {
        fs::copy_file(Base / "renderer.js", Base / "renderer.js.bak");
        spdlog::info("已新建备份 renderer.js -> renderer.js.bak");
    }

    // 判断版本
    if(FileVer.status != std::Version::Dev && !localization["minversion"].empty()) {
        std::Version JsonVer(localization["minversion"].get<std::string>().c_str());
        if(!JsonVer) {
            spdlog::warn("映射文件中 minversion 解析失败... at {}", localization["minversion"].get<std::string>().c_str());
            PAUSE;
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
                PAUSE;
            }
        }
    }
    else {
        PAUSE;
    }
    
    int ret_num = 0;
    // 处理 main[_dev].json文件
    {
        int out = 0;
        // 如果"从备份文件中汉化"选项打开 则判断备份文件是否存在,以便尝试从备份文件中读取
        std::string main_str = ((_debug_translation_from_bak || _debug_invalid_check_mode) && fs::exists(Base / "main.js.bak")) ? utils::ReadFile(fs::path(Base / "main.js.bak").string()) : utils::ReadFile(fs::path(Base / "main.js").string());
        for (auto& item : localization[_debug_dev_replace?"main_dev":"main"].items())
        {
#if NO_REPLACE
            continue;
#endif // NO_REPLACE

            std::string rege = item.value()[0].get<std::string>();
            if (rege.empty() || rege == "\"\"") {
                continue;
            }
            std::regex pattern(rege);

            // 开发者选项 失效检测
            if (_debug_invalid_check_mode) {
                bool found = std::regex_search(main_str, pattern);
                if (!found) {
                    spdlog::warn("[main] 检测到失效项: {}", rege);
                    ret_num++;
                }
                if(item.value().size() >= 3) {
                    std::regex pattern3(item.value()[2].get<std::string>());
                    found = std::regex_search(main_str, pattern3);
                    if(!found) {
                        spdlog::warn("[renderer] 检测到失效项: {}", item.value()[2].get<std::string>());
                        ret_num++;
                    }
                }
                continue;
            }


            if(item.value().size() >= 3) {
                // 对数组第三项进行全局查找
                std::string regex_str = item.value()[2].get<std::string>();
                std::regex pattern3(regex_str);
                std::sregex_iterator it = std::sregex_iterator(main_str.begin(), main_str.end(), pattern3);
                if(it != std::sregex_iterator()) {
                    const std::smatch& match = *it;
                    for(size_t i = 1; i < match.size(); i++) {
                        std::string replace_str = "#\\{" + std::to_string(i) + "\\}";
                        std::regex replace_regx(replace_str);
                        item.value()[1] = std::regex_replace(item.value()[1].get<std::string>(), replace_regx, match[i].str());
                    }
                }
                else {
                    // 如果没有找到，则应该进行提示并跳过此项，以免进行错误的字符插入，造成程序无法打开
                    spdlog::warn("[main] 出现一处失效项,此项将跳过: {}", regex_str);
                    continue;
                }
            }

            // 替换
            main_str = std::regex_replace(main_str, pattern, item.value()[1].get<std::string>());
            if (_debug_error_check_mode_main) {
                spdlog::info("[main][out:{}]已经替换:{}->{}", out, rege, utils::utf8ToAnsi(item.value()[1].get<std::string>()));
                out--;
                if (out <= 0) {
                    utils::WriteFile(fs::path(Base / "main.js").string(), main_str);
                    spdlog::info("已写入. 你希望下次替换多少条后写入:");
                    std::cin >> out;
                }
            }
        }
        if (!_debug_invalid_check_mode && !_debug_no_replace_res) {
            // 写入
            utils::WriteFile(fs::path(Base / "main.js").string(), main_str);
        }
        spdlog::info("{} 汉化结束.", "main.js");
    }



    // 处理renderer.js文件
    {
        int out = 0;
        std::string renderer_str = ((_debug_translation_from_bak || _debug_invalid_check_mode) && fs::exists(Base / "renderer.js.bak")) ? utils::ReadFile(fs::path(Base / "renderer.js.bak").string()) :  utils::ReadFile(fs::path(Base / "renderer.js").string());
        for (auto& item : localization[_debug_dev_replace?"renderer_dev":"renderer"].items())
        {
#if NO_REPLACE
            continue;
#endif // NO_REPLACE

            std::string rege = item.value()[0].get<std::string>();
            if (rege.empty() || rege == "\"\"") {
                continue;
            }
            std::regex pattern(rege);

            // 开发者选项 失效检测
            if (_debug_invalid_check_mode) {
                bool found = std::regex_search(renderer_str, pattern);
                if (!found) {
                    spdlog::warn("[renderer] 检测到失效项: {}", rege);
                    ret_num++;
                }
                if(item.value().size() >= 3) {
                    std::regex pattern3(item.value()[2].get<std::string>());
                    found = std::regex_search(renderer_str, pattern3);
                    if(!found) {
                        spdlog::warn("[renderer] 检测到失效项: {}", item.value()[2].get<std::string>());
                        ret_num++;
                    }
                }
                continue;
            }

            if(item.value().size() >= 3) {
                // 对数组第三项进行全局查找
                std::string regex_str = item.value()[2].get<std::string>();
                std::regex pattern3(regex_str);
                std::sregex_iterator it = std::sregex_iterator(renderer_str.begin(), renderer_str.end(), pattern3);
                if(it != std::sregex_iterator()) {
                    const std::smatch& match = *it;
                    for(size_t i = 1; i < match.size(); i++) {
                        std::string replace_str = "#\\{" + std::to_string(i) + "\\}";
                        std::regex replace_regx(replace_str);
                        item.value()[1] = std::regex_replace(item.value()[1].get<std::string>(), replace_regx, match[i].str());
                    }
                }
                else {
                    // 如果没有找到，则应该进行提示并跳过此项，以免进行错误的字符插入，造成程序无法打开
                    spdlog::warn("[renderer] 出现一处失效项,此项将跳过: {}", regex_str);
                    continue;
                }
            }

            renderer_str = std::regex_replace(renderer_str, pattern, item.value()[1].get<std::string>());
            if (_debug_error_check_mode_renderer) {
                spdlog::info("[renderer][out:{}]已经替换:{}->{}",out , rege, utils::utf8ToAnsi(item.value()[1].get<std::string>()));
                out--;
                if (out <= 0) {
                    utils::WriteFile(fs::path(Base / "renderer.js").string(), renderer_str);
                    spdlog::info("已写入. 你希望下次替换多少条后写入:");
                    std::cin >> out;
                }
            }
        }
        if (!_debug_invalid_check_mode && !_debug_no_replace_res) {
            // 写入
            utils::WriteFile(fs::path(Base / "renderer.js").string(), renderer_str);
        }
        spdlog::info("{} 汉化结束.", "renderer.js");
    }


    PAUSE;
    return ret_num;
}

/**
 * @brief 获取当前用户的sid
 * @return 
 */
std::string GetCurrentUserSid() {
    const int MAX_NAME = 260;
    char userName[MAX_NAME] = "";
    char sid[MAX_NAME] = "";
    DWORD nameSize = sizeof(userName);
    GetUserName((LPSTR)userName, &nameSize);


    char userSID[MAX_NAME] = "";
    char userDomain[MAX_NAME] = "";
    DWORD sidSize = sizeof(userSID);
    DWORD domainSize = sizeof(userDomain);


    SID_NAME_USE snu;
    LookupAccountName(NULL,
        (LPSTR)userName,
        (PSID)userSID,
        &sidSize,
        (LPSTR)userDomain,
        &domainSize,
        &snu);


    PSID_IDENTIFIER_AUTHORITY psia = GetSidIdentifierAuthority(userSID);
    sidSize = sprintf(sid, "S-%lu-", SID_REVISION);
    sidSize += sprintf(sid + strlen(sid), "%-lu", psia->Value[5]);


    int i = 0;
    int subAuthorities = *GetSidSubAuthorityCount(userSID);


    for (i = 0; i < subAuthorities; i++)
    {
        sidSize += sprintf(sid + sidSize, "-%lu", *GetSidSubAuthority(userSID, i));
    }
    return std::string(sid);
}

bool GetBasePath(std::string& out) {
    getline(std::cin, out);
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
        fs::copy_file(base / "main.js.bak", base / "main.js");
        spdlog::warn("main.js 未找到, 但已从备份main.js.bak中还原");
    }

    fs::path rendererjs = "renderer.js";
    fs::path rendererjsbak = "renderer.js.bak";
    if (!fs::exists(base / rendererjs)) {
        if (!fs::exists(base / rendererjsbak)) {
            spdlog::warn("目录有误，找不到目录下的renderer.js. ");
            return false;
        }
        fs::copy_file(base / "renderer.js.bak", base / "renderer.js");
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
        else {
            spdlog::info("重新输入.");
        }
    }
}

void DeveloperOptions() {
    while (true)
    {
        system("cls");
        spdlog::info("选择你要修改的功能");
        spdlog::info("0) 跳出.");
        spdlog::info("1) [{}] main崩溃调试.", _debug_error_check_mode_main);
        spdlog::info("2) [{}] renderer崩溃调试.", _debug_error_check_mode_renderer);
        spdlog::info("3) [{}] 翻译项失效检测.", _debug_invalid_check_mode);
        spdlog::info("4) [{}] 不替换资源.不干预其他开发者选项.", _debug_no_replace_res);
        spdlog::info("5) [{}] 优先从备份文件中汉化(会直接改变资源文件的来源,影响其他选项).", _debug_translation_from_bak);
        spdlog::info("6) [{}] 仅替换指定映射项，以优化汉化作者替换时间", _debug_dev_replace);
        std::cout << std::endl;

        int sys = 0;
        //int sw = 0;	//功能开关
        spdlog::info("请输入你要修改的功能:");
        std::cin >> sys;
        switch (sys)
        {
        case 0:
            return;
        case 1:
            //spdlog::info("输入你要切换的状态(0关 1开):");
            //std::cin >> sw;
            //_debug_error_check_mode_main = (bool)sw;
            _debug_error_check_mode_main = utils::ReadUserInput_bool({ "false", "true" });
            break;
        case 2:
            //spdlog::info("输入你要切换的状态(0关 1开):");
            //std::cin >> sw;
            _debug_error_check_mode_renderer = utils::ReadUserInput_bool({ "false", "true" });
            break;
        case 3:
            //spdlog::info("输入你要切换的状态(0关 1开):");
            //std::cin >> sw;
            _debug_invalid_check_mode = utils::ReadUserInput_bool({ "false", "true" });
            break;
        case 4:
            //spdlog::info("输入你要切换的状态(0关 1开):");
            //std::cin >> sw;
            _debug_no_replace_res = utils::ReadUserInput_bool({ "false", "true" });
            break;
        case 5:
            //spdlog::info("输入你要切换的状态(0关 1开):");
            //std::cin >> sw;
            _debug_translation_from_bak = utils::ReadUserInput_bool({ "false", "true" });
            break;
        case 6:
            //spdlog::info("输入你要切换的状态(0关 1开):");
            //std::cin >> sw;
            _debug_dev_replace = utils::ReadUserInput_bool({ "false", "true" });
            break;
        }
    }
    
}


std::optional<std::string> formatTime(std::string time_str) {
    using namespace std::chrono;
    std::optional<std::string> ret;
    std::istringstream ss{ time_str };

    sys_seconds tp;
    ss >> parse("%FT%TZ", tp);
    if(!ss.fail()) {
        auto sctp = time_point_cast<seconds>(tp);
        std::time_t cftime = decltype(sctp)::clock::to_time_t(sctp);
        std::tm* tm = std::localtime(&cftime);
        std::ostringstream oss;
        oss << std::put_time(tm, "%Y年%m月%d日 %H时%M分%S秒");
        ret = oss.str();
    }
    return ret;
}