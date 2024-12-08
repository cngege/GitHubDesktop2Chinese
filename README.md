# GitHubDesktop2Chinese

![GitHub Actions](https://img.shields.io/github/actions/workflow/status/cngege/GitHubDesktop2Chinese/build.yml)
![GitHub Actions](https://img.shields.io/github/actions/workflow/status/cngege/GitHubDesktop2Chinese/check.yml?label=合并检查)
![GitHub Release](https://img.shields.io/github/v/release/cngege/GitHubDesktop2Chinese)
![GitHub Release](https://img.shields.io/badge/Cpp-json-blue?logo=cplusplus)
![GitHub License](https://img.shields.io/github/license/cngege/GitHubDesktop2Chinese)



## 🥮这是什么🥮

> 这是一个自动替换GithubDesktop中文本为目标语言文本的程序，  
  优点是对GithubDesktop频繁更新的版本变化兼容性比较高。  
  即便只有那么一两条失去了翻译，也只需要手动再修改添加一下即可，  
  项目维护成本很低  

## 🎯怎么使用它🎯

[🎀GOTO BiliBili Video🎀](https://www.bilibili.com/video/BV17HpSeHEaC/)

> 第一种方法: 下载或自行构建 GitHubDesktop2Chinese.exe 双击运行, 自动联网获取最新localization.json完成汉化  
  第二种方法: 下载或自行构建 GitHubDesktop2Chinese.exe  和 localization.json  放在同一个文件夹。运行程序即可  
  GitHubDesktop每次版本更新都需要运行一次此程序才能完成汉化

## 🎏怎么编译源代码🎏

> 下载克隆项目 本地使用VS2022 使用CMAKE打开，即可构建

## 👕我怎么帮助完成汉化或者后续更新时如何补充汉化👕

> 只需要克隆仓库，阅读`json/关于一些注意事项.txt`，注意一些编写时的事项, 然后在`json/localization.json` 文件中参照已经写过的格式补充要汉化的条目即可  
  将此文件放在程序目录下,确保程序可以读取运行 然后提交PR

## 🍬映射文件：localization.json🍬
> 此文件存储所有GitHubDesktop中英文文本到本地化(中文)文本之间的映射，  
  使用正则匹配的方式 将包含英文字符串的文本替换为包含中文字符串的文本。  
  项目更新主要更新此文件。
  
- 路径: [项目目录]/json/localization.json 
- 主节点 - version(int): 此json文件的版本,此值仅会因未来格式更新而更新 
- 主节点 - minversion(string): 需要最低的加载器版本 
- 主节点 - main(array): 存储用于替换GitHubDesktop的main.js的映射 
- 主节点 - main_dev(array): 存储用于替换GitHubDesktop的main.js的映射,区别于开发时快速替换 
- 主节点 - renderer(array): 存储用于替换GitHubDesktop的renderer.js的映射 
- 主节点 - renderer_dev(array): 存储用于替换GitHubDesktop的renderer.js的映射,区别于开发时快速替换 


## 🧭其他🧭

> 如果报错提示找不到openssl 的dll文件，请更新到[最新版](https://github.com/cngege/GitHubDesktop2Chinese/releases)   
  如果你有任何建议可以提issus.


## 🍬第三方库🍬  
**感谢以下诸位提供的优质的开源项目**  

- CLI: https://github.com/CLIUtils/CLI11  
- HttpLib: https://github.com/yhirose/cpp-httplib  
- nlohmann/json: https://github.com/nlohmann/json  
- spdlog: https://github.com/gabime/spdlog  
- WinReg: https://github.com/GiovanniDicanio/WinReg  

## 🎋TODO🎋  
 - [x] json文件格式修改,用于标识文件版本、最低支持加载器（GitHubDesktop2Chinese.exe）的版本
 - [x] 加载器加入程序版本宏定义
 - [x] 加载器支持替换映射的第三个参数,即查找参数,将第三个参数（如果有）进行全局正则查找,  
       将匹配到的结果在第二个参数中进行特殊字符的替换，替换标记为`#{number}`
 - [x] 加载器检查json文件所需最低版本是否满足需求,如果不满足且定义了`--nopause`则提示后退出,
       否则提示后询问是否强制替换
 - [x] 加载器在没有定义`--nopause`的情况下,在替换之前进行一次暂停,以便用户自行确认信息后是否往下执行

## 🍬星标(收藏)历史🍬
![Star History Chart](https://api.star-history.com/svg?repos=cngege/GitHubDesktop2Chinese&type=Date)

## 🏘️感谢大家的群策群力🏘️  
![Contributors](https://contrib.rocks/image?repo=cngege/GitHubDesktop2Chinese)

<details> 
    <summary>点击展开示例图片</summary>
<img src="https://github.com/lkyero/GitHubDesktop_zh/assets/28597788/3023d028-8f63-4919-8900-ab3e953a1f76" alt="展示图" />
</details>


