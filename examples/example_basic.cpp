#include "../include/trace_scope.hpp"
#include <thread>
#include <chrono>
#include <cstdio>

void bar(int i) {
    TRACE_SCOPE();
    TRACE_MSG("bar start i=%d", i);
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    TRACE_MSG("bar end i=%d", i);
}

void foo() {
    TRACE_SCOPE();
    for (int i=0;i<3;++i) bar(i);
}

int main() {
    TRACE_SCOPE();
    trace::config.out = std::fopen("trace.log", "w");

    std::thread t1([]{
        TRACE_SCOPE();
        TRACE_MSG("t1 starting");
        foo();
        TRACE_MSG("t1 done");
        trace::flush_ring(trace::thread_ring());
    });

    foo();
    t1.join();

    trace::flush_all();
    trace::dump_binary("trace.bin");
    return 0;
}
