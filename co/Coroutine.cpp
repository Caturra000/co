#include "Coroutine.h"
#include "Environment.h"

namespace co {

Coroutine& Coroutine::current() {
    return *Environment::instance().current();
}

void Coroutine::resume() {
    if(!_runtime.running) {
        _context.prepare(Coroutine::callWhenFinish, this);
        _runtime.running = true;
    }
    _master->push(shared_from_this());
    _context.switchFrom(&_master->previous()->_context);
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
    runtime.running = false;
    runtime.exit = true;
    // coroutine->yield();
    yield();
}

} // co
