#include <iostream>
#include <memory>
#include <vector>
#include "Coroutine.h"
#include "Environment.h"
#include "Utilities.h"

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