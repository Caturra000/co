#pragma once

namespace co {

// 协程的运行时状态位
// 目前状态位比较少，不使用位图
struct State {
    bool main;
    bool idle;
    bool running;
    bool exit;
};

} // co
