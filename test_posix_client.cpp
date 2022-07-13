#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <iostream>
#include "co.hpp"

int main(int argc, const char *argv[]) {
    if(argc < 2) {
        std::cerr << "ip?" << std::endl;
        return -1;
    }
    auto &env = co::open();
    constexpr static size_t LIMIT = 3;

    ::signal(SIGPIPE, SIG_IGN);

    for(auto countdown = LIMIT; countdown--;) {

        int client = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if(client < 0) {
            ::exit(-1);
        }

        sockaddr_in addr;
        ::memset(&addr, 0, sizeof addr);
        addr.sin_family = AF_INET;
        addr.sin_port = ::htons(2333);
        // hard code
        addr.sin_addr.s_addr = ::inet_addr(argv[1]);


        auto co = env.createCoroutine([=] {
            size_t index = LIMIT - countdown;
            std::cerr << "co: " << index << std::endl;

            // connection
            if(co::connect(client, (const sockaddr*)&addr, sizeof(addr))) {
                std::cerr << "what? " << strerror(errno) << std::endl;
                std::cerr << "index: " << index << std::endl;
                return;
            }
            std::cout << "connection: " << index << std::endl;

            // read-write echo
            char jojo[] = "jojo \n";
            int fill = index > 10 ? 0 : index;
            jojo[4] = index + '0';
            int firstWrite = co::write(client, jojo, 6);
            if(firstWrite >= 0) {
                std::cout << "index " << index
                    << "write " << firstWrite << " bytes" << std::endl;
            } else if(firstWrite < 0) {
                std::cout << "index " << index
                    << "write failed:" << strerror(errno) << std::endl;
            }
            while(1) {
                char buf[0xff];
                int n = co::read(client, buf, sizeof buf);
                if(n < 0) {
                    std::cerr << "index " << index
                        << " read failed: " << strerror(errno) << std::endl;
                } else {
                    // 可能是0，比如对端已经关闭（已忽略broken pipe）
                    std::cout << "index " << index
                        << " read " << n << "bytes" << std::endl;

                    if(n == 0) break;
                }
                n = co::write(client, buf, n);
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
