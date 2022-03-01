#include "Environment.h"

namespace co {

Environment& Environment::instance() {
    static thread_local Environment env;
    return env;
}

Coroutine* Environment::current() {
    return _cStack[_cStackTop - 1].get();
}

void Environment::push(std::shared_ptr<Coroutine> coroutine) {
    _cStack[_cStackTop++] = std::move(coroutine);
}

void Environment::pop() {
    _cStackTop--;
}

Environment::Environment(): _cStackTop(0) {
    _main = std::make_shared<Coroutine>(this, [](){});
    // TODO set State
    push(_main);
}

} // co
