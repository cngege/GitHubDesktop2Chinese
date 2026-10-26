# GitHubDesktop2Chinese

![GitHub Actions](https://img.shields.io/github/actions/workflow/status/cngege/GitHubDesktop2Chinese/build.yml)
![GitHub Actions](https://img.shields.io/github/actions/workflow/status/cngege/GitHubDesktop2Chinese/check.yml?label=json检查)
![GitHub Release](https://img.shields.io/github/v/release/cngege/GitHubDesktop2Chinese)
![GitHub Release](https://img.shields.io/badge/Cpp-json-blue?logo=cplusplus)
![GitHub License](https://img.shields.io/github/license/cngege/GitHubDesktop2Chinese)



## 🥮这是什么

> 这是一个自动替换GithubDesktop中文本为目标语言文本的程序，  
  优点是对GithubDesktop频繁更新的版本变化兼容性比较高。  
  即便只有那么一两条失去了翻译，也只需要手动再修改添加一下即可，  
  项目维护成本很低  

## 🎯怎么使用它

[🎀GOTO BiliBili Video🎀](https://www.bilibili.com/video/BV17HpSeHEaC/)

> 第一种方法: 下载或自行构建 GitHubDesktop2Chinese.exe 双击运行, 自动联网获取最新localization.json完成汉化  
  第二种方法: 下载或自行构建 GitHubDesktop2Chinese.exe  和 localization.json  放在同一个文件夹。运行程序即可  
  GitHubDesktop每次版本更新都需要运行一次此程序才能完成汉化

### 🍏 macOS 使用说明

> macOS 版与 Windows 版功能一致（自动发现 GitHub Desktop、汉化、备份、回滚、开发者选项、失效检测、自动更新）。

- **下载**：从 [Releases](https://github.com/cngege/GitHubDesktop2Chinese/releases) 下载对应架构的二进制：
  - `GitHubDesktop2Chinese_macos_arm64` —— Apple Silicon（M1/M2/M3…）
  - `GitHubDesktop2Chinese_macos_x64` —— Intel 芯片
- **运行**：打开“终端”，进入文件所在目录后执行：
  ```bash
  chmod +x GitHubDesktop2Chinese_macos_arm64
  ./GitHubDesktop2Chinese_macos_arm64
  ```
- **自动发现**：程序会自动扫描 `/Applications` 与 `~/Applications` 下的 `GitHub Desktop.app`，读取其版本号并定位 `Contents/Resources/app/` 下的 `main.js`/`renderer.js`；若未找到，会提示手动输入目录（与 Windows 的 `-g` 参数行为一致）。
- **首次运行被拦截？** 若 macOS 提示“无法验证开发者”，前往 `系统设置 → 隐私与安全性` 点击“仍要打开”；或临时执行 `xattr -d com.apple.quarantine <文件>`。
- **移除汉化**：执行 `./GitHubDesktop2Chinese_macos_arm64 --rollback` 即可从备份还原汉化前的文件。
- **代理**：自动读取 `HTTPS_PROXY`/`https_proxy` 环境变量与系统代理（解析 `scutil --proxy`）。

### 🐧 Linux 使用说明

> **重要前提**：GitHub Desktop **没有官方 Linux 版本**。Linux 上使用的是社区分支 [shiftkey/desktop](https://github.com/shiftkey/desktop)（提供 deb/rpm/AppImage/Flatpak/AUR 安装包）。本程序支持该分支汉化。

- **下载**：从 [Releases](https://github.com/cngege/GitHubDesktop2Chinese/releases) 下载 `GitHubDesktop2Chinese_linux_x64`。
- **运行**：打开终端，进入文件所在目录后执行：
  ```bash
  chmod +x GitHubDesktop2Chinese_linux_x64
  ./GitHubDesktop2Chinese_linux_x64
  ```
- **自动发现**：程序会按优先级扫描以下安装根下的 `resources/app`（含 `index.html` 即视为有效）：
  - `/usr/lib/github-desktop`（deb/rpm 安装）、`/usr/lib64/github-desktop`、`/usr/local/lib/github-desktop`、`/usr/share/github-desktop`、`/opt/github-desktop`
  - Flatpak 安装目录：`/var/lib/flatpak/app/io.github.shiftey.Desktop/current/active/files/github-desktop`（系统级）与 `~/.local/share/flatpak/app/io.github.shiftey.Desktop/current/active/files/github-desktop`（用户级）
  - 用户级安装 `~/.local/share/github-desktop`
- **AppImage/Flatpak 用户注意**：AppImage 是单文件（运行时才挂载出资源目录），无法静态扫描；部分 Flatpak 布局也无法命中。此时请用 `-g` 手动指定资源目录：
  ```bash
  # AppImage 示例：先挂载找到 resources/app 路径，再执行
  ./GitHubDesktop2Chinese_linux_x64 -g /usr/lib/github-desktop/resources/app
  ```
- **进入开发者选项**：Linux 上无法检测 Shift 按键（无可靠的无窗口修饰键 API），请用命令行入口：
  ```bash
  ./GitHubDesktop2Chinese_linux_x64 dev -d
  ```
- **移除汉化**：执行 `./GitHubDesktop2Chinese_linux_x64 --rollback` 从备份还原。
- **代理**：自动读取 `HTTPS_PROXY`/`https_proxy` 环境变量与 GNOME 系统代理（解析 `gsettings`）。

## 🎏怎么编译源代码

> 下载克隆项目 本地使用VS2022 使用CMAKE打开（直接用vs2022打开项目文件夹），即可构建

### macOS 构建说明

> 需要通过 [Homebrew](https://brew.sh/) 安装 `openssl@3`：`brew install openssl@3`

```bash
# 使用预设（推荐）
cmake --preset macos-release       # 生成于 out/build/macos-release
cmake --build out/build/macos-release

# 或手动指定（Apple Silicon 示例）
cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_SYSTEM_PROCESSOR=arm64 \
      -DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3)
cmake --build build
```

- macOS 上不使用 `openssl-proj/` 的 Windows 预编译库，改用 Homebrew 的 `openssl@3`。
- 若同时安装了多个 OpenSSL，请用 `-DOPENSSL_ROOT_DIR` 显式指向 Homebrew 的安装目录。
- 构建产物为无后缀的可执行文件 `GitHubDesktop2Chinese`，可自行重命名（如加 `_macos_arm64` 后缀）。

### Linux 构建说明

> 需要通过系统包管理器安装 OpenSSL 开发库（提供 `find_package(OpenSSL)` 所需的头文件与库）：
> - Debian/Ubuntu：`sudo apt-get install libssl-dev`
> - Fedora/RHEL：`sudo dnf install openssl-devel`
> - Arch：`sudo pacman -S openssl`（通常已预装）

```bash
# 使用预设（推荐）
cmake --preset linux-release         # 生成于 out/build/linux-release
cmake --build out/build/linux-release

# 或手动指定
cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_SYSTEM_PROCESSOR=x86_64
cmake --build build
```

- Linux 上直接使用系统 OpenSSL（不使用 `openssl-proj/` 的 Windows 预编译库，也无需 Homebrew）。
- termios/signal/popen 等能力由 libc 提供，无额外框架依赖。
- 构建产物为无后缀的可执行文件 `GitHubDesktop2Chinese`，可自行重命名（如加 `_linux_x64` 后缀）。
- 架构由 `CMAKE_SYSTEM_PROCESSOR` 自动识别（x86_64 / aarch64）。

## 👕我怎么帮助完成汉化或者后续更新时如何补充汉化

> 只需要克隆仓库，阅读`json/关于一些注意事项.txt`，注意一些编写时的事项, 然后在`json/localization.json` 文件中参照已经写过的格式补充要汉化的条目即可  
  将此文件放在程序目录下,确保程序可以读取运行 然后提交PR
> 如果你写了部分翻译条目，可以先将其放在`main_dev`或者`renderer_dev`条目中, 然后按下`shift`运行软件,开启 `仅替换指定映射项，以优化汉化作者替换时间` 进行汉化测试
  完成之后, 将对应条目移动到`main` 或 `renderer` 数组最后, 提交PR

## 🍬映射文件：localization.json
> 此文件存储所有GitHubDesktop中英文文本到本地化(中文)文本之间的映射，  
  使用正则匹配的方式 将包含英文字符串的文本替换为包含中文字符串的文本。  
  项目更新主要更新此文件。

- 路径: [项目目录]/json/localization.json
- 主节点 - version(int): 此json文件的版本,此值仅会因未来格式更新而更新
- 主节点 - minversion(string): 需要最低的加载器版本
- 主节点 - tip(array[string]): 用于在加载器中显示的通知信息
- 主节点 - select(JSON): 本地化时提示进行选择性修改
  - array[]
    - replaceFile(string): 进行替换的文件
    - tooltip(string): 提示信息
    - enable(bool)：此条是否启用
    - replace(array[]): 二维数组, 用户启用时的替换项
- 主节点 - main(array): 存储用于替换GitHubDesktop的main.js的映射
- 主节点 - main_dev(array): 存储用于替换GitHubDesktop的main.js的映射,区别于开发时快速替换
- 主节点 - renderer(array): 存储用于替换GitHubDesktop的renderer.js的映射
- 主节点 - renderer_dev(array): 存储用于替换GitHubDesktop的renderer.js的映射,区别于开发时快速替换

## ⚠️minifier 敏感条目维护说明
> GitHub Desktop 的 `main.js`/`renderer.js` 是**压缩后**的产物，变量名由压缩器分配（如 `Ae`、`xe`、`EH`、`ZB`），  
  且不同版本/不同打包轮次可能采用**不同的压缩形态**。这是大部分"失效项"的根源，编写正则时务必注意。

### 两种 createElement 形态
同一份 `renderer.js` 里常常同时存在两种写法：
- `Ae.createElement(...)` —— 无调用括号的别名（多数）
- `xe().createElement(...)` —— 带调用括号（少数）

**不要**写死成 `(.)\(\)\.createElement`（只能匹配带括号形态），也**不要**写死成 `Ae\.createElement`（升级后别名一变就失效）。  
应使用同时兼容两者的写法：

```
([A-Za-z$_][\w$]*(?:\(\))?)?\.createElement
```

### 捕获组与替换串回填必须配套
- 正则里的占位符请使用**捕获组** `(...)` 而非非捕获组 `(?:...)`，否则其后的 `$N` 编号会错位，回填出类似 `strong().createElement(...)` 的**非法代码**（程序运行时报错）。
- 替换串中若引用别名，写 `$1.createElement` 即可（捕获组已含可选括号）；匹配到 `Ae` 回填 `Ae.createElement`，匹配到 `xe()` 回填 `xe().createElement`，两种都合法。
- 优先用**固定文案做锚点**（如 `createElement("strong"`、`"Pull requests targeting "`），少用 `(.)/(..)` 这类单字符占位符，可显著降低升级失效概率。

### 改完如何自查（不用等 CI）
CI（`check.yml`）会用 `--invalidcheck` 对 `json/localization.json` 全量校验，有任何失效项即挂；  
此外 CI 还会跑 `tests/check_localization.js` 做同语义复核，在日志里直接列出每一条失效项。  

本地自查方式：下载对应版本的 `-x64-full.nupkg`，解压取 `lib/net45/resources/app/` 下的 `main.js`/`renderer.js`，然后运行：

```bash
node tests/check_localization.js json/localization.json <解压出的 app 目录>
```

输出 `失效项总数: 0` 即通过；否则会逐条列出失效正则，按上面的规则修正即可。


## 使用环境变量开启GitHubDesktop预览版选项
> 内部脚本预览版判断机制  
```javascript
	const nn = !1;
	function rn() {
		return !nn && "1" === process.env.GITHUB_DESKTOP_PREVIEW_FEATURES
	}
	// rn() 返回true时，开启预览版机制
```

 - **设置环境变量开启预览版选项**  

```cmd
 > set GITHUB_DESKTOP_PREVIEW_FEATURES=1
 > "GitHub Desktop.lnk"
```
 - **或者通过加载器按提示选择自动开启预览版功能**


## 🚀发布流程（Release）

> 本项目通过 **打 tag 触发 `build.yml`** 自动构建全部平台并发布 GitHub Release；Windows 产物也可在本机用 MSVC 直接构建交付。  
> 发布前请先确认 `check.yml`（含 `--invalidcheck` 与 `tests/check_localization.js`）全部通过，避免发布失效映射。

### 产物清单

| 产物文件 | 平台 | 构建方式 |
| --- | --- | --- |
| `GitHubDesktop2Chinese.exe` | Windows x64 | `build.yml` 的 `build` 任务，或本机 MSVC |
| `GitHubDesktop2Chinese_x86.exe` | Windows x86 | 同上（`-A Win32`） |
| `GitHubDesktop2Chinese_macos_arm64` | macOS Apple Silicon | `build.yml` 的 `build-macos`（matrix: macos-latest） |
| `GitHubDesktop2Chinese_macos_x64` | macOS Intel | `build.yml` 的 `build-macos`（matrix: macos-13） |
| `GitHubDesktop2Chinese_linux_x64` | Linux x86_64 | `build.yml` 的 `build-linux` |
| `json/localization.json` | 映射文件 | 随 Release 一并发布 |

### 方式一：打 tag 触发 CI（推荐，出全平台）

```bash
# 1. 确认工作区干净且 check 已通过
git status --short

# 2. 打上版本 tag（build.yml 只在 push tag 时触发发布）
git tag v1.3.0
git push origin v1.3.0
```

CI 流程（`build.yml`）会依次完成：
1. **Windows**：CMake 构建 x64 与 x86（`-A x64` / `-A Win32`），产物重命名 `GitHubDesktop2Chinese_x86.exe`
2. **macOS**：`macos-13`（Intel）与 `macos-latest`（Apple Silicon）各构建一份，跑 `platform_logic_tests`，按架构加 `_macos_arm64` / `_macos_x64` 后缀
3. **Linux**：ubuntu-latest + 系统 `libssl-dev` 构建，跑 `platform_logic_tests`，加 `_linux_x64` 后缀
4. **发布**：`publish_release` 任务汇总全部产物 + `json/localization.json`，用 `ReleaseBody.md` 作为 Release 说明创建 GitHub Release（tag 含 `beta` 时自动标记为预发布）

> [!note]
> tag 名含 `beta` 时 Release 会自动标记为预发布（`isPrerelease`）。

### 方式二：本机构建 Windows 产物（MSVC，无 CMake）

本机没有 cmake/ninja 时，可直接用 VS 自带的 `cl.exe` 复刻 `CMakeLists.txt` 的参数构建：

```bash
# 前置：CLI11 单头文件与 openssl 预编译库（放到临时目录）
curl -L -o CLI/CLI.hpp https://github.com/CLIUtils/CLI11/releases/download/v2.4.1/CLI11.hpp
# Windows 预编译库来自 cngege.github.io 的 v0.1-beta.1 release：
#   x64:  libssl.lib / libcrypto.lib
#   x86:  libssl_x86.lib / libcrypto_x86.lib

# x64
cl.exe -nologo -std:c++20 -EHsc -O2 -utf-8 "-DFILEVERSION=\"1.3.0\"" \
  -I. -ICLI -IUtils -Iinclude \
  GitHubDesktop2Chinese.cpp libssl.lib libcrypto.lib -Fe:GitHubDesktop2Chinese_x64.exe

# x86（用 Hostx64/x86 交叉编译）
cl.exe -nologo -std:c++20 -EHsc -O2 -utf-8 "-DFILEVERSION=\"1.3.0\"" \
  -I. -ICLI -IUtils -Iinclude \
  GitHubDesktop2Chinese.cpp libssl_x86.lib libcrypto_x86.lib -Fe:GitHubDesktop2Chinese_x86.exe
```

需要在 VS 开发者命令行（或手动导出 `INCLUDE`/`LIB`/`PATH`）中执行，详见 `dist/build/README.md`。

### 版本号约定

- 源码默认 `FILE_VERSION` 为 `0.0.0-Dev.0`，发布时通过 CMake 参数 `-DFILE_VERSION=<tag>` 注入（CI 已自动从 tag 名读取）
- 程序运行时会打印 `程序架构 / 版本`（`GitHubDesktop2Chinese.cpp:210`），版本号错会导致 json 的 `minversion` 校验告警
- Release 说明维护在 `ReleaseBody.md`，发布前更新对应版本的更新日志

### 发布后的校验

- 从 Release 下载对应平台产物，运行 `--version` 类命令或直接双击运行确认架构与版本号正确
- 用 `tests/check_localization.js` 对 Release 附带的 `localization.json` 做一次失效项复核
- Windows 若提示缺失 `MSVCP140_ATOMIC_WAIT.dll`，安装 [VC++ 运行库](https://learn.microsoft.com/zh-cn/cpp/windows/latest-supported-vc-redist?view=msvc-170)（见上方“其他”一节）

## 🧭其他

> 如果报错提示找不到openssl 的dll文件，请更新到[最新版](https://github.com/cngege/GitHubDesktop2Chinese/releases)   
  如果你有任何建议可以提issues.

> [!tip]
> 如果打开 GitHubDesktop2Chinese.exe 时程序不运行或一闪而过， 或发现缺失 `MSVCP140_ATOMIC_WAIT.dll`，可以尝试下载微软运行库 [Microsoft Visual C++ Redistributable 14.42.34433.0](https://learn.microsoft.com/zh-cn/cpp/windows/latest-supported-vc-redist?view=msvc-170)。  
> 找到 `最新的 Microsoft Visual C++ 可再发行程序包版本`，选择与你相应的操作系统的版本进行下载（比如64位的电脑选择 `vc_redist.x64.exe	`，32位的电脑选择 `vc_redist.x86.exe`），安装即可。  
> 如果安装了最新 VC++ 运行库依旧无法运行，请检查程序所在目录下是否残留 MSVCP140.dll、VCRUNTIME140.dll 等文件，如有请删除。

## 🍬第三方库  
**感谢以下诸位提供的优质的开源项目**  

- CLI: https://github.com/CLIUtils/CLI11  
- HttpLib: https://github.com/yhirose/cpp-httplib  
- nlohmann/json: https://github.com/nlohmann/json  
- spdlog: https://github.com/gabime/spdlog  
- WinReg: https://github.com/GiovanniDicanio/WinReg  

## 🎋TODO  
 - [x] json文件格式修改,用于标识文件版本、最低支持加载器（GitHubDesktop2Chinese.exe）的版本
 - [x] 加载器加入程序版本宏定义
 - [x] 加载器支持替换映射的第三个参数,即查找参数,将第三个参数（如果有）进行全局正则查找,  
       将匹配到的结果在第二个参数中进行特殊字符的替换，替换标记为`#{number}`
 - [x] 加载器检查json文件所需最低版本是否满足需求,如果不满足且定义了`--nopause`则提示后退出,
       否则提示后询问是否强制替换
 - [x] 加载器在没有定义`--nopause`的情况下,在替换之前进行一次暂停,以便用户自行确认信息后是否往下执行
 - [x] 加载器自动检测更新，用户可以选择一键自动更新
 - [x] 提示确认后自动更新
 - [x] JSON文件附加描述文本，在加载器中显示
 - [x] 加载器显示所有项目参与者并在汉化后显示
 - [x] 汉化异常后恢复汉化前文件
 - [x] 改动json格式允许进行提示后选择性汉化
 - [x] 在映射文件中添加转为预览版
 - [x] 支持使用系统http代理,包括环境变量和读系统代理注册表
 - [x] 下载更新的断点续传功能
 - [x] 读取GitHub Desktop的最新版和本地版本比较，如果有新版，则用其他颜色给出提示

## 🍬星标(收藏)历史
![Star History Chart](https://star-history.dera.page/svg?repos=cngege/GitHubDesktop2Chinese&type=Date)

## 🏘️感谢大家的群策群力  
![Contributors](https://contrib.rocks/image?repo=cngege/GitHubDesktop2Chinese)

<details>
    <summary>点击展开示例图片</summary>
<img src="https://github.com/lkyero/GitHubDesktop_zh/assets/28597788/3023d028-8f63-4919-8900-ab3e953a1f76" alt="展示图" />
</details>
