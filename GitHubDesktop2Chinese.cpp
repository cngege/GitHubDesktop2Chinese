// GitHubDesktop2Chinese.cpp: 定义应用程序的入口点。
//
#define _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING

#include "GitHubDesktop2Chinese.h"
#include <string>
#include <filesystem>
#include <codecvt>
#include <regex>

#include "spdlog/spdlog.h"			// 日志式输出库
#include "nlohmann/json.hpp"		// JSON读取本地配置库
#include "WinReg/WinReg.hpp"		// 注册表操作库


namespace fs = std::filesystem;
using json = nlohmann::json;

// 设置一个路径的全局变量  指向要修改JS的目录
fs::path Base;
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



int main()
{
	// 设置控制台打印日志输出等级
	spdlog::set_level(spdlog::level::debug);
	// 开发者声明
	spdlog::info("开发者：CNGEGE<2024/04/13");

	// 判断汉化映射文件是否存在, 不存在则创建一个
	if (!fs::exists("localization.json")) {
		std::ofstream io("localization.json");
		io << std::setw(4) << localization << std::endl;
		spdlog::warn("没有发现本地化文件: {}, 已创建,请先编辑创建翻译映射", "localization.json");
		return 0;
	}


	// 首先要能够成功读取注册表
	
	//	拿到当前用户sid
	std::string sid = GetCurrentUserSid();
	spdlog::debug("sid:{}", sid);

	//	检查注册表中是否存在GithubDesktop
    winreg::RegKey key;
	winreg::RegResult result = key.TryOpen(HKEY_USERS, to_wide_string(sid) + L"\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\GitHubDesktop");
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
			spdlog::info("Github Desktop 版本: {}", to_byte_string(ver));
			spdlog::info("安装目录: {}", to_byte_string(path));
			spdlog::info("最后拼接完整目录: {}", Base.string());

			if (!fs::exists(Base)) {
				spdlog::warn("注册表最终获取到的目录不存在,请手动输入");
				Base = LoopGetBasePath();
			}

		}
		catch (const winreg::RegException& regerr)
		{
			spdlog::error("{} at line: {}",regerr.what(), __LINE__);
			return 0;
		}
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

	// 本地读取汉化文件到json中
	{
		std::ifstream config("localization.json");
		if (!config) {
			spdlog::error("localization.json 打开失败,无法读取");
		}
		config >> localization;
	}

	// TODO 读取main.js文件
	{
		std::string main_str = ReadFile(fs::path(Base / "main.js").string());
		for (auto& item : localization["main"].items())
		{
			std::regex pattern(item.value()[0]);
			main_str = std::regex_replace(main_str, pattern, item.value()[1].get<std::string>());
		}
		// 写入
		std::ofstream override_main(fs::path(Base / "main.js").string(), std::ios::binary);
		override_main.write(main_str.c_str(), main_str.size());
		if (!override_main) {
			spdlog::error("打开并写入目标文件:{} 时失败", "main.js");
		}
		override_main.close();
		spdlog::info("{} 汉化结束.", "main.js");
	}



	// TODO 读取renderer.js文件
	{
		std::string renderer_str = ReadFile(fs::path(Base / "renderer.js").string());
		for (auto& item : localization["renderer"].items())
		{
			std::regex pattern(item.value()[0]);
			renderer_str = std::regex_replace(renderer_str, pattern, item.value()[1].get<std::string>());
		}
		// 写入
		std::ofstream override_renderer(fs::path(Base / "renderer.js").string(), std::ios::binary);
		override_renderer.write(renderer_str.c_str(), renderer_str.size());
		if (!override_renderer) {
			spdlog::error("打开并写入目标文件:{} 时失败", "renderer.js");
		}
		override_renderer.close();
		spdlog::info("{} 汉化结束.", "renderer.js");
	}



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

inline std::wstring to_wide_string(const std::string& input)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
	return converter.from_bytes(input);
}

inline std::string to_byte_string(const std::wstring& input)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
	return converter.to_bytes(input);
}

bool GetBasePath(std::string& out) {
	getline(std::cin, out);
	if (!fs::exists(out)) {
		spdlog::warn("你输入的目录不存在. ");
		return false;
	}
	fs::path base = out;
	fs::path mainjs = "main.js";
	if (!fs::exists(base / mainjs)) {
		spdlog::warn("目录有误，找不到目录下的main.js. ");
		return false;
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

std::string ReadFile(const std::string& filename) {
	std::ifstream fin(filename);
	std::stringstream buffer;
	buffer << fin.rdbuf();
	std::string str(buffer.str());
	return str;
}