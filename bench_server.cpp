#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <iostream>
#include <vector>
#include <thread>
#include "co.hpp"

// script: https://github.com/Caturra000/FluentNet/blob/master/bench/fluent_throughput/bench.py
// client: https://github.com/Caturra000/FluentNet/blob/master/bench/fluent_throughput/asio_client.cpp



// return: server fd
int prepare();

void worker(int index, int connection);
void listener(int server);



int main(int argc, const char *argv[]) {
    if(argc < 2) {
        std::cerr << "threads?" << std::endl;
        return -1;
    }

    std::vector<std::thread> workers;
    int threads = ::atoi(argv[1]);

    for(auto t = 0; t < threads; ++t) {
        workers.emplace_back([=] {
            auto &env = co::open();
            int fd = prepare();
            auto co = env.createCoroutine(listener, fd);
            co->resume();
            co::loop();
        });
    }
    for(auto &&w : workers) w.join();
    return 0;
}




int prepare() {
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
    if(::setsockopt(server, SOL_SOCKET, SO_REUSEPORT, &opt,
            static_cast<socklen_t>(sizeof opt))) {
        ::exit(-3);
    }
    sockaddr_in addr;
    ::memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(2533);
    addr.sin_addr.s_addr = ::htonl(INADDR_ANY);
    if(::bind(server, (const sockaddr*)&addr, sizeof addr)) {
        ::exit(-4);
    }
    if(::listen(server, SOMAXCONN)) {
        ::exit(-5);
    }
    return server;
}

void worker(int index, int connection) {
    // read-write echo
    char buf[65538];
    while(1) {
        int n = co::read(connection, buf, sizeof buf);

        int lea = n;
        int start = 0;
        while(lea > 0) {
            int consume = co::write(connection, buf + start, lea);
            if(consume > 0) {
                lea -= consume;
                start += consume;
            }
        }
    }
}

void listener(int server) {
    sockaddr addr;
    socklen_t len = sizeof addr;
    int index = 0;
    auto &env = co::open();
    co::getPollConfig().timeout = {};
    while(1) {
        int connection = co::accept4(server, &addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if(connection < 0) {
            continue;
        }
        int optval = true;
        ::setsockopt(connection, IPPROTO_TCP, TCP_NODELAY, &optval,
                static_cast<socklen_t>(sizeof optval));
        auto co = env.createCoroutine(worker, index++, connection);
        co->resume();
    }
}
