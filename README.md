# _ModServer_

&#160; &#160; &#160; &#160; 致力于实现一个包括**文件传输**与**聊天管理**的服务器，快来与 _**Modnar**_ 一起学习吧！

## 日志

- 2020/04/01

&#160; &#160; &#160; &#160; 回顾了关于网络的一些基础知识，并学习了一些socket通信函数。

- 2020/04/02

&#160; &#160; &#160; &#160; 学习参考书上的示例，复现了该实现代码，包括TCP数据传输、以及外带数据传输(待完善)。

- 2020/04/03

&#160; &#160; &#160; &#160; 读完了《Linux高性能服务器编程》，将重心放在 _chatServer_ 的实现，并开始尝试。

- 2020/04/04

&#160; &#160; &#160; &#160; 针对书中的一些内容，重点阅读并深入理解了一下，对于macOS下无法使用epoll进行I/O复用的问题，采取kqueue来进行实现。

- 2020/04/05

&#160; &#160; &#160; &#160; 调试程序，查阅关于`kqueue`、`EV_SET`、`kevent`等内容的博客经验，修改程序中的Bug：

    - 文件描述符阻塞问题(在写码时忽略了将文件属性设置为O\_NONBLOCK)，使得I/O复用表现异常。

    - 共享内存问题(由于一次程序非法退出，使得创建的一块共享内存未被回收，进而造成后续程序无法运行。通过在程序中先调用`shm_unlink`解决)。

    - 关于kqueue的使用问题，暂未解决。
