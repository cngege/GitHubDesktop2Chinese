#ifndef VERSIONPARSE_VERSION_HPP
#define VERSIONPARSE_VERSION_HPP


#pragma once
#include <string>

namespace std {

class Version {
public:
    enum Status {
        Dev,            /*开发版*/
        Beta,           /*测试版*/
        Release         /*正式发布版*/
    };
    int major = 0;
    int minor = 0;
    int revision = 0;
    int betaversion = 0;
    Status status = Status::Release;

private:
    enum ParseCheckPosition {
        enummajor,          /*反序列化标志位 主版本号*/
        enumminor,          /*反序列化标志位 次版本号*/
        enumrevision,       /*反序列化标志位 修订号*/
        enumbetaversion,    /*反序列化标志位 测试版本号*/
    };
    ParseCheckPosition parseCheckPos = ParseCheckPosition::enummajor;
    bool vaild = false;
public:
	Version(const char* v) {
        vaild = Parse(v);
	}

    Version(int major, int minor, int revision, Status status = Status::Release, int betaversion = 0)
    : major(major), minor(minor), revision(revision), status(status), betaversion(betaversion)
    {
        vaild = true;
    }

public:
    bool operator==(const Version& b) const {
        if (major != b.major) return false;
        if (minor != b.minor) return false;
        if (revision != b.revision) return false;
        // 如果双方都为dev 或 beta版 则进行内部版本号的比较
        if (this->status == b.status && this->status != Status::Release) {
            if (betaversion != b.betaversion) return false;
        }
        return true;
    }

    bool operator<=(const Version& b) const {
        if (major > b.major) return false;
        if (minor > b.minor) return false;
        if (revision > b.revision) return false;
        // 如果双方都为dev 或 beta版 则进行内部版本号的比较
        if (this->status == b.status && this->status != Status::Release) {
            if (betaversion > b.betaversion) return false;
        }
        return true;
    }

    bool operator<(const Version& b) const {
        return *this <= b && *this != b;
    }

    bool operator>=(const Version& b) const {
        return b <= *this;
    }

    bool operator>(const Version& b) const {
        return b < *this;
    }

    operator bool() const {
        return vaild;
    }

public:
    /**
     * @brief 转为字符串形式方便阅读
     * @param hasStatus 是否显示版本类型
     * @param hasBetaVer 是否显示测试版本号
     * @return 返回格式为 0.0.0 或 0.0.0 - Beta 或 0.0.0.0 - Beta
    */
    std::string toString(bool hasStatus = false, bool hasBetaVer = false) {
        std::string ret = std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(revision);
        if (hasBetaVer) {
            ret += "." + std::to_string(betaversion);
        }
        if (hasStatus) {
            if (status == Status::Dev)  ret += " - Dev";
            if (status == Status::Beta)  ret += " - Beta";
            if (status == Status::Release)  ret += " - Release";
        }
        return ret;
    }

    /**
     * @brief 返回类似 v1.40.100-alpha.456 的版本结构
     * @return 
    */
    std::string toDetailedString() {
        std::string ret = "v" + std::to_string(major) + "." + std::to_string(minor);
        if (revision != 0) {
            ret += "." + std::to_string(revision);
        }
        if (status == Status::Dev)  ret += "-dev";
        if (status == Status::Beta)  ret += "-beta";
        if (status != Status::Release) {
            ret += "." + std::to_string(betaversion);
        }
        return ret;
    }

private:
    /**
     * @brief 将a~z的字符转为大写,其他字符原样返回
     * @param c 字符 a~z
     * @return 返回大写的A~Z
    */
    char ToUpper(char c) {
        if (c >= 97 && c <= 122) { // a ~ z
            return c - 32;
        }
        return c;
    }

    /**
     * @brief 将A~Z的字符转为小写,其他字符原样返回
     * @param c 字符 A~Z
     * @return 返回小写的a~z
    */
    char ToLower(char c) {
        if (c >= 65 && c <= 90) {  // A ~ Z
            return c + 32;
        }
        return c;
    }

    /**
     * @brief 检查版本是否是alpha beta
     * 
     * @param sourcevalue
     * @param checkvalue            判断字符是否和这个相等 必须小写 这个数不能是空字符串 因为没有意义
     * @return                      <0: 遇到了 \0, 0: 不是, >0 是
    */
    int CheckIsAlphaOrBeta(const char* sourcevalue, const char* checkvalue) {
        int _increase = 1;          // 如果返回值大于0 则返回这个数,来告诉调用者应该将原始字节偏移多少才能跳过这个检查值
        for (const char* _v = sourcevalue; *_v != '\0'; _v++)
        {
            if (ToLower(*_v) != *checkvalue) {
                return 0;
            }
            if (*(checkvalue + 1) == '\0') {
                return _increase;
            }
            if (*(checkvalue + 1) == ' ') { // 空格跳过
                _increase++;
                continue;
            }
            checkvalue++;
            _increase++;
        }
        return -1;
    }

    /**
     * @brief 反序列化 将字符串反序列化为内部参数
     * @param v 
     * @return 是否读出了版本号, 至少要读出两位版本号(1.2)
    */
    bool Parse(const char* v) {
        bool ret = false;
        for (;;) {
            if (*v == '\0') {
                break;
            }
            // parseCheckPos == ParseCheckPosition::enummajor
            if (parseCheckPos == ParseCheckPosition::enummajor) {
                if (*v == 'v' || *v == 'V') {
                    v++;
                    continue;
                }
                if (*v == '.') {
                    parseCheckPos = ParseCheckPosition::enumminor;
                    v++;
                    continue;
                }
                if (isdigit(*v) != 0) {
                    major = major * 10 + (*v - '0');
                    v++;
                    continue;
                }
                // 到这里不可能是. \\n 数字
                // 所以只能是非法字符(为了兼容性,将空格放过)
                if (*v == ' ') {
                    v++;
                    continue;
                }
                // 非法字符 反序列化失败
                break;
            }
            // parseCheckPos == ParseCheckPosition::minor
            if (parseCheckPos == ParseCheckPosition::enumminor) {
                if (*v == '.') {
                    parseCheckPos = ParseCheckPosition::enumrevision;
                    v++;
                    continue;
                }
                if (isdigit(*v) != 0) {
                    minor = minor * 10 + (*v - '0');
                    v++;
                    ret = true;
                    continue;
                }
                if (*v == '-' || *v == '_') {
                    v++;

                    // 检查是否是 alpha 或者 dev
                    int ret = CheckIsAlphaOrBeta(v, "alpha");
                    if(!ret) ret = CheckIsAlphaOrBeta(v, "dev");
                    if (ret < 0) break;
                    if (ret > 0) { // 是 alpha 内部测试版 -> Dev
                        status = Status::Dev;
                        parseCheckPos = ParseCheckPosition::enumbetaversion;
                        v += ret;
                        if (*v == '.') {
                            v++;
                        }
                        continue;
                    }
                    // 检查是否是 beta
                    ret = CheckIsAlphaOrBeta(v, "beta");
                    if (ret < 0) break;
                    if (ret > 0) { // 是 beta
                        status = Status::Beta;
                        parseCheckPos = ParseCheckPosition::enumbetaversion;
                        v += ret;
                        if (*v == '.') {
                            v++;
                        }
                        continue;
                    }
                }
                v++;
                continue;
            }
            // parseCheckPos == ParseCheckPosition::enumrevision
            if (parseCheckPos == ParseCheckPosition::enumrevision) {
                if (*v == '.') {
                    parseCheckPos = ParseCheckPosition::enumbetaversion;
                    v++;
                    continue;
                }
                if (isdigit(*v) != 0) {
                    revision = revision * 10 + (*v - '0');
                    v++;
                    continue;
                }
                if (*v == '-' || *v == '_') {
                    v++;

                    // 检查是否是 alpha 或者 dev
                    int ret = CheckIsAlphaOrBeta(v, "alpha");
                    if (!ret) ret = CheckIsAlphaOrBeta(v, "dev");
                    if (ret < 0) break;
                    if (ret > 0) { // 是 alpha 内部测试版 -> Dev
                        status = Status::Dev;
                        parseCheckPos = ParseCheckPosition::enumbetaversion;
                        v += ret;
                        if (*v == '.') {
                            v++;
                        }
                        continue;
                    }
                    // 检查是否是 beta
                    ret = CheckIsAlphaOrBeta(v, "beta");
                    if (ret < 0) break;
                    if (ret > 0) { // 是 beta
                        status = Status::Beta;
                        parseCheckPos = ParseCheckPosition::enumbetaversion;
                        v += ret;
                        if (*v == '.') {
                            v++;
                        }
                        continue;
                    }
                }
                v++;
                continue;
            }
            // parseCheckPos == ParseCheckPosition::enumbetaversion
            if (parseCheckPos == ParseCheckPosition::enumbetaversion) {
                // 这里遇到'.'就结束了, 版本字符串的结构只能支持到 0.0.0.0
                if (*v == '.') {
                    break;
                }
                if (isdigit(*v) != 0) {
                    if (status == Status::Release && ((*v - '0') != 0)) status = Status::Beta;
                    betaversion = betaversion * 10 + (*v - '0');
                    v++;
                    continue;
                }

                v++;
                continue;
            }
        }
        return ret;
    }
};

}


#endif // !VERSIONPARSE_VERSION_HPP
