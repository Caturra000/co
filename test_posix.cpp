#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <iostream>
#include "co.hpp"

// 测试posix风格的协程接口
// 目前已支持：
// - accept4
// - read
// - write

int main() {
    auto &env = co::open();
    constexpr static size_t LIMIT = 1;

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
    if(::listen(server, LIMIT)) {
        ::exit(-4);
    }

    // 只处理LIMTI个连接，断连后不恢复
    for(auto countdown = LIMIT; countdown--;) {
        auto co = env.createCoroutine([&, countdown] {
            size_t index = LIMIT - countdown;
            std::cerr << "co: " << index << std::endl;

            sockaddr addr;
            socklen_t len;

            // connection
            int connection = co::accept4(server, &addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
            if(connection < 0) {
                std::cerr << "what? " << strerror(errno) << std::endl;
                std::cerr << "index: " << index << std::endl;
                return;
            }
            std::cout << "connection: " << connection << std::endl;

            // read-write echo
            while(1) {
                char buf[0xff];
                int n = co::read(connection, buf, sizeof buf);
                if(n < 0) {
                    std::cerr << "index " << index
                        << " read failed: " << strerror(errno) << std::endl;
                } else {
                    // 可能是0，比如对端已经关闭（已忽略broken pipe）
                    std::cout << "index " << index
                        << "read " << n << "bytes" << std::endl;

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
            }
        });

        co->resume();
    }

    co::loop();
}