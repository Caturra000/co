#pragma once
#include <functional>
#include <memory>
#include "State.h"
#include "Context.h"

namespace co {

class Environment;

class Coroutine: public std::enable_shared_from_this<Coroutine> {
    friend class Environment;
    friend class Context;

public:
    static Coroutine& current();

    static void yield();

    void resume();

    // usage: Coroutine::current().yield()
    // void yield();

// 由于用到std::make_shared，必须公开这个构造函数
// TODO 设为private
public:
    template <typename Entry, typename ...Args>
    Coroutine(Environment *master, Entry entry, Args ...arguments)
        : _entry([=] { entry(std::move(arguments)...); }),
          _master(master) {}

private:
    static void callWhenFinish(Coroutine *coroutine);

private:
    State _runtime {};
    Context _context;
    std::function<void()> _entry;
    Environment *_master;
};

} // co
