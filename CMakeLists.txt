# CMakeList.txt: GitHubDesktop2Chinese 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
# 平台支持：Windows（MSVC / x64 / x86）、macOS（AppleClang / arm64 / x64）
# 与 Linux（GCC / x86_64 / aarch64）。
# Windows 分支保持原有下载流程与产物不变，仅对下载启用 TLS 证书校验。
#
cmake_minimum_required (VERSION 3.30)

project ("GitHubDesktop2Chinese")

# MSVC 专有编译选项 —— 收进 if(MSVC)，macOS 不受影响
if(MSVC)
    # 如果支持，请为 MSVC 编译器启用热重载。
    if (POLICY CMP0141)
      cmake_policy(SET CMP0141 NEW)
      set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
    endif()

    # MSVC 需要显式 /utf-8，否则源码中的中文字面量按本地代码页解析
    add_compile_options(/utf-8)

    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(/bigobj)
        message("当前是以Debug模式编译")
    endif()

    # 针对 Visual Studio 编译器（MSVC）
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        # 为 Release 模式添加生成 PDB 文件的标志
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
    endif()
endif()


if(WIN32)
    if (CMAKE_GENERATOR_PLATFORM STREQUAL "x64" OR CMAKE_EXE_LINKER_FLAGS STREQUAL "/machine:x64")
    message("当前是以x64模式编译")
    add_definitions(-DCURRENT_PLATFORM_ISX64)
    set(CURRENT_PLATFORM_ISX64 1)
    else()
    message("当前是以x86模式编译")
    endif()
elseif(APPLE)
    # macOS 架构标识：arm64（Apple Silicon）与 x64（Intel）
    message("当前是以macOS模式编译")
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
        message("当前是以arm64模式编译")
        add_definitions(-DCURRENT_PLATFORM_ISARM64)
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64")
        message("当前是以x64模式编译")
        add_definitions(-DCURRENT_PLATFORM_ISX64)
        set(CURRENT_PLATFORM_ISX64 1)
    else()
        message("当前是以未知架构模式编译: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()
elseif(LINUX)
    # Linux 架构标识：x86_64 / aarch64（分别用 CURRENT_PLATFORM_ISX64 / CURRENT_PLATFORM_ISARM64）
    message("当前是以Linux模式编译")
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64")
        message("当前是以x64模式编译")
        add_definitions(-DCURRENT_PLATFORM_ISX64)
        set(CURRENT_PLATFORM_ISX64 1)
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
        message("当前是以arm64模式编译")
        add_definitions(-DCURRENT_PLATFORM_ISARM64)
    else()
        message("当前是以未知架构模式编译: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()
endif()


if(NOT DEFINED FILE_VERSION)
    set(FILE_VERSION "0.0.0-Dev.0")
endif()
add_definitions(-DFILEVERSION="${FILE_VERSION}")

include(FetchContent)

include_directories(include)
include_directories(Utils)
include_directories(${PROJECT_SOURCE_DIR})

# CLI11 下载地址：CI 环境走 GitHub 原始地址，本地走镜像
if(DEFINED GitHubActions)
    set(CLI11_URL https://github.com/CLIUtils/CLI11.git)
else()
    set(CLI11_URL https://kkgithub.com/CLIUtils/CLI11.git)
endif()


########################################################################
# Windows：openssl 预编译库下载流程（保持原有行为完全不变）
########################################################################
if(WIN32)

    if(DEFINED GitHubActions)
        if(DEFINED CURRENT_PLATFORM_ISX64)
            set(LIBSSL_URL https://github.com/cngege/cngege.github.io/releases/download/v0.1-beta.1/libssl.lib)
            set(CRYPTO_URL https://github.com/cngege/cngege.github.io/releases/download/v0.1-beta.1/libcrypto.lib)
        else()
            set(LIBSSL_URL https://github.com/cngege/cngege.github.io/releases/download/v0.1-beta.1/libssl_x86.lib)
            set(CRYPTO_URL https://github.com/cngege/cngege.github.io/releases/download/v0.1-beta.1/libcrypto_x86.lib)
        endif()
    else()
        if(DEFINED CURRENT_PLATFORM_ISX64)
            set(LIBSSL_URL https://www.ghproxy.cc/https://github.com/cngege/cngege.github.io/releases/download/v0.1-beta.1/libssl.lib)
            set(CRYPTO_URL https://www.ghproxy.cc/https://github.com/cngege/cngege.github.io/releases/download/v0.1-beta.1/libcrypto.lib)
        else()
            set(LIBSSL_URL https://www.ghproxy.cc/https://github.com/cngege/cngege.github.io/releases/download/v0.1-beta.1/libssl_x86.lib)
            set(CRYPTO_URL https://www.ghproxy.cc/https://github.com/cngege/cngege.github.io/releases/download/v0.1-beta.1/libcrypto_x86.lib)
        endif()
    endif()

    # 下载libssl.lib

    # 设置库远程下载超时时间
    # 下载的静态库会直接链接进最终可执行文件，必须校验 TLS 证书（见下方 TLS_VERIFY ON），
    # 避免本地镜像/代理把预编译库替换成被篡改的二进制。
    SET(DOWNLOAD_OPENSSL_TIMEOUT 300 CACHE STRING "Timeout in seconds when downloading openssl_lib.")
    if(NOT EXISTS "openssl-proj/lib")
        file(MAKE_DIRECTORY "openssl-proj/lib")
        MESSAGE(STATUS "Folder “openssl-proj/lib” created successfully.")
    endif()
    # 无论目录是否已存在（x64 与 x86 两次配置共享同一目录），都必须给出有效下载路径，
    # 否则第二次配置时 LOCAL_OPENSSL_LIBRARIES_PATH 为空，库会下载到源码根目录导致链接失败
    SET(LOCAL_OPENSSL_LIBRARIES_PATH "openssl-proj/lib")

    # /////////////////////////////////////预编译库完整性校验////////////////////////////////////////////////
    # 预编译库会直接链接进最终可执行文件。TLS 证书校验只能阻止中间人替换下载内容，
    # 无法防上游产物本身被替换或本地缓存被篡改，因此固定各文件的 SHA256 期望值：
    # 下载后、以及复用本地已有文件时都做一次校验，不一致直接中止配置。
    # 期望值来源：cngege/cngege.github.io 的 v0.1-beta.1 发布产物（已重新下载并逐字节比对确认）。
    set(OPENSSL_EXPECTED_SHA256_libssl.lib        "80c92c42796d4cde173916debae464c9e7e428fed07ee313b64ff1845c08736f")
    set(OPENSSL_EXPECTED_SHA256_libcrypto.lib     "1e32b094bf4d9c5d9eb62c46b3b258c23bf6078e25f48b3cc869b86fed33d87c")
    set(OPENSSL_EXPECTED_SHA256_libssl_x86.lib    "8f206901be6fa6e636d3643e200c0b3323a4441457c171fc5bd4fa7a27acc0a7")
    set(OPENSSL_EXPECTED_SHA256_libcrypto_x86.lib "27904a21887be0fc258a9e79a9552c209400031153ba9375b9bfd8547215878e")

    # 校验单个预编译库的 SHA256；不一致则中止配置并给出可操作的处理方式
    function(verify_openssl_lib lib_path lib_name expected_sha256)
        if(NOT EXISTS "${lib_path}")
            MESSAGE(FATAL_ERROR "OpenSSL 预编译库不存在: ${lib_path}")
        endif()
        file(SHA256 "${lib_path}" actual_sha256)
        if(NOT actual_sha256 STREQUAL expected_sha256)
            MESSAGE(FATAL_ERROR
                "OpenSSL 预编译库完整性校验失败: ${lib_name}\n"
                "  期望 SHA256: ${expected_sha256}\n"
                "  实际 SHA256: ${actual_sha256}\n"
                "  该文件会被直接链接进最终可执行文件，可能已被篡改。\n"
                "  请删除 ${lib_path} 后重新配置以重新下载；\n"
                "  若上游确实更新了产物，请同步更新 CMakeLists.txt 中对应的 OPENSSL_EXPECTED_SHA256_* 期望值。")
        endif()
        MESSAGE(STATUS "SHA256 OK: ${lib_name} (${actual_sha256})")
    endfunction()

    # /////////////////////////////////////ssl库lib名称////////////////////////////////////////////////
    if(DEFINED CURRENT_PLATFORM_ISX64)
        SET(OPENSSL_SSL_NAME "libssl.lib")
    else()
        SET(OPENSSL_SSL_NAME "libssl_x86.lib")
    endif()

    # ssl远程下载地址
    SET(SSL_DOWNLOAD_URL ${LIBSSL_URL})
    MESSAGE(STATUS "Checking ${OPENSSL_SSL_NAME}...")
    # 判断库文件是否已经存在
    FIND_FILE(LOCAL_OPENSSL_SSL_LIB
        NAMES ${OPENSSL_SSL_NAME}
        HINTS "${PROJECT_SOURCE_DIR}/openssl-proj/lib"
        #NO_CACHE
        NO_DEFAULT_PATH)
    MESSAGE(STATUS "Checking url link ${LIBSSL_URL} ...")
    # 如果路径下没有目标文件
    IF(NOT LOCAL_OPENSSL_SSL_LIB)
        MESSAGE(STATUS "Downloading ${OPENSSL_SSL_NAME} to ${LOCAL_OPENSSL_LIBRARIES_PATH} ...")
            # 从设定的 URL 地址下载相应的压缩包
        FILE(DOWNLOAD ${LIBSSL_URL}
            ${LOCAL_OPENSSL_LIBRARIES_PATH}/${OPENSSL_SSL_NAME}
            TIMEOUT ${DOWNLOAD_OPENSSL_TIMEOUT}
            STATUS ERR
            TLS_VERIFY ON
            SHOW_PROGRESS)
        # 判断下载是否存在错误
        IF(ERR EQUAL 0)
            # 如果下载无错误则设置压缩包名称标记下载成功
            SET(LOCAL_OPENSSL_SSL_LIB "${LOCAL_OPENSSL_LIBRARIES_PATH}/${OPENSSL_SSL_NAME}")
        #下载错误输出相关的提示信息
        ELSE()
            FILE(REMOVE ${LOCAL_OPENSSL_LIBRARIES_PATH}/${OPENSSL_SSL_NAME})
            MESSAGE(STATUS "Download failed, error: ${ERR}")
            MESSAGE(FATAL_ERROR
            "You can try downloading ${LIBSSL_URL} manually"
            "using curl/wget or a similar tool")
            return()
        ENDIF()
    ENDIF()
    # 如果路径下存在文件
    IF(LOCAL_OPENSSL_SSL_LIB)
        MESSAGE(STATUS "Checking the direction of ${LOCAL_OPENSSL_LIBRARIES_PATH}/${OPENSSL_SSL_NAME}")
        verify_openssl_lib("${LOCAL_OPENSSL_SSL_LIB}" "${OPENSSL_SSL_NAME}" "${OPENSSL_EXPECTED_SHA256_${OPENSSL_SSL_NAME}}")
    ENDIF()


    # /////////////////////////////////////crypto库lib名称////////////////////////////////////////////////
    if(DEFINED CURRENT_PLATFORM_ISX64)
        SET(OPENSSL_CRYPTO_NAME "libcrypto.lib")
    else()
        SET(OPENSSL_CRYPTO_NAME "libcrypto_x86.lib")
    endif()
    # ssl远程下载地址
    SET(CRYPTO_DOWNLOAD_URL ${CRYPTO_URL})
    MESSAGE(STATUS "Checking ${OPENSSL_CRYPTO_NAME}...")
    # 判断库文件是否已经存在
    FIND_FILE(LOCAL_OPENSSL_CRYPTO_LIB
        NAMES ${OPENSSL_CRYPTO_NAME}
        HINTS "${PROJECT_SOURCE_DIR}/openssl-proj/lib"
        #NO_CACHE
        NO_DEFAULT_PATH)
    MESSAGE(STATUS "Checking url link ${CRYPTO_URL} ...")
    # 如果路径下没有目标文件
    IF(NOT LOCAL_OPENSSL_CRYPTO_LIB)
        MESSAGE(STATUS "Downloading ${OPENSSL_CRYPTO_NAME} to ${LOCAL_OPENSSL_LIBRARIES_PATH} ...")
            # 从设定的 URL 地址下载相应的压缩包
        FILE(DOWNLOAD ${CRYPTO_URL}
            ${LOCAL_OPENSSL_LIBRARIES_PATH}/${OPENSSL_CRYPTO_NAME}
            TIMEOUT ${DOWNLOAD_OPENSSL_TIMEOUT}
            STATUS ERR
            TLS_VERIFY ON
            SHOW_PROGRESS)
        # 判断下载是否存在错误
        IF(ERR EQUAL 0)
            # 如果下载无错误则设置压缩包名称标记下载成功
            SET(LOCAL_OPENSSL_CRYPTO_LIB "${LOCAL_OPENSSL_LIBRARIES_PATH}/${OPENSSL_CRYPTO_NAME}")
        #下载错误输出相关的提示信息
        ELSE()
            FILE(REMOVE ${LOCAL_OPENSSL_LIBRARIES_PATH}/${OPENSSL_CRYPTO_NAME})
            MESSAGE(STATUS "Download failed, error: ${ERR}")
            MESSAGE(FATAL_ERROR
            "You can try downloading ${CRYPTO_URL} manually"
            "using curl/wget or a similar tool")
            return()
        ENDIF()
    ENDIF()
    # 如果路径下存在压缩文件
    IF(LOCAL_OPENSSL_CRYPTO_LIB)
        MESSAGE(STATUS "Checking the direction of ${LOCAL_OPENSSL_LIBRARIES_PATH}/${OPENSSL_CRYPTO_NAME}")
        verify_openssl_lib("${LOCAL_OPENSSL_CRYPTO_LIB}" "${OPENSSL_CRYPTO_NAME}" "${OPENSSL_EXPECTED_SHA256_${OPENSSL_CRYPTO_NAME}}")
    ENDIF()

    # 包含 openssl 的头文件
    include_directories(openssl-proj/include)

endif()


########################################################################
# macOS：通过 Homebrew 的 openssl@3 获取头文件与库
########################################################################
if(APPLE)
    # Homebrew 安装的 openssl@3 提供官方 CMake 配置文件
    find_package(OpenSSL REQUIRED)
    message(STATUS "macOS 使用 OpenSSL: ${OPENSSL_VERSION}")
endif()

########################################################################
# Linux：通过系统 libssl-dev 获取头文件与库（无 Homebrew/框架依赖）
########################################################################
if(LINUX)
    find_package(OpenSSL REQUIRED)
    message(STATUS "Linux 使用 OpenSSL: ${OPENSSL_VERSION}")
endif()


# 将源代码添加到此项目的可执行文件。
add_executable (GitHubDesktop2Chinese "GitHubDesktop2Chinese.cpp")


if(WIN32)
    # 链接OpenSSL库
    target_link_libraries(GitHubDesktop2Chinese PRIVATE
        ${CMAKE_SOURCE_DIR}/openssl-proj/lib/${OPENSSL_SSL_NAME}
        ${CMAKE_SOURCE_DIR}/openssl-proj/lib/${OPENSSL_CRYPTO_NAME})
endif()

if(APPLE)
    # 链接 OpenSSL（Homebrew）
    target_link_libraries(GitHubDesktop2Chinese PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    # macOS 平台能力：CGEventSourceFlagsState（Shift 检测）与 Foundation（PlistBuddy 辅助）
    find_library(APPKIT_LIBRARY AppKit REQUIRED)
    find_library(COREGRAPHICS_LIBRARY CoreGraphics REQUIRED)
    find_library(FOUNDATION_LIBRARY Foundation REQUIRED)
    target_link_libraries(GitHubDesktop2Chinese PRIVATE
        ${APPKIT_LIBRARY}
        ${COREGRAPHICS_LIBRARY}
        ${FOUNDATION_LIBRARY})
endif()

if(LINUX)
    # 链接 OpenSSL（系统 libssl-dev）
    # Linux 无框架依赖：termios/signal/popen 均由 libc 提供，无需额外 find_library
    target_link_libraries(GitHubDesktop2Chinese PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()


    # 动态引入链接 CLI参数管理器库
FetchContent_Declare(
    cli11_proj
    QUIET
    GIT_REPOSITORY ${CLI11_URL}
    GIT_TAG v2.4.1
)
FetchContent_MakeAvailable(cli11_proj)
target_link_libraries(GitHubDesktop2Chinese PRIVATE CLI11::CLI11)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  # 用 target_compile_features 而不是直接改属性：前者会同时参与编译特性推导，
  # 避免同一目标被不同语言标准重复编译
  target_compile_features(GitHubDesktop2Chinese PRIVATE cxx_std_20)
endif()

########################################################################
# 测试：平台无关逻辑的回归测试（Windows/macOS/Linux 均可运行）
########################################################################
enable_testing()

add_executable(test_platform_logic "tests/test_platform_logic.cpp")
# 测试不依赖 CLI11（被测逻辑是纯字符串/版本解析），无需链接第三方头文件库
# 测试用到 std::optional / 结构化绑定等 C++17 特性，必须显式设定标准
# （主程序的 CXX_STANDARD 不会传递到该目标，VS 默认按 C++14 编译会报 C2039）
target_compile_features(test_platform_logic PRIVATE cxx_std_20)
target_compile_definitions(test_platform_logic PRIVATE FILEVERSION="${FILE_VERSION}")
# 被测逻辑是纯字符串/版本解析，不引用任何 OpenSSL 符号。
# macOS 上若链接 Homebrew 的 libssl/libcrypto，运行测试时还需要 dyld 找到这些
# dylib，会让与 OpenSSL 无关的单元测试无谓地失败；Linux 同样不需要。

# CTest 注册：失败时输出详情
add_test(
    NAME platform_logic_tests
    COMMAND test_platform_logic
)
set_tests_properties(platform_logic_tests PROPERTIES
    FAIL_REGULAR_EXPRESSION "FAIL"
    PASS_REGULAR_EXPRESSION "ALL PASS"
)

# TODO: 如有需要，请添加测试并安装目标。
