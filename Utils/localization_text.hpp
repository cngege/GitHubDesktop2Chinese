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
#include <vector>

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

//////////////////////////////////////////////////////////////////////////
// 预览版特性 / 未完成菜单项防护（#48 / #49 的残留场景）
//
// 旧版映射文件（v1.2.4 及更早）里有 select 规则强制开启 GitHub Desktop 的
// 预览特性开关：
//   main.js     : ...GITHUB_DESKTOP_PREVIEW_FEATURES||!1  ->  ||!0
//   renderer.js : ...GITHUB_DESKTOP_PREVIEW_FEATURES}      ->  /*...*/"1"}
// 从 3.6.10 起，该开关会让主进程暴露未完成的菜单项（如 open-in-copilot-app），
// renderer 的 fatal-error.ts 遇到未知菜单 id 会直接崩溃。
//
// #48 只是把映射文件里的 enable 改成 false，并让 loader 跳过关键字缺失的规则；
// 手动把 enable 改回 true、或使用很老的本地 json 时仍会触发。
// 这类规则与「中文本地化」无关，因此在 loader 侧无条件禁用：
// 不依赖 enable 值、不依赖映射文件版本、也不依赖资源文件里是否有匹配关键字。
//////////////////////////////////////////////////////////////////////////

inline constexpr const char* kPreviewFeatureToken = "GITHUB_DESKTOP_PREVIEW_FEATURES";

// 未完成 / 未对外发布的菜单 id：即使某条规则没碰预览开关，也不允许写进资源文件。
inline constexpr const char* const kIncompleteMenuIds[] = {
    "open-in-copilot-app",
};

// 判定单条 [匹配, 替换] 规则是否会把预览特性强制置真或注入未完成菜单项。
inline bool is_unsafe_feature_rule(const std::string& pattern, const std::string& replacement) {
    if(pattern.find(kPreviewFeatureToken) != std::string::npos) {
        return true;
    }
    if(replacement.find(kPreviewFeatureToken) != std::string::npos) {
        return true;
    }
    for(const char* id : kIncompleteMenuIds) {
        if(replacement.find(id) != std::string::npos) {
            return true;
        }
    }
    return false;
}

// 判定一个 select 条目的有效替换规则是否「全部」被禁用。
// 返回 true 时 loader 应整条跳过：既不向用户弹出确认提示，也不写入任何内容。
// 只要还存在一条可用规则就返回 false，保持原有的询问与替换流程。
// 空规则（["",""] 等）不参与判定，避免改变历史 json 里空条目的既有行为。
// 只有匹配串、没有替换串的规则无法注入任何内容，既不是有效的注入规则也不
// 是有效翻译，不应把它算作「被禁用」：否则会误伤同条目下其它可用规则，并把
// 整条 select 判为可跳过。这类畸形规则交给 loader 的逐条过滤（越界访问已防护），
// 既不会写入、也不会崩。
inline bool select_rules_all_blocked(const std::vector<std::vector<std::string>>& rules) {
    bool has_blocked = false;
    for(const auto& rule : rules) {
        if(rule.empty() || rule[0].empty() || rule[0] == "\"\"") {
            continue;
        }
        // 没有替换串：无法注入，也不参与「全部被禁用」判定。
        if(rule.size() < 2 || rule[1].empty()) {
            return false;
        }
        if(is_unsafe_feature_rule(rule[0], rule[1])) {
            has_blocked = true;
        }
        else {
            return false;
        }
    }
    return has_blocked;
}

} // namespace localization_text

#endif // UTILS_LOCALIZATION_TEXT_HPP
