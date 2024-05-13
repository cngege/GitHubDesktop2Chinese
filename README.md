# GitHubDesktop2Chinese

## 这是什么

> 这是一个自动替换GithubDesktop中文本为目标语言文本的程序，
  优点是对GithubDesktop频繁更新的版本变化兼容性比较高。
  即便只有那么一两条失去了翻译，也只需要手动再修改添加一下即可，
  项目维护成本很低

## 怎么使用它

> 第一种方法: 下载或自行构建 GitHubDesktop2Chinese.exe 双击运行, 自动联网获取最新localization.json完成汉化

> 第二种方法: 下载或自行构建 GitHubDesktop2Chinese.exe  和 localization.json  放在同一个文件夹
  运行程序即可
  
> GitHubDesktop每次版本更新都需要运行一次此程序才能完成汉化

## 怎么编译源代码

> 下载克隆项目 本地使用VS2022 使用CMAKE打开，即可构建

## 我怎么帮助完成汉化或者后续更新时如何补充汉化

> 只需要克隆仓库，阅读`json/关于一些注意事项.txt`，注意一些编写时的事项, 然后在`json/localization.json` 文件中参照已经写过的格式补充要汉化的条目即可
  将此文件放在程序目录下,确保程序可以读取运行 然后提交PR

## 其他

> 如果报错提示找不到openssl 的dll文件，则说明你的机器上没有安装openssl(在线获取最新本地化文件需要openssl的支持)  
  你可以在[这里](https://github.com/cngege/GitHubDesktop2Chinese/releases/download/0.1.8/openssl-dll.zip)下载，解压后将两个dll文件复制的exe同目录下即可  
  *我一直在寻找使用openssl库时不依赖其dll的方法,如果你恰巧知道,请一定要[![告诉我](http://rescdn.qqmail.com/zh_CN/htmledition/images/function/qm_open/ico_mailme_01.png)](mailto:2586850402@qq.com)或提交[Issues](https://github.com/cngege/GitHubDesktop2Chinese/issues)*
  **此方法我已经找到，当下最新exe无需依赖openssl dll**

## 第三方库  

**感谢以下诸位提供的优质的开源项目**  

- CLI: https://github.com/CLIUtils/CLI11  
- HttpLib: https://github.com/yhirose/cpp-httplib  
- nlohmann/json: https://github.com/nlohmann/json  
- spdlog: https://github.com/gabime/spdlog  
- WinReg: https://github.com/GiovanniDicanio/WinReg  

## TODO 

> 暂无




![图片](https://github.com/lkyero/GitHubDesktop_zh/assets/28597788/3023d028-8f63-4919-8900-ab3e953a1f76)
