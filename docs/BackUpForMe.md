### 教程

- [Markdown 官方教程](https://markdown.com.cn "Markdown 官方教程")
- [CMake 中文文档](https://cmake-doc.readthedocs.io/zh-cn/latest/index.html "CMake 3.26.4 Documentation")
- [clangd 官方介绍](https://clang.llvm.net.cn/ "clangd 官方介绍")
- [clang](https://clang.llvm.net.cn/ "clang")
- [C/C++ 参考手册](https://zh.cppreference.com/ "cppreference")
- [Hello 算法](https://www.hello-algo.com/chapter_hello_algo/ "Hello 算法")
- [信息学奥林匹克竞赛](http://oi-wiki.com/ "oi-wiki")

### 资源

- [Linux内核源码](https://elixir.bootlin.com/linux/latest/source "在线阅读 Linux内核源码")
- [Compiler Explorer](https://godbolt.org/ "在线C/C++编译")
- [KMS 地址列表](https://www.coolhub.top/tech-articles/kms_list.html "KMS 地址列表")
- [winlibs_mingw](https://github.com/brechtsanders/winlibs_mingw/releases "winlibs_mingw")


### 博客

- [weharmonyos](https://weharmonyos.com "weharmonyos")
- [爱编程的大丙](https://subingwen.cn "爱编程的大丙")

### 重置仓库

- `git checkout --orphan latest_branch`
- `git add -A`
- `git commit -m "init"`
- `git branch -D master`
- `git branch -m master`
- `git branch --set-upstream-to=origin/master master`
- `git push -f origin master`

### 仓库同步

- `git remote set-url --add --push origin git@gitcode.com:cpunchline/cpunchline-hub.git`
- `git remote set-url --add --push origin git@gitee.com:cpunchline/cpunchline-hub.git`
- `git remote set-url --add --push origin git@github.com:cpunchline/cpunchline-hub.git`
### 暂存
- `git stash --all` 包括未跟踪的内容

### 追溯

#### blame
- `-w` 忽略空格/移动代码等
- `git blame -w -C -C -C -L n1,n2 /path/to/file` 追溯一个指定文件的历史修改记录.它能显示任何文件中每行最后一次修改的提交记录
- `git blame -w -C -C -C -L n1,n2 /path/to/file` 追溯一个指定文件的历史修改记录
- `commitid (代码提交作者  提交时间  代码位于文件中的行数)  实际代码`

### show
- `git log`或者`git tag`过滤出来commit或者tag后; 使用`git show commitid/tag`或`git show tag/commitid:file`来查看详细信息
- `git log -L n1,n2:/path/to/file`; 上面那个`git blame`的历史修改记录的具体内容变动;
- `git log -L :File:/path/to/file`;

### config
- `git config --global rerere.enabled true` 合并冲突解决后, 记录它, 同样的冲突下次遇到会自己处理;
- `git config --global branch.sort -committerdate` `git branch` 按提交的倒序排序分支
- `git config --global core.fsmonitor true` 大仓库文件监控系统;

### push
`git push --force-with-lease` 安全的强制push

### maintenance
`git maintenance start` 后台创建crub定时任务定期更新/维护/精简/生成graph/仓库;

### clone
`git clone --filter=blob:none`; 不拉二进制;

- git 稀疏检出(sparse checkout)
    1. `git clone --no-checkout "xxx"`
    2. `git config core.sparsecheckout true`
    3. 指定待检出的相对于仓库根目录的文件/目录; `echo "restroy/xxx" >> .git/info/sparse-checkout`
    4. `git checkout master`
