#ifndef UTILS_IS_EXITES
#define UTILS_IS_EXITES

#include <string>
#include <codecvt>
#include <tchar.h>
#include <filesystem>
#include <windows.h>
#include <winhttp.h>
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "http/httplib.h"
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

class utils {
public:
    static std::string GetEnvVar(const std::string& varName) {
        // Windows API 优先使用宽字符版本，避免编码问题
        std::wstring wVarName(varName.begin(), varName.end());
        wchar_t* wValue = _wgetenv(wVarName.c_str());

        if(wValue == nullptr) return "";

        // 转换 UTF-16 到 UTF-8
        int len = WideCharToMultiByte(CP_UTF8, 0, wValue, -1, nullptr, 0, nullptr, nullptr);
        std::string value(len, 0);
        WideCharToMultiByte(CP_UTF8, 0, wValue, -1, &value[0], len, nullptr, nullptr);
        value.pop_back(); // 移除末尾的空字符
        return value;
    }

    static std::optional<std::pair<std::string, int>> get_proxy_env() {
        auto p1 = GetEnvVar("HTTPS_PROXY");
        if(p1.empty()) p1= GetEnvVar("https_proxy");

        if(p1.empty()) {
            return {};
        }
        size_t protocolEnd = p1.find("://");
        std::string addrWithoutProtocol = (protocolEnd != std::string::npos)
            ? p1.substr(protocolEnd + 3)
            : p1;

        // 2. 分割主机和端口（以第一个 : 为界）
        size_t colonPos = addrWithoutProtocol.find(':');
        if(colonPos == std::string::npos) {
            std::cerr << "错误：代理地址无端口号！" << std::endl;
            return {};
        }
        // 3. 提取主机和端口
        int port = 0;
        std::string host = addrWithoutProtocol.substr(0, colonPos);
        try {
            port = std::stoi(addrWithoutProtocol.substr(colonPos + 1));
        }
        catch(const std::exception& e) {
            std::cerr << "错误：端口号格式无效 - " << e.what() << std::endl;
            return {};
        }
        if(port) {
            return std::make_pair(host, port);
        }
        return {};
    }

    static std::optional<std::pair<std::string, int>> GetSystemProxySettings() {
        std::string address;
        int port = 0;
        // Windows 实现
        WINHTTP_CURRENT_USER_IE_PROXY_CONFIG ieProxyConfig = { 0 };

        if(WinHttpGetIEProxyConfigForCurrentUser(&ieProxyConfig)) {
            if(ieProxyConfig.lpszProxy) {
                //config.enabled = true;
                std::wstring proxyW(ieProxyConfig.lpszProxy);
                address = { proxyW.begin(), proxyW.end() };
                
                // 尝试解析端口 (格式: address:port 或 http=address:port)
                size_t pos = address.find(':');
                if(pos != std::string::npos) {
                    try {
                        port = std::stoi(address.substr(pos + 1));
                        address = address.substr(0, pos);
                    }
                    catch(...) {
                        // 端口解析失败
                    }
                }
            }

            // 清理资源
            if(ieProxyConfig.lpszProxy) GlobalFree(ieProxyConfig.lpszProxy);
            if(ieProxyConfig.lpszProxyBypass) GlobalFree(ieProxyConfig.lpszProxyBypass);
            if(ieProxyConfig.lpszAutoConfigUrl) GlobalFree(ieProxyConfig.lpszAutoConfigUrl);
        }
        if(port) {
            return std::make_pair(address, port);
        }
        return {};
    }

    static auto ansiToUtf8(const std::string& ansiString) -> std::string {
        int ansiSize = static_cast<int>(ansiString.size());
        int utf8Size = MultiByteToWideChar(CP_ACP, 0, ansiString.c_str(), ansiSize, nullptr, 0);
        std::vector<wchar_t> wideString(utf8Size);
        MultiByteToWideChar(CP_ACP, 0, ansiString.c_str(), ansiSize, wideString.data(), utf8Size);
        utf8Size = WideCharToMultiByte(CP_UTF8, 0, wideString.data(), utf8Size, nullptr, 0, nullptr, nullptr);
        std::vector<char> utf8String(utf8Size);
        WideCharToMultiByte(CP_UTF8, 0, wideString.data(), utf8Size, utf8String.data(), utf8Size, nullptr, nullptr);
        return std::string(utf8String.begin(), utf8String.end());
    }

    static auto utf8ToAnsi(const std::string& utf8String) -> std::string {
        int utf8Size = static_cast<int>(utf8String.size());
        int ansiSize = MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), utf8Size, nullptr, 0);
        std::vector<wchar_t> wideString(ansiSize);
        MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), utf8Size, wideString.data(), ansiSize);
        ansiSize = WideCharToMultiByte(CP_ACP, 0, wideString.data(), ansiSize, nullptr, 0, nullptr, nullptr);
        std::vector<char> ansiString(ansiSize);
        WideCharToMultiByte(CP_ACP, 0, wideString.data(), ansiSize, ansiString.data(), ansiSize, nullptr, nullptr);
        return std::string(ansiString.begin(), ansiString.end());
    }

    inline static auto to_wide_string(const std::string& input)-> std::wstring
    {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        return converter.from_bytes(input);
    }

    inline static auto to_byte_string(const std::wstring& input)->std::string
    {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        return converter.to_bytes(input);
    }

    static auto ReadFile(const std::string& filename)->std::string {
        std::ifstream fin(filename, std::ios::binary);
        //std::stringstream buffer{};
        //buffer << fin.rdbuf();
        //std::string str(buffer.str());
        return std::string(
            std::istreambuf_iterator<char>(fin),
            std::istreambuf_iterator<char>()
        );
        //return str;
    }

    static auto WriteFile(const std::string& filename, std::string& txt) -> void {
        std::ofstream override (filename, std::ios::binary);
        override.write(txt.c_str(), txt.size());
        if (!override) {
            spdlog::error("打开并写入目标文件:{} 时失败", filename);
        }
        override.close();
    }

    /**
     * @brief 从网络上读取数据
     * @param url_host 网络主机
     * @param params 请求链接后缀
     * @param out 成功后输出读取到的数据
     * @return 是否成功
     */
    static auto ReadHttpDataString(std::string url_host, std::string params, std::string& out, std::pair<std::string, int> proxy = {}) -> bool {
        httplib::Client cli(url_host);
        if(proxy.second) {
            cli.set_proxy(proxy.first, proxy.second);
        }
        cli.enable_server_certificate_verification(false);
        cli.set_follow_location(true);                          //https://raw.github.com 会要求301重定向
        auto res = cli.Get(params);
        if(!res) {
            for(int i = 0; i < 3; i++) {
                res = cli.Get(params);
                if(res) break;
            }
        }
        if (res) {
            if (res->status == httplib::StatusCode::OK_200) {
                out = res->body;
                return true;
            }
            return false;
        }
        else {
            return false;
        }
    }

    //static auto DownloadHttpResource(std::string url_host, std::string params);
    static auto UpdateProgram(std::string url_host, std::string params, fs::path Self, int64_t max_size, std::pair<std::string, int> proxy) ->bool{
        fs::path parent_dir = Self.parent_path();   // 文件所在目录
        fs::path exe_name = Self.filename();                // 文件名 包含扩展名,但不包含路径

        fs::path new_file = Self;
        fs::path tmp_file = Self;
        tmp_file += ".new"; // 下载临时文件
        //printf_s(url_host.c_str()); printf_s("\n");
        //printf_s(params.c_str());        printf_s("\n");
        //printf_s(exe_name.string().c_str());        printf_s("\n");
        //printf_s(tmp_file.string().c_str());        printf_s("\n"); return false;

            // 1. 如果临时文件已存在，获取已下载大小（用于断点续传）
        uint64_t downloaded_bytes = 0;
        if(fs::exists(tmp_file)) {
            downloaded_bytes = fs::file_size(tmp_file);
        }

        if(downloaded_bytes < max_size) {
            // 以 二进制追加模式 打开文件（断点续传关键）
            std::ofstream downfile(tmp_file, std::ios::binary | std::ios::out | std::ios::app);
            if(!downfile.is_open()) {
                spdlog::error("无法打开临时文件写入新版本");
                return false;
            }

            httplib::Client cli(url_host);
            if(proxy.second) {
                cli.set_proxy(proxy.first, proxy.second);
            }
            cli.enable_server_certificate_verification(false);
            cli.set_follow_location(true);                          //https://raw.github.com 会要求301重定向
            httplib::Headers headers;
            if(downloaded_bytes > 0) {
                headers.emplace("Range", "bytes=" + std::to_string(downloaded_bytes) + "-");
            }
            headers.emplace("Accept", "application/octet-stream");

            auto res = cli.Get(params, headers, 
            [&](const char* data, size_t data_length) {
                if(data_length > 0 && downfile.is_open()) {
                    downfile.write(data, data_length);
                    downfile.flush(); // 立即刷入磁盘，不缓存
                }
                return downfile.is_open();
            },
            [&](uint64_t len, uint64_t total) {
                uint64_t total_ = downloaded_bytes + total;  // 文件总大小
                uint64_t now_ = downloaded_bytes + len;
                int percent_ = static_cast<int>(now_ * 100 / total_);

                printf_s("\r %s %d%% ==>  %lld / %lld", (downloaded_bytes > 0) ? "[续传]" : "[下载]", percent_, now_, total_);
                return true;
            });
            printf_s("\n");
            downfile.close();

            if(!res) {
                spdlog::error("网络请求失败:{}", httplib::to_string(res.error()));
                return false;
            }
            if(res->status != httplib::StatusCode::OK_200 && res->status != httplib::StatusCode::PartialContent_206) {
                spdlog::error("更新错误, 服务器返回错误的状态码: {}", res->status);
                return false;
            }
        }

        
        spdlog::info("下载完成, 请稍等, 随后自动完成并(无参)重启..");
        // 完成后创建进程
        // 构建参数
        std::string p = "/c \"ping 127.0.0.1 -n 6 > nul & move /Y ";
        p += tmp_file.filename().string();
        p += " ";
        p += exe_name.string();
        p += " & start ";
        p += exe_name.string();
        p += "\"";
        ShellExecute(
            NULL,                   // 父窗口句柄
            _T("open"),             // 操作
            _T("cmd.exe"),          // 应用程序
            p.c_str(),              // 参数
            parent_dir.string().c_str(),                   // 工作目录
            SW_SHOW);               // 显示方式

        return true;
    }

    static auto ReadUserInput_string(std::vector<std::string> input, int defaultval = -1) -> std::string {

        while (true)
        {
            // 输出提示
            //if (defaultval == -1)
            //	spdlog::info("请输入一个表示字符串的值({})", input);
            //else
            //	spdlog::info("请输入一个表示字符串的值({} 默认 {})", input, input[defaultval]);

            std::string instr;
            std::cin >> instr;

            for (std::string& item : input)
            {
                if (item == instr) {
                    return item;
                }
            }

            if (defaultval == -1) {
                // 循环
                continue;
            }
            else {
                return input[defaultval];
            }
        }
    }

    static auto ReadUserInput_bool(std::vector<std::string> input = {"false", "true"}, int defaultval = -1) -> bool {
        if (input.size() != 2) throw std::exception("读取 bool 类型值时 input 数组长度必须为两个");
        if (defaultval > (int)input.size() - 1 || defaultval < -1) throw std::exception(std::format("defaultval 必须能够指向 input数组，或者为 -1, defaultval:{}" , defaultval).c_str());
        while (true)
        {
            // 输出提示
            if(defaultval == -1)
                spdlog::info("请输入一个表示bool的值({}/{})", input[0], input[1]);
            else
                spdlog::info("请输入一个表示bool的值({}/{} 默认 {})", input[0], input[1], input[defaultval]);

            std::string instr;
            //std::cin >> instr;
            std::cin.clear();
            if(std::cin.rdbuf()->in_avail() > 0) std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::getline(std::cin, instr);

            if (instr == input[0]) {
                return false;
            }
            else if (instr == input[1]) {
                return true;
            }
            else {
                if (defaultval == -1) {
                    // 循环
                    continue;
                }
                else if (defaultval == 0){
                    return false;
                }
                else {
                    return true;
                }
            }
        }
    }
};


#endif // !UTILS_IS_EXITES
