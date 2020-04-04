# 笔记

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
