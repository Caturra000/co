#include <iostream>
#include <memory>
#include <vector>
#include "Coroutine.h"
#include "Environment.h"

void print1() {
    std::cout << 1 << std::endl;
    co::Coroutine::yield();
    std::cout << 2 << std::endl;
}

void print2(int i, co::Coroutine *co) {
    std::cout << i << std::endl;
    co->resume();
}

int main() {
    auto &env = co::Environment::instance();
    auto co = env.createCoroutine(print1);
    auto co2 = env.createCoroutine(print2, 3, co.get());
    co->resume();
    co2->resume();
    return 0;
}