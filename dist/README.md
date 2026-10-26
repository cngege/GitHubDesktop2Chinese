# dist/ —— 发布产物

本目录存放发布产物：`dist/` 根目录为**本机 MSVC 构建的 Windows 产物**，`dist/ci/` 为 **CI（`build.yml` 打 tag 触发）构建的各平台产物**。

## 当前产物（v1.3.0）

### 本机构建（dist/ 根目录）

| 文件 | 平台 | 构建方式 | 依赖 |
| --- | --- | --- | --- |
| `GitHubDesktop2Chinese_x64.exe` | Windows x64 | MSVC `cl.exe`（本机） | VC++ 2022 运行库 |
| `GitHubDesktop2Chinese_x86.exe` | Windows x86 | MSVC `cl.exe` 交叉编译（Hostx64/x86） | VC++ 2022 运行库 |

### CI 构建（dist/ci/）

| 文件 | 平台 | 构建方式 | 依赖 |
| --- | --- | --- | --- |
| `GitHubDesktop2Chinese.exe` | Windows x64 | `build.yml` 的 `build` 任务（CMake `-A x64`） | VC++ 2022 运行库 |
| `GitHubDesktop2Chinese_x86.exe` | Windows x86 | `build.yml` 的 `build` 任务（CMake `-A Win32`） | VC++ 2022 运行库 |
| `GitHubDesktop2Chinese_linux_x64` | Linux x86_64 | `build.yml` 的 `build-linux`（系统 libssl-dev） | glibc / libssl3 |
| `GitHubDesktop2Chinese_macos_x64` | macOS Intel | `build.yml` 的 `build-macos`（macos-13） | macOS 13+ |
| `GitHubDesktop2Chinese_macos_arm64` | macOS Apple Silicon | `build.yml` 的 `build-macos`（macos-latest） | macOS 14+ |

> macOS 两个产物由 CI 产出后拷入本目录；本机无法构建 macOS/Linux（无 macOS/Linux 工具链）。

CI 构建与本机构建使用同一份源码，参数等价（`CMakeLists.txt` 的 `-O2 Release` + `FILEVERSION` 注入）。两者可互换使用；CI 版本经过完整 `platform_logic_tests` 回归。


## 构建方式（无 CMake 时的备选）

本机没有 cmake/ninja，以下命令复刻 `CMakeLists.txt` 的编译与链接参数，直接调用 VS 自带的 `cl.exe`。  
需在「Visual Studio 开发者命令行」中执行（或手动导出 `INCLUDE`/`LIB`/`PATH`）。

```bash
# 公共参数
CLFLAGS="-nologo -std:c++20 -EHsc -O2 -utf-8 -I. -ICLI -IUtils -Iinclude"
LIBS="user32.lib shell32.lib advapi32.lib winhttp.lib"

# 前置：CLI11 单头文件（放到 CLI/CLI.hpp）
curl -L -o CLI/CLI.hpp https://github.com/CLIUtils/CLI11/releases/download/v2.4.1/CLI11.hpp

# Windows 预编译 OpenSSL 库（来自 cngege.github.io 的 v0.1-beta.1 release）
#   x64: libssl.lib / libcrypto.lib
#   x86: libssl_x86.lib / libcrypto_x86.lib

# x64
cl.exe $CLFLAGS '-DFILEVERSION="1.3.0"' \
  GitHubDesktop2Chinese.cpp libssl.lib libcrypto.lib $LIBS \
  -Fe:GitHubDesktop2Chinese_x64.exe

# x86（用 Hostx64/x86 交叉编译，x86 库需换成 _x86 后缀版本）
cl.exe $CLFLAGS '-DFILEVERSION="1.3.0"' \
  GitHubDesktop2Chinese.cpp libssl_x86.lib libcrypto_x86.lib $LIBS \
  -Fe:GitHubDesktop2Chinese_x86.exe
```

### 手动配置 MSVC 环境（非开发者命令行）

```bash
VSCROOT="C:/Program Files/Microsoft Visual Studio/18/Community"
MSVCVER="14.44.35207"
SDK="10.0.26100.0"

# x64
export PATH="$VSCROOT/VC/Tools/MSVC/$MSVCVER/bin/Hostx64/x64:$PATH"
export INCLUDE="$VSCROOT/VC/Tools/MSVC/$MSVCVER/include;C:/Program Files (x86)/Windows Kits/10/Include/$SDK/ucrt;C:/Program Files (x86)/Windows Kits/10/Include/$SDK/um;C:/Program Files (x86)/Windows Kits/10/Include/$SDK/shared"
export LIB="$VSCROOT/VC/Tools/MSVC/$MSVCVER/lib/x64;C:/Program Files (x86)/Windows Kits/10/Lib/$SDK/ucrt/x64;C:/Program Files (x86)/Windows Kits/10/Lib/$SDK/um/x64"

# x86 改用 Hostx64/x86 的 cl.exe 与 lib/x86、ucrt/x86、um/x86 目录
```

## 验证产物

```bash
# 架构标识（x86=0x014C / x64=0x8664）
python -c "import struct;d=open('GitHubDesktop2Chinese_x64.exe','rb').read(1024);o=struct.unpack_from('<I',d,0x3C)[0];print(hex(struct.unpack_from('<H',d,o+4)[0]))"

# 运行检查
./GitHubDesktop2Chinese_x64.exe --help
```

## 注意

- `openssl-proj/` 里的 Windows 预编译库由 CMake 在配置阶段自动下载（`CMakeLists.txt:98-221`）；上述手动流程绕过 CMake，需自行准备好对应架构的 `libssl*.lib` / `libcrypto*.lib`
- 若用户运行时提示缺失 `MSVCP140_ATOMIC_WAIT.dll`，需安装 [VC++ 可再发行程序包](https://learn.microsoft.com/zh-cn/cpp/windows/latest-supported-vc-redist?view=msvc-170)
- 产物体积差异属正常（x64 约 6.1MB，x86 约 4.2MB，同一份源码不同目标平台）
