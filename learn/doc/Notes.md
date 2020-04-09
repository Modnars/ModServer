# 笔记

## 说明

&#160; &#160; &#160; &#160; 以下所有章节顺序及序号，均来自于**《Linux高性能服务器编程》**这本书。这本书总体来说很适合学习实践，且书中会针对一些需要注意的地方进行强调，适合反复读一读，挑选自己经常需要的内容进行消化理解。

## 第5章 Linux网络编程基础API

### 5.8 数据读写

#### 5.8.1 TCP数据读写

- TCP数据流读写的系统调用

```cpp
#include <sys/types.h>
#include <sys/socket.h>
// 读取sockfd上的数据
// buf: 读缓冲区的位置  len: 读缓冲区的大小  flags: 通常设置为0即可
// 调用成功时，返回实际读到的数据的长度，它可能小于我们的期望长度len，因而可能需要多次
// 调用recv才能保证获取数据的完整；此外，recv可能返回0，这表示通信对方已经关闭了连接；
// recv调用出错返回-1，并设置errno。
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
// 向sockfd上写数据
// buf: 指定的缓存区的位置
// len: 缓冲区的大小
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
// 关于flags参数的选择，参见《Linux高性能服务器编程》第88页
```

&#160; &#160; &#160; &#160; Linux下对于“文件读”、“文件写”分别提供了`read`和`write`方法。当然这里的socket文件描述符`sockfd`也是被抽象成的一种文件，所以也可以使用这样的方法进行读写(书中所述，未做尝试)，而采用`recv`和`send`则是针对socket文件描述符的一种系统调用，所以对于网络编程数据传输，还是尽量采用这样的方式来进行数据读写。

#### 5.8.2 UDP数据读写

- UDP数据报读写的系统调用

```cpp
#include <sys/types.h>
#include <sys/socket.h>
// recvfrom读取sockfd上的数据，buf和len分别指定读缓冲区的位置和大小。由于UDP没有连接
// 的概念，所以每次读取都要获取发送端的socket地址，且addrlen指定该地址的长度
ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, 
        struct sockaddr *src_addr, socklen_t *addrlen);
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, 
        const struct sockaddr *dest_addr, socklen_t addrlen);
```

## 第6章 高级I/O函数

### 6.1 _**pipe**_ 函数

```cpp
#include <unistd.h>
/**
 * 创建管道
 *   将fd[0]、fd[1]作为管道的两端，往fd[1]中写入的数据可以从fd[0]中读出来，且方向不可
 *   倒置。
 * 函数成功时返回0，失败时返回-1并设置errno。
 */
int pipe(int fd[2]);
```

&#160; &#160; &#160; &#160; 关于管道，主要注意以下四种情况：

- 1. 当对一个空的管道进行read操作时，会被阻塞；

- 2. 当堆一个满的管道进行write操作时，会被阻塞；

- 3. 如果管道的写端描述符fd[1]的数量减少至0，则fd[0]处会读到EOF；

- 4. 如果管道的读端描述符fd[0]的数量减少至0，则fd[1]的写操作会失败，并引发SIGPIPE信号。

&#160; &#160; &#160; &#160; 此外，socket的基础API中有一个socketpair函数。它能够方便地创建双向管道。其定义如下：

```cpp
#include <sys/types.h>
#include <sys/socket.h>
int socketpair(int domain, int type, int protocal, int fd[2]);
```

&#160; &#160; &#160; &#160; 其中，domain只能使用UNIX本地域协议族AF\_UNIX，因为我们仅能在本地使用这个双向管道。socketpair成功时返回0，失败时返回-1并设置errno。

### 6.2 _**dup**_ 函数和 _**dup2**_ 函数

&#160; &#160; &#160; &#160; 可以通过用于复制文件描述符的dup或dup2函数来实现将标准输入重定向到一个文件，或者将标准输出重定向到一个网络连接。

```cpp
#include <unistd.h>
int dup(int file_descriptor);
int dup2(int file_descriptor_one, int file_descriptor_two);
```

&#160; &#160; &#160; &#160; dup函数创建一个新的文件描述符，这个新的文件描述符和原有的文件描述符指向相同的文件、管道或者网络连接，且dup返回的文件描述符总是取系统可提供的最小整数值。

&#160; &#160; &#160; &#160; dup2和dup类似，但它会返回第一个不小于file_descriptor_two的整数值。二者系统调用失败时均返回-1并设置errno。

> **注意**: <br/>&#160; &#160; &#160; &#160; 通过dup和dup2创建的文件描述符并不继承原文件描述符的属性，比如close-on-exec和non-blocking等。

### 6.3 _**readv**_ 函数和 _**writev**_ 函数

&#160; &#160; &#160; &#160; readv函数将数据从文件描述符读到分散的内存块中，即分散读；writev函数则将多块分散的内存数据一并写入到文件描述符中，即集中写。

```cpp
#include <sys/uio.h>
/**
 * 将数据从分散块中读或将数据写到分散块中
 *   fd: 被操作的目标文件描述符
 *   vector: iovec结构数组，该结构体描述一块内存区
 *   count: vector数组的长度，即有多少块内存数据需要从fd读出或写到fd
 * 成功时返回读出/写入fd的字节数，失败则返回-1并设置errno。他们相当于简化版的recvmsg和sendmsg函数。
 */
ssize_t readv(int fd, const struct iovec *vector, int count);
ssize_t writev(int fd, const struct iovec *vector, int count);
```

### 6.4 _**sendfile**_ 函数

&#160; &#160; &#160; &#160; `sendfile`函数在两个文件描述符之间直接传递数据(完全在内核中操作)，从而避免了内核缓冲区和用户缓冲区之间的数据拷贝，效率很高，这被称为零拷贝。

```cpp
#include <sys/sendfile.h>
/**
 * 在文件描述符间直接传递数据
 *   in_fd/out_fd分别为待读/待写文件描述符。
 *   offset: 指定从读入文件流的哪个位置开始读，如果为空，则采用读入文件流默认起始位置。
 *   count: 指定在文件描述符in_fd和out_fd之间传输的字节数。
 * 函数成功时返回传输的字节数，失败时返回-1并设置errno。
 */
ssize_t sendfile(int out_fd, int in_fd, off_t *offset, size_t count);
```

&#160; &#160; &#160; &#160; 该函数的man手册明确指出，`in_fd`必须是一个支持类似mmap函数的文件描述符，即它必须指向真实的文件，不能是socket和管道；而`out_fd`则必须是一个socket。由此可见，sendfile几乎是专门为在网络上传输文件而设计的。

### 6.5 _**mmap**_ 函数和 _**munmap**_ 函数

&#160; &#160; &#160; &#160; `mmap`用于申请一段内存，且这段内存可用于共享内存，也可以将文件直接映射到其中；`munmap`则用于释放由`mmap`创建的内存空间。

```cpp
#include <sys/mman.h>
/**
 * 申请一段内存空间
 *   start: 使用用户传入的地址作为这段内存空间的起始地址，若为NULL，则由系统进行分配
 *   length: 指定内存段长度
 *   prot: 设置内存段访问权限(PROT_READ 可读；PROT_WRITE 可写；EXEC 可执行；NONE 不能被访问)
 *   flags: 控制内存段内容被修改后程序的行为(例如MAP_SHARED共享)
 *   fd: 被映射文件对应的文件描述符，一般通过open系统调用获得
 *   offset: 设置从何处开始映射
 * 调用成功返回指向目标区域的指针，失败则返回MAP_FAILED((void *)-1)并设置errno。
 */
void *mmap(void *start, size_t length, int prot, int flags, int fd, off_t offset);
/**
 * 释放由mmap申请的空间
 * 成功时返回0，失败时返回-1并设置errno。
 */
int munmap(void *start, size_t length);
```

### 6.6 _**splice**_ 函数

&#160; &#160; &#160; &#160; `splice`函数用于在两个文件描述符之间移动数据，也是零拷贝操作。

```cpp
#include <fcntl.h>
/**
 * 文件描述符之间移动数据
 *   fd_in: 待输入数据的文件描述符，若fd_in是一个管道，则off_in必须为NULL；若fd_in不是一
 *          个管道(比如socket)，那么off_in表示从输入数据流的何处开始读数据
 *   len: 指定移动数据的长度
 *   flags: 控制数据如何移动
 * 成功时返回移动字节的数量，可能为0，表示没有数据需要移动，这发生在从管道中读取数据而
 * 该管道没有被写入任何数据时。调用失败时返回-1并设置errno。
 */
ssize_t splice(int fd_in, loff_t *off_in, int fd_out, loff_t *off_out, size_t len,
        unsigned int flags);
```

### 6.7 _**tee**_ 函数

&#160; &#160; &#160; &#160; `tee`函数用于在两个管道文件描述符之间复制数据，也是零拷贝操作。它不消耗数据。

```cpp
#include <fcntl.h>
/**
 * 函数参数含义与splice相同(但fd_in和fd_out必须都是管道文件描述符)
 * 调用成功时返回在两个文件描述符之间复制的数据总量(字节数)。
 * 返回0表示没有复制任何数据。失败时返回-1并设置errno。
 */
ssize_t tee(int fd_in, int fd_out, size_t len, unsigned int flags);
```

### 6.8 _**fcntl**_ 函数

&#160; &#160; &#160; &#160; `fcntl`函数(file control)，提供对文件描述符的各种控制操作。

```cpp
#include <fcntl.h>
int fcntl(int fd, int cmd, ...);
```

&#160; &#160; &#160; &#160; 常用的场景就是可以通过`fcntl`函数将文件设置为`非阻塞`的。

## 第11章 定时器

### 11.1 socket选项

### 11.2 SIGALRM信号

#### 11.2.1 基于升序链表的定时器

#### 11.2.2 处于非活动连接

### 11.3 I/O复用系统调用的超时参数

### 11.4 高性能定时器

#### 11.4.1 时间轮

#### 11.4.2 时间堆

## 第12章 高性能I/O框架Libevent

## 第14章 多线程编程

### 14.1 Linux线程概述

### 14.2 创建线程和结束线程

- _**pthread\_create**_

```cpp
#include <pthread.h>
/** 
 * 创建新线程
 *   thread: 新线程的标识符，后续pthread_*通过它来引用新线程
 *   attr: 用于设置新线程的属性，给它传递NULL表示使用默认线程属性
 *   start_rotinue: 指定新线程将运行的函数
 *   arg: 运行函数的参数
 * 数调用成功返回0，失败时返回错误码。
 */
int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_rotinue)(void *), void *arg);
// pthread_t的定义如下:
#include <bits/pthreadtypes.h>
typedef unsigned long int pthread_t;
```

- _**pthread\_exit**_

```cpp
#include <pthread.h>
/**
 * 线程函数在结束时最好调用pthread_exit函数，以确保安全、干净地退出。
 * 它执行完之后不会返回到调用者，并且永远都不会失败
 *   retval: pthread_exit通过retval参数向线程的回收者传递其退出信息
 */
void pthread_exit(void *retval);
```

- _**pthread\_join**_

```cpp
#include <pthread.h>
/**
 * 调用pthread_join来回收其他线程，即等待其他线程结束。类似于回收进程的
 * wait/waitpid系统调用。
 *   thread: 目标线程的标识符
 *   retval: 目标线程返回的退出信息
 * 该函数会一直阻塞，直到被回收的线程结束为止。成功时返回0，失败则返回错误码
 */
int pthread_join(pthread_t thread, void **retval);
// 错误码类型:
EDEADLK // 可能引起死锁。比如两个线程互相调用pthread_join，或者线程对自身调用pthread_join
EINVAL  // 目标线程是不可回收的，或者已经有其他线程在回收该目标线程
ESRCH   // 目标线程不存在
```

- _**pthread\_cancel**_

```cpp
#include <pthread.h>
/**
 * 异常终止一个线程，即取消线程
 *   thread: 目标线程的标识符
 * 该函数成功时返回0，失败则返回错误码。
 */
int pthread_cancel(pthread_t thread);

// 接收到取消请求的目标线程可以决定是否允许被取消以及如何取消，通过这两个函数来完成:
#include <pthread.h>
int pthread_setcancelstate(int state, int *oldstate);
int pthread_setcanceltype(int tyep, int *oldtype);
```

### 14.3 线程属性

### 14.4 POSIX信号量

### 14.5 互斥锁

#### 14.5.1 互斥锁基础API

&#160; &#160; &#160; &#160; POSIX互斥锁的相关函数主要有如下5个：

```cpp
#include <pthread.h>
// 下列函数成功时返回0，失败时返回错误码
/**
 * 初始化互斥锁
 *   mutexattr: 指定互斥锁的属性。若为NULL，则表示使用默认属性
 */
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr);
int pthread_mutex_destroy(pthread_mutex_t *mutex);
/**
 * 以原子操作的方式给一个互斥锁加锁。如果目标互斥锁已经被锁上，
 * 则pthread_mutex_lock调用将会阻塞，直到该互斥锁的占有者将其解锁
 */
int pthread_mutex_lock(pthread_mutex_t *mutex);
/**
 * 与pthread_mutex_lock类似，但本函数调用后立即返回，相当于上面函数的
 * 非阻塞版本。当已经被加锁时，返回错误码EBUSY。
 */
int pthread_mutex_trylock(pthread_mutex_t *mutex);
/**
 * 以原子操作的方式给一个互斥锁解锁。如果此时有其他线程正在等待这个
 * 互斥锁，则这些线程中的某一个将获得它。
 */
int pthread_mutex_unlock(pthread_mutex_t *mutex);
```

#### 14.5.2 互斥锁属性

#### 14.5.3 死锁举例

&#160; &#160; &#160; &#160; 死锁往往对应着两个线程互相等待(均被阻塞)，因而一些场景下总是会有出现死锁的危险，这对程序逻辑执行、计算机资源分配都造成了影响与损失。

- 一种常见的死锁情况，详见代码`deadlock.cpp`。
