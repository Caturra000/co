#pragma once
#include <cstddef>
#include <cstring>
#include <functional>
#include <memory>
#include "Coroutine.h"

namespace co {

class Environment {
    friend class Coroutine;
public:
    static Environment& instance();

    template <typename Entry, typename ...Args>
    std::shared_ptr<Coroutine> createCoroutine(Entry &&entry, Args &&...arguments);

    Coroutine* current();

    Environment(const Environment&) = delete;
    Environment& operator=(const Environment&) = delete;

private:
    void push(std::shared_ptr<Coroutine> coroutine);
    void pop();
    Environment();

private:
    std::array<std::shared_ptr<Coroutine>, 1024> _cStack;
    size_t _cStackTop;
    std::shared_ptr<Coroutine> _main;
};

template <typename Entry, typename ...Args>
inline std::shared_ptr<Coroutine> Environment::createCoroutine(Entry &&entry, Args &&...arguments) {
    return std::make_shared<Coroutine>(
        this, std::forward<Entry>(entry), std::forward<Args>(arguments)...);
}

} // co
