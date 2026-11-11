
### 1.3.0 更新内容

#### 新增功能
- 新增 macOS（Apple Silicon arm64 / Intel x64）与 Linux x86_64 支持，Release 产物由 2 个 Windows 二进制扩展为 5 个平台二进制
- 新增跨平台安装发现（macOS `/Applications`、Linux deb/rpm 等）与系统代理读取（`scutil --proxy` / `gsettings`）
- 新增 `--insecure-tls` 参数：仅在企业中间人代理等确无法通过证书校验时显式关闭 HTTPS 校验
- 新增平台无关逻辑回归测试（`platform_logic_tests`）与映射文件失效项独立复核（`tests/check_localization.js`）
- 更新映射文件，适配 GitHub Desktop 3.5.11～3.6.10，并统一 minifier 别名写法

#### 修复与加固
- 修复 Windows 安装发现（改为只读注册表 + 本地 app 目录回退）、非 ASCII 安装路径写入失败
- 修复读取失败时把空串写回 `main.js`/`renderer.js`、替换串中 `$` 未转义导致内容被吞
- 修复写入失败仍返回 0、异常未统一捕获导致终止、网络请求无超时、交互输入无 EOF 保护
- 修复自更新命令路径未加引号（路径含空格/中文/`&` 时失败）、下载大小未校验、服务器忽略 Range 时临时文件残留
- 远程读取与自更新默认开启 HTTPS 证书校验，证书失败时给出一次性可操作提示
- 修复预览版特性残留（#48/#49）：loader 无条件跳过改写 `GITHUB_DESKTOP_PREVIEW_FEATURES` 或注入未完成菜单项的规则，手动 `enable:true`、很老的本地 json、`--rollback` 后重汉化均不再触发 3.6.10 未知菜单 id 崩溃
- 修复自更新固定取 `assets[0]`：改为按平台/架构产物名精确匹配，找不到当前平台产物时跳过更新并告警，不再下载、执行其它平台的二进制
- 修复映射文件中单条非法正则导致整轮替换中断并写回「只翻译一半」的 `main.js`/`renderer.js`；处理异常时保持原文件不变并返回非零退出码，`--invalidcheck` 不再漏检其后条目
- 修正 macOS / Linux arm64 构建被标记为 x64 的问题：新增 `CURRENT_PLATFORM_ISARM64`，运行时正确显示 arm64
- 构建期下载 Windows OpenSSL 预编译库启用 TLS 证书校验；补齐标准库头文件依赖，消除成员初始化顺序、`wstring`/`isdigit` 与弃用告警
- CI 工作流将第三方 action 固定到完整 commit SHA；`.gitattributes` 精简并显式声明二进制产物，`.gitignore` 补齐 CI 构建目录与自更新临时文件
- 文档与目录整理：`json/关于一些注意事项.txt` 转为 `json/localization_notes.md`，README 增加「项目目录结构」章节，校验脚本支持可选的 `--dev` 段检查
- `dist/` 中 Windows x64 / x86 二进制已用本次源码重新编译（v1.3.0 Release）

#### ⚠️ 升级前必读（行为变更）
- **默认开启 HTTPS 证书校验**：企业代理或自签证书环境若报证书错误，请先检查系统时间与代理设置，确无法通过时再加 `--insecure-tls`。程序自更新会下载并执行二进制，请勿随意关闭校验。
- **写入失败改为非零退出码**：此前写入失败仍返回 0（失败不会被发现）。CI / 脚本调用方请按退出码判断结果。
- **自更新按平台产物名匹配**：Release 资产中若找不到当前平台/架构对应的产物，会打印告警并跳过自动更新（此前可能误下其它平台二进制）。
- **`--rollback` 不再依赖 `localization.json` 与更新检查**：可离线从 `.bak` 备份还原汉化前文件。
- **Linux 无法检测 Shift 按键**：进入开发者选项请用 `./GitHubDesktop2Chinese_linux_x64 dev -d`。
- 原有命令行参数全部保留，本次未移除任何参数。

### 1.2.x 程序修复的bug和更新内容


#### 版本更新日志:
- 修复 3.5.5 版本js中包含文件中止符导致汉化后程序无法打开的问题（v1.2.0）
- 修复 提示用户输入的时候, 无法直接空行按下回车使用默认值的问题（v1.2.1）
- 新增 自动检测环境变量中的代理,自动检测系统代理,移除`-p`参数代理（v1.2.2）
- 修复 旧版控制台颜色代码未生效而显示乱码的问题（v1.2.3）
- 新增 更新新版本时的断点续传功能（v1.2.4）
- 修复 更新版本，请求GitHub时没有走代理的问题（v1.2.4）
- 新增 读取远程GithubDesktop最新版，和本地最新版比较，若有更新，则给出提示（v1.2.4）
- 新增 JSON文件附加描述文本，在加载器中显示（v1.2.4）

<!--
#### 修复BUG:
- 

#### 更新内容:
- 

-->


### 程序说明  
1. 可以仅下载二进制程序,双击运行后自动汉化  
2. 如果运行提示出错,可重试或下载[此JSON文件](https://github.com/cngege/GitHubDesktop2Chinese/blob/master/json/localization.json)后放在同目录后运行二进制程序  
3. **若使用GitHub 仓库作为json文件源，请升级加载器到最新版本**
4. `GitHubDesktop2Chinese.exe` 为64位程序
5. `GitHubDesktop2Chinese_x86.exe` 为32位程序
6. 如果汉化后主程序无法打开，请更新加载器后执行参数 `GitHubDesktop2Chinese.exe dev --translationfrombak` 或执行 x86.exe程序

### macOS 程序说明
1. `GitHubDesktop2Chinese_macos_arm64` 为 Apple Silicon（M1/M2/M3…）版本，`GitHubDesktop2Chinese_macos_x64` 为 Intel 芯片版本
2. 在“终端”中运行，首次需添加可执行权限：`chmod +x GitHubDesktop2Chinese_macos_*`
3. 若被 macOS 安全机制拦截（“无法验证开发者”），前往 `系统设置 → 隐私与安全性` 点击“仍要打开”，或执行 `xattr -d com.apple.quarantine <文件>`
4. 程序会自动发现 `/Applications` 与 `~/Applications` 下的 `GitHub Desktop.app`（读取其版本号并定位 `Contents/Resources/app/` 下的资源文件），无需手动指定目录
5. 移除汉化：执行 `./GitHubDesktop2Chinese_macos_* --rollback` 从备份还原
6. 代理：自动读取 `HTTPS_PROXY`/`https_proxy` 环境变量与系统代理（`scutil --proxy`）

### Linux 程序说明
1. `GitHubDesktop2Chinese_linux_x64` 为 Linux x86_64 版本
2. **重要**：GitHub Desktop 没有官方 Linux 版本，Linux 上使用的是社区分支 [shiftkey/desktop](https://github.com/shiftkey/desktop)（deb/rpm/AppImage/Flatpak/AUR）
3. 在终端中运行，首次需添加可执行权限：`chmod +x GitHubDesktop2Chinese_linux_x64`
4. deb/rpm 安装可自动发现资源目录（`/usr/lib/github-desktop/resources/app` 等）；**AppImage/Flatpak 需用 `-g` 手动指定资源目录**（AppImage 为单文件无法静态扫描）
5. Linux 上无法检测 Shift 按键，进入开发者选项请用：`./GitHubDesktop2Chinese_linux_x64 dev -d`
6. 移除汉化：执行 `./GitHubDesktop2Chinese_linux_x64 --rollback` 从备份还原
7. 代理：自动读取 `HTTPS_PROXY`/`https_proxy` 环境变量与 GNOME 系统代理（`gsettings`）
