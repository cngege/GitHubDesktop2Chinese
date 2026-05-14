
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
