#include "Coroutine.h"
#include "Environment.h"

namespace co {

Coroutine& Coroutine::current() {
    return *Environment::instance().current();
}

bool Coroutine::test() {
    return current()._context.test();
}

void Coroutine::resume() {
    if(!(_runtime & State::RUNNING)) {
        _context.prepare(Coroutine::callWhenFinish, this);
        _runtime |= State::RUNNING;
    }
    auto previous = _master->current();
    _master->push(shared_from_this());
    _context.switchFrom(&previous->_context);
}

// usage: Coroutine::current().yield()
// void Coroutine::yield() {
//     if(&current() != this) {
//         throw std::runtime_error("cannot yield");
//     }
//     _master->pop();
//     current()._context.switchFrom(&_context);
// }

void Coroutine::yield() {
    auto &coroutine = current();
    auto &currentContext = coroutine._context;

    coroutine._master->pop();

    auto &previousContext = current()._context;
    previousContext.switchFrom(&currentContext);
}

void Coroutine::callWhenFinish(Coroutine *coroutine) {
    auto &routine = coroutine->_entry;
    auto &runtime = coroutine->_runtime;
    if(routine) routine();
    runtime ^= (State::EXIT | State::RUNNING);
    // coroutine->yield();
    yield();
}

} // co