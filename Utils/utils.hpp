#ifndef UTILS_IS_EXITES
#define UTILS_IS_EXITES

#include <string>
#include <codecvt>
#include <tchar.h>
#include <filesystem>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "http/httplib.h"


namespace fs = std::filesystem;

class utils {
public:
    
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
    static auto ReadHttpDataString(std::string url_host, std::string params, std::string& out) -> bool {
        httplib::Client cli(url_host);
        cli.enable_server_certificate_verification(false);
        cli.set_follow_location(true);                          //https://raw.github.com 会要求301重定向
        if (auto res = cli.Get(params)) {
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
    static auto UpdateProgram(std::string url_host, std::string params, fs::path Self) ->bool{
        fs::path parent_dir = Self.parent_path();   // 文件所在目录
        fs::path stem = Self.filename();                // 文件名 不包含扩展名

        //printf_s(url_host.c_str()); printf_s("\n");
        //printf_s(params.c_str());        printf_s("\n");
        //printf_s(stem.string().c_str());        printf_s("\n");
        //printf_s(stem.string().c_str());        printf_s("\n");

        httplib::Client cli(url_host);
        cli.enable_server_certificate_verification(false);
        cli.set_follow_location(true);                          //https://raw.github.com 会要求301重定向
        auto res = cli.Get(params, [](uint64_t len, uint64_t total) {
            printf_s("\r %d%% ==>  %lld / %lld", (int)(len * 100 / total), len, total);
            return true;
        });
        printf_s("\n");
        if(res && res->status == httplib::StatusCode::OK_200) {
            // 讲数据写入到本地文件中
            std::ofstream downfile(Self.string() + ".new", std::ios::binary | std::ios::out | std::ios::trunc);
            if(downfile.is_open()) {
                printf_s("下载完成, 请稍等, 随后自动完成并(无参)重启.. \n");
                // 写入文件
                downfile.write(res->body.data(), res->body.length());
                downfile.flush();
                downfile.close();
                // 完成后创建进程
                // 构建参数
                std::string p = "/c \"ping 127.0.0.1 -n 6 > nul & move /Y ";
                p += stem.string() + ".new ";
                p += stem.string();
                p += " & start ";
                p += stem.string();
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
            else {
                return false;
            }
        }
        else {
            return false;
        }
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
            std::cin >> instr;

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
