
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
