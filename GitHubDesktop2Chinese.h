// GitHubDesktop2Chinese.h: 标准系统包含文件的包含文件
// 或项目特定的包含文件。

#pragma once

#include <iostream>
#include <fstream>

// TODO: 在此处引用程序需要的其他标头。

std::string GetCurrentUserSid();

inline std::wstring to_wide_string(const std::string& input);
inline std::string to_byte_string(const std::wstring& input);

bool GetBasePath(std::string& out);
std::string LoopGetBasePath();
std::string ReadFile(const std::string& filename);
void DeveloperOptions();
std::string ansiToUtf8(const std::string& ansiString);
std::string utf8ToAnsi(const std::string& utf8String);
