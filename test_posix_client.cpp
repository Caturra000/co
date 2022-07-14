#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <iostream>
#include "co.hpp"

void worker(int index, sockaddr_in addr) {
    std::cout << "co: " << index << std::endl;

    int client = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if(client < 0) {
        ::exit(-1);
    }

    // connection
    if(co::connect(client, (const sockaddr*)&addr, sizeof(addr))) {
        std::cerr << "what? " << strerror(errno) << std::endl;
        std::cerr << "index: " << index << std::endl;
        return;
    }
    std::cout << "connection: " << index << std::endl;

    auto log = [index](int nbytes, const char *failmsg, const char *succmsg) {
        if(nbytes < 0) {
            std::cerr << "index " << index << ' '
                << failmsg << ": " << strerror(errno) << std::endl;
        } else {
            // 可能是0，比如对端已经关闭（已忽略broken pipe）
            std::cout << "index " << index << ' '
                << succmsg << ": " <<  nbytes << " bytes" << std::endl;
        }
    };

    // read-write echo
    char jojo[] = "jojo \n";
    jojo[4] = (index % 10) + '0';
    int firstWrite = co::write(client, jojo, 6);
    log(firstWrite, "write failed", "write");

    char buf[0xff];

    while(1) {
        int n = co::read(client, buf, sizeof buf);
        log(n, "read failed", "read");
        // FIN?
        if(n == 0) break;

        if(n > 0) {
            n = co::write(client, buf, n);
            log(n, "write failed", "write");
        }
    }
}

int main(int argc, const char *argv[]) {
    if(argc < 2) {
        std::cerr << "ip?" << std::endl;
        return -1;
    }
    auto &env = co::open();
    constexpr static size_t WORKERS = 3;

    ::signal(SIGPIPE, SIG_IGN);

    sockaddr_in addr;
    ::memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(2333);
    addr.sin_addr.s_addr = ::inet_addr(argv[1]);

    for(auto index = 0; index < WORKERS; index++) {
        auto co = env.createCoroutine(worker, index, addr);
        co->resume();
    }

    co::loop();
}
