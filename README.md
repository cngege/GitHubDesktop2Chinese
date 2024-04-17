# GitHubDesktop2Chinese

## 这是什么

> 这是一个自动替换GithubDesktop中文本为目标语言文本的程序,只需手动编写一次映射文件(如: File ：文件,  Close : 关闭)
  优点是对GithubDesktop频繁更新的版本变化兼容性比较高。
  即便只有那么一两条失去了翻译，也只需要手动再修改添加一下即可

## 怎么使用它

> 下载或自行构建 GitHubDesktop2Chinese.exe  和 localization.json  放在同一个文件夹
  运行程序即可
  GitHubDesktop每次版本更新都需要运行一次 此程序才能完成汉化

## 怎么编译源代码

> 下载克隆项目 本地使用VS2022 使用CMAKE打开，即可构建

## 我怎么帮助完成汉化或者后续更新时如何补充汉化

> 只需要克隆仓库，阅读`json/关于一些注意事项.txt`，注意一些编写时的事项, 然后在`json/localization.json` 文件中参照已经写过的格式补充要汉化的条目即可
  将此文件放在程序目录下,确保程序可以读取运行 然后提交PR
  
## TODO 

> (开发者功能)替换项失效检测




![图片](https://github.com/lkyero/GitHubDesktop_zh/assets/28597788/3023d028-8f63-4919-8900-ab3e953a1f76)
