// GitHubDesktop2Chinese.cpp: 定义应用程序的入口点。


#define _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING	//消除 converter.to_bytes的警告
#define _CRT_SECURE_NO_WARNINGS								//消除 sprintf的警告

#define PAUSE if(!no_pause) system("pause")

#include "GitHubDesktop2Chinese.h"
#include <string>
#include <filesystem>

#include <regex>

#include "spdlog/spdlog.h"			// 日志式输出库
#include "nlohmann/json.hpp"		// JSON读取本地配置库
#include "WinReg/WinReg.hpp"		// 注册表操作库

#include <CLI/CLI.hpp>				// 参数管理器:		https://github.com/CLIUtils/CLI11
#include "Utils/utils.hpp"


#if _DEBUG
#define NO_REPLACE 0
#endif // _DEBUG




namespace fs = std::filesystem;
using json = nlohmann::json;

// 设置一个路径的全局变量  指向要修改JS的目录
fs::path Base;
fs::path LocalizationJSON;

bool no_pause;									// 程序在结束前是否暂停
bool only_read_from_remote;						// 仅从远程url中读取本地化文件

json localization = R"(
						{
							"main": [
								["XXXXX", "XXXXX"]
							],
							"renderer": [
								["xxxxx","xxxxx"]
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

// argv[0] 是程序路径
int main(int argc, char* argv[])
{
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
		dev_cmd->add_flag("-d,--dev", _debug_goto_devoptions, "进入开发者选项调整功能(可在开启程序时按住shift直接进入)");
		dev_cmd->add_flag("--mainerrorcheck", _debug_error_check_mode_main,			"[错误检查模式]对main.js以错误检查模式进行排查");
		dev_cmd->add_flag("--rendererrorcheck", _debug_error_check_mode_renderer,	"[错误检查模式]对renderer.js以错误检查模式进行排查");
		dev_cmd->add_flag("--invalidcheck", _debug_invalid_check_mode,				"[检测失效项]对本地化文件中的失效替换项进行检测");
		dev_cmd->add_flag("--noreplaceres", _debug_no_replace_res,					"[资源不替换]开启后不会对资源进行替换,但不会阻止[错误检查模式]");
		dev_cmd->add_flag("--translationfrombak", _debug_translation_from_bak,		"[从备份文件中读取替换]优先从备份文件中读取js文件内容进行替换,开启[检测失效项]时建议开启此项");
		dev_cmd->add_flag("--devreplace", _debug_dev_replace,						"[开发模式替换]仅替换指定映射以节约汉化时间(会影响其他项)");


		app.add_flag("--nopause", no_pause,							"程序在结束前不再暂停等待");
		app.add_flag("-r,--onlyfromremote", only_read_from_remote,	"仅从远程url中读取本地化文件");
		app.add_option("-j,--json", LocalizationJSON,				"指定本地化JSON文件的本地路径");
		app.add_option("-g,--githubdesktoppath", Base,				"指定GitHubDesktop要汉化的资源所在目录");
		
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

	// 开发者声明
	spdlog::info("开发者：CNGEGE>2024/04/13");
	
	if (GetKeyState(VK_SHIFT) & 0x8000 || _debug_goto_devoptions) {
		// 如果Shift按下, 则进入开发者选项
		SetConsoleTitle("开发者模式");
		spdlog::info("您已进入开发者模式");
		DeveloperOptions();
	}

	if (LocalizationJSON.empty()) {
		LocalizationJSON = "localization.json";
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
				spdlog::warn("远程获取失败: {}, 已创建框架,请先编辑创建翻译映射", "localization.json");
				std::ofstream io(LocalizationJSON);
				io << std::setw(4) << localization << std::endl;
				io.close();
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
					spdlog::warn("注册表最终获取到的目录不存在,请手动输入");
					Base = LoopGetBasePath();
				}

			}
			catch (const winreg::RegException& regerr)
			{
				spdlog::error("{} at line: {}", regerr.what(), __LINE__);
				PAUSE;
				return 0;
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
			return 0;
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
			return 0;
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



	// TODO 读取main.js文件
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
				}
				continue;
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



	// TODO 读取renderer.js文件
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
				}
				continue;
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
	return 0;
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
		int sw = 0;	//功能开关
		spdlog::info("请输入你要修改的功能:");
		std::cin >> sys;
		switch (sys)
		{
		case 0:
			return;
		case 1:
			spdlog::info("输入你要切换的状态(0关 1开):");
			std::cin >> sw;
			_debug_error_check_mode_main = (bool)sw;
			break;
		case 2:
			spdlog::info("输入你要切换的状态(0关 1开):");
			std::cin >> sw;
			_debug_error_check_mode_renderer = (bool)sw;
			break;
		case 3:
			spdlog::info("输入你要切换的状态(0关 1开):");
			std::cin >> sw;
			_debug_invalid_check_mode = (bool)sw;
			break;
		case 4:
			spdlog::info("输入你要切换的状态(0关 1开):");
			std::cin >> sw;
			_debug_no_replace_res = (bool)sw;
			break;
		case 5:
			spdlog::info("输入你要切换的状态(0关 1开):");
			std::cin >> sw;
			_debug_translation_from_bak = (bool)sw;
			break;
		case 6:
			spdlog::info("输入你要切换的状态(0关 1开):");
			std::cin >> sw;
			_debug_dev_replace = (bool)sw;
			break;
		}
	}
	
}
