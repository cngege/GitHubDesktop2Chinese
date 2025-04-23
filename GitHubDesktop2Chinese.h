// GitHubDesktop2Chinese.h: 标准系统包含文件的包含文件
// 或项目特定的包含文件。

#pragma once

#include <iostream>
#include <fstream>

// TODO: 在此处引用程序需要的其他标头。

//std::string GetCurrentUserSid();

bool GetBasePath(std::string& out);
std::string LoopGetBasePath();
void DeveloperOptions();