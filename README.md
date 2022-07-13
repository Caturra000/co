# co

## TL;DR

简单的协程库，有以下特性：

* 非对称式+有栈协程
* 适用于`Linux x86_64`环境
* 提供友好的函数调用方式、闭包和协程检测机制
* `header-only`支持
* 协程化的`POSIX`接口适配

## 接口

接口很简单

### open()

每个线程下的`coroutine`环境都需要`co::open()`获取

`co::open()`操作可以重复调用，仅会在首次调用时初始化环境

### createCoroutine()

上述`co::open()`将返回一个`co::Environment&`实例

你可以通过它来创建协程，创建方式和`std::thread`的构造函数差不多，接受任意可调用对象

比如有一个函数接口为`print(int, char)`，可以这么写

`auto coroutine = environment.createCoroutine(print, 1, 'a')`

返回类型为`std::shared_ptr<co::Coroutine>`

注意创建好的协程`co::Coroutine`并不会立刻启动

### resume()

不管你是启动一个协程，还是恢复协程，都要`coroutine.resume()`

### yield()

让出自身的协程上下文

只允许当前在运行的协程让出，既`co::this_coroutine::yield()`

### test()

`co::test()`返回一个`bool`，表示当前执行的控制流是否位于协程上下文

这个有什么用？看你发挥

比如有栈协程本身栈空间就是比较小的（相比完整的线程），但是堆分配本身又很慢

那么在一些空间吃紧的场合下可以用`test()`来区分，实现协程上下文使用堆分配，线程/进程上下文使用栈分配

这需要应用层自己去实现

## 简单示例

```C++
#include <iostream>
#include <memory>
#include <vector>
#include "co.hpp"

void where() {
    std::cout << "running code in a "
              << (co::test() ? "coroutine" : "thread")
              << std::endl;
}

void print1() {
    std::cout << 1 << std::endl;
    co::this_coroutine::yield();
    std::cout << 2 << std::endl;
}

void print2(int i, co::Coroutine *co1) {
    std::cout << i << std::endl;
    co1->resume();
    where();
    std::cout << "bye" << std::endl;
}

int main() {
    auto &env = co::open();
    auto co1 = env.createCoroutine(print1);
    auto co2 = env.createCoroutine(print2, 3, co1.get());
    co1->resume();
    co2->resume();
    where();
    return 0;
}
// 1
// 3
// 2
// running code in a coroutine
// bye
// running code in a thread
```

## 进阶：POSIX接口

`co`支持`POSIX`风格的接口：比如用`co::read`替代系统调用`read`

其目的是以直观、同步的写法来完成异步事件，很轻松地完成高并发的服务器和客户端

目前已支持：

* `co::read`
* `co::write`
* `co::accept4`
* `co::connect`
* `co::usleep`

示例可以看`test_posix`前缀的文件，仅要求`fd`为`NONBLOCK`形式

原理还是控制流的切换，并且搭配`epoll`来作为一个隐藏的调度器

### TODO 事件机制

目前针对协程没有足够方便的事件通知机制，以降低生产者-消费者模型的编写难度，但是可以简单处理

如果只是一对一，可以用`pipefd`结合`co::read`即可，主动通知事件直接写1字节进去

如果是一对多或者多对多，可以自己写一个简单的`wait / notify`（见示例）

或者干脆用`co::usleep`等接口，生产者定时轮询，不需要生产者做任何事情

## 闲聊

关于这个协程库，它的实现方式和微信libco是一致的，感兴趣的话可以看下我写的libco关键源码剖析（[Caturra/RTFSC/libco](https://github.com/Caturra000/RTFSC/tree/master/libco)）

另外我对云风大佬的coroutine也挺感兴趣，他的做法恰好是和这个库相反的对称式协程加上共享栈模式，有时间也可以试着实现一下

不过在这之前，我还是想想，该如何把我之前写的网络库进行协程化改造（第三版coming soon！）
