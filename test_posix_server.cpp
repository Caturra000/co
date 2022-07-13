#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <iostream>
#include <deque>
#include <vector>
#include "co.hpp"

// 测试posix风格的协程接口
// 目前已支持：
// - connect
// - accept4
// - read
// - write
// - usleep

// ready connections
static std::deque<int> fdPool;

// pending workers
// and simple global notification
static std::vector<std::shared_ptr<co::Coroutine>> pendings;
void wait();
void notify();

// index: worker index
void worker(int index);

// server: server fd
void listener(int server);

int main() {
    auto &env = co::open();
    constexpr static size_t WORKERS = 10;

    ::signal(SIGPIPE, SIG_IGN);

    int server = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if(server < 0) {
        ::exit(-1);
    }

    int opt = 1;
    if(::setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt,
            static_cast<socklen_t>(sizeof opt))) {
        ::exit(-2);
    }

    sockaddr_in addr;
    ::memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(2333);
    addr.sin_addr.s_addr = ::htonl(INADDR_ANY);

    if(::bind(server, (const sockaddr*)&addr, sizeof addr)) {
        ::exit(-3);
    }
    if(::listen(server, SOMAXCONN)) {
        ::exit(-4);
    }

    for(auto index = 0; index < WORKERS; index++) {
        auto co = env.createCoroutine(worker, index);
        co->resume();
    }

    auto co = env.createCoroutine(listener, server);
    co->resume();

    co::loop();
}

auto current() -> std::shared_ptr<co::Coroutine> {
    return co::Coroutine::current().shared_from_this();
}

void wait() {
    pendings.emplace_back(current());
    co::this_coroutine::yield();
}

void notify() {
    if(!pendings.empty()) {
        auto wakeup = *--pendings.end();
        pendings.pop_back();
        wakeup->resume();
    }
}

void worker(int index) {
    std::cerr << "co: " << index << std::endl;

    char roll = 0;
    // 让每个connection都有机会处理到，即使worker小于client数目
    auto pick = [&roll] {
        int fd;
        if(roll) {
            fd = fdPool.front();
            fdPool.pop_front();
        } else {
            fd = fdPool.back();
            fdPool.pop_back();
        }
        roll ^= 1;
        return fd;
    };

    // read-write echo
    while(1) {
        // 此时无法消费
        if(fdPool.empty()) {
            // 可以使用简单的wait-notify封装
            // 消费者wait，需要生产者配合notify
            wait();

            // 或者使用usleep协程定时机制
            // 不需要生产者主动通知
            // co::usleep(1000);

            continue;
        }
        auto connection = pick();
        char buf[0xff];
        int n = co::read(connection, buf, sizeof buf);
        if(n < 0) {
            std::cerr << "index " << index
                << " read failed: " << strerror(errno) << std::endl;
        } else {
            // 可能是0，比如对端已经关闭（已忽略broken pipe）
            std::cout << "index " << index
                << "read " << n << " bytes" << std::endl;

            if(n == 0) break;
        }
        n = co::write(connection, buf, n);
        if(n < 0) {
            std::cerr << "index " << index
                << " write failed: " << strerror(errno) << std::endl;
        } else {
            std::cout << "index " << index
                << " write " << n << " bytes" << std::endl;

            if(n == 0) break;
        }

        fdPool.push_back(connection);
    }
}

void listener(int server) {
    sockaddr addr;
    socklen_t len = sizeof addr;
    while(1) {
        int connection = co::accept4(server, &addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if(connection < 0) {
            std::cerr << "what? " << strerror(errno) << std::endl;
            continue;
        }
        std::cout << "connection: " << connection << std::endl;
        fdPool.push_back(connection);
        notify();
    }
}
