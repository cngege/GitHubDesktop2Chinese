#ifndef UTILS_ELEVATE_HPP
#define UTILS_ELEVATE_HPP

// 管理员提权（Windows UAC）相关的纯逻辑。
//
// 单独拆成 header 的原因：
//   1) 不依赖 Windows API（不 include windows.h），可以被单元测试直接包含；
//   2) 三个平台共用同一份判定逻辑，避免各平台实现出现语义漂变。
//
// 提权本身的系统调用在 Utils/platform_win.hpp 中实现（ShellExecuteExW + "runas"），
// 本文件只负责"要不要提权"与"跨进程传参怎么编码"这两件可测试的事。

#include <string>

namespace elevate {

/**
 * @brief 按 Windows 命令行规则转义单个参数
 *
 * 规则与 CommandLineToArgvW 的解析规则对应：
 *   - 整个参数用双引号包裹；
 *   - 参数内的反斜杠原样保留，但紧跟在双引号之前的反斜杠需要成对转义；
 *   - 参数内的双引号需要用反斜杠转义。
 *
 * 例：a b -> "a b"；a"b -> "a\"b"；C:\dir\ -> "C:\dir\\"
 */
template <typename CharT>
inline std::basic_string<CharT> quote_windows_arg(const std::basic_string<CharT>& arg) {
    std::basic_string<CharT> out;
    out.push_back(static_cast<CharT>('"'));
    std::size_t backslashes = 0;
    for(const CharT c : arg) {
        if(c == static_cast<CharT>('\\')) {
            ++backslashes;
            continue;
        }
        if(c == static_cast<CharT>('"')) {
            // 双引号前的反斜杠需要成对转义，再加一个转义双引号本身
            out.append(backslashes * 2 + 1, static_cast<CharT>('\\'));
            out.push_back(static_cast<CharT>('"'));
            backslashes = 0;
            continue;
        }
        out.append(backslashes, static_cast<CharT>('\\'));
        backslashes = 0;
        out.push_back(c);
    }
    // 结尾处的反斜杠（若紧跟收尾双引号）同样需要成对转义
    out.append(backslashes * 2, static_cast<CharT>('\\'));
    out.push_back(static_cast<CharT>('"'));
    return out;
}

/**
 * @brief 开发者选项（提权重启时需要从父进程传给子进程）
 *
 * 开发者选项是在"开发者选项"菜单里交互出来的，结果只存在进程内存中；
 * 提权重启会换成另一个进程，不传递就会丢失。
 */
struct DevOptions {
    bool error_check_main     = false;  // main.js 错误检查模式
    bool error_check_renderer = false;  // renderer.js 错误检查模式
    bool invalid_check        = false;  // 失效项检测
    bool no_replace_res       = false;  // 不替换资源
    bool translation_from_bak = false;  // 从备份文件翻译
    bool dev_replace          = false;  // 仅替换指定映射项
};

/**
 * @brief 把开发者选项编码成逗号分隔的选项名列表
 *
 * 形如 "invalidcheck,noreplaceres"。选项名都是纯 ASCII，
 * 可以安全地经命令行参数传递（不需要像路径那样做十六进制编码）。
 */
inline std::string encode_dev_options(const DevOptions& o) {
    std::string out;
    auto add = [&out](bool enabled, const char* name) {
        if(!enabled) {
            return;
        }
        if(!out.empty()) {
            out += ',';
        }
        out += name;
    };
    add(o.error_check_main,     "mainerrorcheck");
    add(o.error_check_renderer, "rendererrorcheck");
    add(o.invalid_check,        "invalidcheck");
    add(o.no_replace_res,       "noreplaceres");
    add(o.translation_from_bak, "translationfrombak");
    add(o.dev_replace,          "devreplace");
    return out;
}

/**
 * @brief encode_dev_options 的逆操作；无法识别的项直接忽略
 */
inline DevOptions decode_dev_options(const std::string& text) {
    DevOptions o;
    std::size_t pos = 0;
    while(pos <= text.size()) {
        const std::size_t end = text.find(',', pos);
        const std::string item = text.substr(pos, end == std::string::npos
                                                    ? std::string::npos
                                                    : end - pos);
        if(item == "mainerrorcheck") {
            o.error_check_main = true;
        }
        else if(item == "rendererrorcheck") {
            o.error_check_renderer = true;
        }
        else if(item == "invalidcheck") {
            o.invalid_check = true;
        }
        else if(item == "noreplaceres") {
            o.no_replace_res = true;
        }
        else if(item == "translationfrombak") {
            o.translation_from_bak = true;
        }
        else if(item == "devreplace") {
            o.dev_replace = true;
        }
        if(end == std::string::npos) {
            break;
        }
        pos = end + 1;
    }
    return o;
}

/**
 * @brief 是否需要申请管理员提权
 * @param is_elevated_child 当前进程是否是提权后重新启动的子进程
 * @param already_elevated  当前进程是否已经拥有管理员权限
 * @param targets_writable   所有写入目标在当前权限下是否都可写
 * @return 需要提权返回 true
 *
 * 规则：
 *   - 提权子进程不再重复申请（否则会无限重启）；
 *   - 已经是管理员就不必再提权；
 *   - 写入目标都可写说明当前权限足够，无需打扰用户弹 UAC。
 */
inline bool should_request_elevation(bool is_elevated_child,
                                     bool already_elevated,
                                     bool targets_writable) {
    if(is_elevated_child) {
        return false;
    }
    if(already_elevated) {
        return false;
    }
    return !targets_writable;
}

/**
 * @brief 把字节序列编码成十六进制文本（小写）
 *
 * 提权重启时需要把"父进程已解析出的资源目录"传给子进程。
 * 直接走命令行参数会经过系统的 ANSI 代码页转换，中文路径会被破坏
 * （main() 的 argv 在 Windows 上就是 ANSI 编码），因此改用纯 ASCII 的十六进制传递。
 */
inline std::string encode_hex(const std::string& bytes) {
    static const char* hex_digits = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for(const unsigned char c : bytes) {
        out.push_back(hex_digits[c >> 4]);
        out.push_back(hex_digits[c & 0x0F]);
    }
    return out;
}

/**
 * @brief 把十六进制文本解码回字节序列（encode_hex 的逆操作）
 *
 * 遇到非法字符即停止解码（已解出的部分照常返回），保证不会抛异常。
 */
inline std::string decode_hex(const std::string& hex) {
    auto hex_value = [](char c) -> int {
        if(c >= '0' && c <= '9') return c - '0';
        if(c >= 'a' && c <= 'f') return c - 'a' + 10;
        if(c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    std::string out;
    out.reserve(hex.size() / 2);
    for(std::size_t i = 0; i + 1 < hex.size(); i += 2) {
        const int hi = hex_value(hex[i]);
        const int lo = hex_value(hex[i + 1]);
        if(hi < 0 || lo < 0) {
            break;
        }
        out.push_back(static_cast<char>((hi << 4) | lo));
    }
    return out;
}

} // namespace elevate

#endif // UTILS_ELEVATE_HPP
