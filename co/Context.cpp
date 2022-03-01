#include "Context.h"

extern "C" {
    extern void contextSwitch(co::Context*, co::Context*) asm("contextSwitch");
}

namespace co {

void Context::switchFrom(Context *previous) {
    contextSwitch(previous, this);
}

void Context::prepare(Context::Callback ret, Context::Word rdi) {
    Word sp = getSp();
    fillRegisters(sp, ret, rdi);
}

bool Context::test() {
    char jojo;
    ptrdiff_t diff = std::distance(std::begin(_stack), &jojo);
    return diff >= 0 && diff < STACK_SIZE;
}

Context::Word Context::getSp() {
    auto sp = std::end(_stack) - sizeof(Word);
    sp = decltype(sp)(reinterpret_cast<size_t>(sp) & (~0xF));
    return sp;
}

void Context::fillRegisters(Word sp, Callback ret, Word rdi, ...) {
    ::memset(_registers, 0, sizeof _registers);
    auto pRet = (Word*)sp;
    *pRet = (Word)ret;
    _registers[RSP] = sp;
    _registers[RET] = *pRet;
    _registers[RDI] = rdi;
}

} // co
