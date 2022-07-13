#pragma once
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <array>
#include <chrono>
#include <unordered_map>
#include <memory>
#include <iostream>
#include "Coroutine.h"
#include "Utilities.h"

// posix.h文件提供一些常见POSIX接口的协程改造
// 比如用co::read替代::read
//
// 并不打算高度封装
//
// 这里做些规约：每个协程处理各自的fd，不共享给其它协程使用
// 这样我可以方便地管理event

namespace co {

struct PollConfig;
struct Event;

/// interface

ssize_t read(int fd, void *buf, size_t size);
ssize_t write(int fd, void *buf, size_t size);
int connect(int fd, const sockaddr *addr, socklen_t len);
int accept4(int fd, sockaddr *addr, socklen_t *len, int flags);


PollConfig& getPollConfig();
void loop();










/// implement

struct Event {
    enum Type {
        READ = 0,
        WRITE = 1,
        ERROR = 2,
        SIZE
    };
    // 0: POLLIN
    // 1: POLLOUT
    // 2: POLLERR
    using RoutineTable = std::array<std::shared_ptr<Coroutine>, 3>;
    RoutineTable routines;
    epoll_event event {};
};

struct PollConfig {
    // key: fd;
    // value: epoll_event
    using EventList = std::unordered_map<int, Event>;
    using Milliseconds = std::chrono::milliseconds;

    constexpr static auto DEFAULT_TIMEOUT = std::chrono::milliseconds(1000);

    int          epfd;
    Milliseconds timeout {DEFAULT_TIMEOUT};
    EventList    events;

    explicit PollConfig(int fd = -1): epfd(fd) {
        if(epfd < 0) {
            epfd = ::epoll_create1(EPOLL_CLOEXEC);
        }
        if(epfd < 0) {
            throw std::runtime_error("poll config");
        }
    }
    ~PollConfig() { ::close(epfd); }
    PollConfig(const PollConfig&) = delete;
    PollConfig& operator=(const PollConfig&) = delete;
};

inline PollConfig& getPollConfig() {
    static thread_local PollConfig config;
    return config;
}

// internal
inline auto addEvent(int fd, Event::Type type)
-> PollConfig::EventList::iterator {
    auto &config = getPollConfig();
    auto &events = config.events;
    int epfd = config.epfd;
    auto iter = events.find(fd);
    int op;
    bool newAdd = false;
    if(iter == events.end()) {
        op = EPOLL_CTL_ADD;
        auto where = events.insert({fd, {}});
        newAdd = true;
        iter = where.first;
        iter->second.event.data.fd = fd;
        auto &slot = iter->second.routines[type];
        slot = Coroutine::current().shared_from_this();
    } else {
        // impossible?
        op = EPOLL_CTL_MOD;
    }
    epoll_event *e = &iter->second.event;
    int newEvent = 0;
    switch(type) {
        case Event::READ:
            newEvent = EPOLLIN;
            break;
        case Event::WRITE:
            newEvent = EPOLLOUT;
            break;
        case Event::ERROR:
            newEvent = EPOLLERR;
            break;
    };
    // duplicate ?
    if(e->events & newEvent) {
        // revert
        if(newAdd) {
            events.erase(fd);
        }
        return events.end();
    }
    e->events |= newEvent;
    if(::epoll_ctl(epfd, op, fd, e)) {
        // std::cerr << "ctl failed: " << strerror(errno) << std::endl;
    }
    return iter;
}

inline ssize_t read(int fd, void *buf, size_t size) {
    // try
    ssize_t ret = ::read(fd, buf, size);
    // if ready
    if(ret > 0) return ret;

    auto &poll = getPollConfig();
    auto iter = addEvent(fd, Event::Type::READ);

    // 存在重复的关注事件
    // 返回0建议上层重试处理
    // FIXME. -1更好点？
    if(iter == poll.events.end()) {
        return 0;
    }

    this_coroutine::yield();

    // yield back from loop
    ret = ::read(fd, buf, size);
    return ret;
}

inline ssize_t write(int fd, void *buf, size_t size) {
    ssize_t ret;
    ret = ::write(fd, buf, size);
    if(ret > 0) return ret;
    auto &poll = getPollConfig();
    auto iter = addEvent(fd, Event::Type::WRITE);
    if(iter == poll.events.end()) {
        return 0;
    }
    this_coroutine::yield();
    ret = ::write(fd, buf, size);
    return ret;
}

inline int connect(int fd, const sockaddr *addr, socklen_t len) {
    while(1) {
        int ret;

        ret = ::connect(fd, addr, len);

        auto &poll = getPollConfig();
        auto iter = addEvent(fd, Event::Type::WRITE);
        if(iter == poll.events.end()) {
            errno = EPERM;
            return -1;
        }

        this_coroutine::yield();

        int soerr;
        socklen_t jojo = sizeof(soerr);
        if(::getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &jojo)) {
            errno = EPERM;
            return -1;
        }
        switch(soerr) {
            case 0: {
                sockaddr jojo;
                socklen_t dio = sizeof jojo;
                if(::getpeername(fd, &jojo, &dio)) {
                    // retry
                    continue;
                }
                return 0;
            }
            case EINTR:
            case EINPROGRESS:
            case EISCONN:
            case EALREADY:
            case ECONNREFUSED:
                // retry
                continue;
            default:
                errno = soerr;
                return -1;
        }
        return 0;
    }
}

inline int accept4(int fd, sockaddr *addr, socklen_t *len, int flags) {
    int ret = ::accept4(fd, addr, len, flags);
    if(ret >= 0) return ret;
    auto &poll = getPollConfig();
    auto iter = addEvent(fd, Event::Type::READ);
    // FIXME 这里只允许单个协程对同一fd进行accpet
    if(iter == poll.events.end()) return 0;
    this_coroutine::yield();
    ret = ::accept4(fd, addr, len, flags);
    return ret;
}

inline void loop() {
    auto &config = getPollConfig();
    // config may change
    // don't get / cache fields outside loop
    for(;;) {
        epoll_event e {};
        auto &eventList = config.events;
        int n = ::epoll_wait(config.epfd, &e, 1, config.timeout.count());
        // TODO 暂不处理errno
        if(n == 1) {
            int fd = e.data.fd;
            auto iter = eventList.find(fd);
            if(iter == eventList.end()) continue;
            ::epoll_ctl(config.epfd, EPOLL_CTL_DEL, fd, nullptr);
            auto routines = std::move(iter->second.routines);
            auto revent = iter->second.event;
            eventList.erase(iter);
            // 为了简化处理
            // 即使已关注的**部分**revent没有到来，也同样进行resume
            // 因为可能存在跨协程操作同一个fd，不处理就会丢失
            // 同时这要求接口必须是nonblock的，yield back处理可能得到err或者0
            //
            // 而且由于协程的性质，不可能同一个协程关注多个事件
            // 如果是按照约定使用，那么到来的事件中肯定只有一个slot是存在event的
            if(/* (revent.events & EPOLLIN) && */routines[Event::READ]) {
                routines[Event::READ]->resume();
            }
            if(/* (revent.events & EPOLLOUT) && */routines[Event::WRITE]) {
                routines[Event::WRITE]->resume();
            }
            if(/* (revent.events & EPOLLERR) && */routines[Event::ERROR]) {
                routines[Event::ERROR]->resume();
            }
        }
        // TODO 处理超时event
    }
}

} // co
