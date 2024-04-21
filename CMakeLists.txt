# CMakeList.txt: GitHubDesktop2Chinese 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required (VERSION 3.8)

# 如果支持，请为 MSVC 编译器启用热重载。
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("GitHubDesktop2Chinese")

include(FetchContent)

include_directories(include)
include_directories(Utils)

# 包含 openssl 的头文件
include_directories(openssl-proj/include)

# 将源代码添加到此项目的可执行文件。
add_executable (GitHubDesktop2Chinese "GitHubDesktop2Chinese.cpp")


# 链接OpenSSL库  
target_link_libraries(GitHubDesktop2Chinese PRIVATE
	${CMAKE_SOURCE_DIR}/openssl-proj/lib/libssl.lib
	${CMAKE_SOURCE_DIR}/openssl-proj/lib/libcrypto.lib)
  

# 动态引入链接 CLI参数管理器库
FetchContent_Declare(
    cli11_proj
    QUIET
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.4.1
)
FetchContent_MakeAvailable(cli11_proj)
target_link_libraries(GitHubDesktop2Chinese PRIVATE CLI11::CLI11)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET GitHubDesktop2Chinese PROPERTY CXX_STANDARD 20)
endif()

# TODO: 如有需要，请添加测试并安装目标。
