# CMakeList.txt: GitHubDesktop2Chinese 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required (VERSION 3.8)

# 如果支持，请为 MSVC 编译器启用热重载。
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()


#不要放在 if(MSVC) 中
add_compile_options(/utf-8)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(/bigobj)
    message("当前是以Debug模式编译")
endif()



if(DEFINED FILE_VERSION)
add_definitions(-DFILEVERSION="${FILE_VERSION}")
#add_definitions(-DFILEVERSION="v1.0.2")
else()
add_definitions(-DFILEVERSION="0.0.0-Dev.0")
endif()

project ("GitHubDesktop2Chinese")

include(FetchContent)

include_directories(include)
include_directories(Utils)

if(DEFINED GitHubActions)
	set(CLI11_URL https://github.com/CLIUtils/CLI11.git)
	set(LIBSSL_URL https://github.com/cngege/cngege.github.io/releases/download/v0.1-beta.1/libssl.lib)
	set(CRYPTO_URL https://github.com/cngege/cngege.github.io/releases/download/v0.1-beta.1/libcrypto.lib)
else()
	set(CLI11_URL https://gitclone.com/github.com/CLIUtils/CLI11.git)
	set(LIBSSL_URL https://gh.ddlc.top/https://github.com/cngege/cngege.github.io/releases/download/v0.1-beta.1/libssl.lib)
	set(CRYPTO_URL https://gh.ddlc.top/https://github.com/cngege/cngege.github.io/releases/download/v0.1-beta.1/libcrypto.lib)
endif()



# 下载libssl.lib

# 设置库远程下载超时时间
SET(DOWNLOAD_OPENSSL_TIMEOUT 300 CACHE STRING "Timeout in seconds when downloading openssl_lib.")
FIND_FILE(LOCAL_OPENSSL_LIBRARIES_PATH
	NAMES ""
	HINTS "openssl-proj/lib"
	NO_DEFAULT_PATH)
if(NOT EXISTS "openssl-proj/lib")
	file(MAKE_DIRECTORY "openssl-proj/lib")
	SET(LOCAL_OPENSSL_LIBRARIES_PATH "openssl-proj/lib")
	MESSAGE(STATUS "Folder “openssl-proj/lib” created successfully.")
endif()

# /////////////////////////////////////ssl库lib名称////////////////////////////////////////////////
SET(OPENSSL_SSL_NAME "libssl.lib")
# ssl远程下载地址
SET(SSL_DOWNLOAD_URL ${LIBSSL_URL})
MESSAGE(STATUS "Checking ${OPENSSL_SSL_NAME}...")
# 判断库文件是否已经存在
FIND_FILE(LOCAL_OPENSSL_SSL_LIB
	NAMES ${OPENSSL_SSL_NAME}
	HINTS "${PROJECT_SOURCE_DIR}/openssl-proj/lib"
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
	ENDIF()
ENDIF()
# 如果路径下存在压缩文件
IF(LOCAL_OPENSSL_SSL_LIB)
	MESSAGE(STATUS "Checking the direction of ${LOCAL_OPENSSL_LIBRARIES_PATH}/${OPENSSL_SSL_NAME}")
ENDIF()


# /////////////////////////////////////crypto库lib名称////////////////////////////////////////////////
SET(OPENSSL_CRYPTO_NAME "libcrypto.lib")
# ssl远程下载地址
SET(CRYPTO_DOWNLOAD_URL ${CRYPTO_URL})
MESSAGE(STATUS "Checking ${OPENSSL_CRYPTO_NAME}...")
# 判断库文件是否已经存在
FIND_FILE(LOCAL_OPENSSL_CRYPTO_LIB
	NAMES ${OPENSSL_CRYPTO_NAME}
	HINTS "${PROJECT_SOURCE_DIR}/openssl-proj/lib"
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
	ENDIF()
ENDIF()
# 如果路径下存在压缩文件
IF(LOCAL_OPENSSL_CRYPTO_LIB)
	MESSAGE(STATUS "Checking the direction of ${LOCAL_OPENSSL_LIBRARIES_PATH}/${OPENSSL_CRYPTO_NAME}")
ENDIF()

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
    GIT_REPOSITORY ${CLI11_URL}
    GIT_TAG v2.4.1
)
FetchContent_MakeAvailable(cli11_proj)
target_link_libraries(GitHubDesktop2Chinese PRIVATE CLI11::CLI11)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET GitHubDesktop2Chinese PROPERTY CXX_STANDARD 20)
endif()

# TODO: 如有需要，请添加测试并安装目标。
