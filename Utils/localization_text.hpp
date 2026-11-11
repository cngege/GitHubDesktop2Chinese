#ifndef UTILS_LOCALIZATION_TEXT_HPP
#define UTILS_LOCALIZATION_TEXT_HPP

// 本地化映射的文本处理。
//
// 映射条目的第三个参数是"查找用正则"，匹配到的捕获组会按 #{n} 占位符填进
// 第二个参数（替换文本）。这里把该逻辑抽成纯函数，便于单元测试覆盖。
//
// 关键点：捕获到的内容是"数据"而不是"格式串"。替换文本随后会作为
// std::regex_replace 的格式串使用，其中的 '$' 有特殊含义（$1/$&/$$），
// 因此捕获内容里的 '$' 必须先转义成 "$$"，否则会被当成反向引用而丢弃。

#include <cctype>
#include <regex>
#include <string>

namespace localization_text {

inline std::string substitute_capture_groups(const std::string& tmpl, const std::smatch& match) {
    std::string out;
    out.reserve(tmpl.size());
    for (size_t i = 0; i < tmpl.size();) {
        if (tmpl.compare(i, 2, "#{") == 0) {
            size_t j = i + 2;
            size_t index = 0;
            bool has_digit = false;
            while (j < tmpl.size() && std::isdigit(static_cast<unsigned char>(tmpl[j]))) {
                index = index * 10 + static_cast<size_t>(tmpl[j] - '0');
                has_digit = true;
                ++j;
            }
            if (has_digit && j < tmpl.size() && tmpl[j] == '}' && index < match.size()) {
                for (char c : match[index].str()) {
                    if (c == '$') {
                        out += "$$";
                    }
                    else {
                        out += c;
                    }
                }
                i = j + 1;
                continue;
            }
        }
        out += tmpl[i];
        ++i;
    }
    return out;
}

} // namespace localization_text

#endif // UTILS_LOCALIZATION_TEXT_HPP
