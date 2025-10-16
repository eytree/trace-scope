#include <trace_scope.hpp>
#include <thread>
#include <chrono>
#include <filesystem>
#include <cassert>
#include <cstdio>

namespace fs = std::filesystem;

static void leaf(int n) {
    TRACE_SCOPE();
    TRACE_MSG("leaf n=%d", n);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

static void branch() {
    TRACE_SCOPE();
    for (int i=0;i<5;++i) leaf(i);
}

int main() {
    TRACE_SCOPE();
    // trace::config.out = std::fopen("test_trace.log", "w");

    std::thread t(branch);
    branch();
    t.join();

    trace::flush_all();
    bool ok = trace::dump_binary("test_trace.bin");
    assert(ok && "dump_binary failed");

    fs::path p("test_trace.bin");
    assert(fs::exists(p));
    auto sz = fs::file_size(p);
    assert(sz > 0);

    // Basic sanity: ensure we can pretty print without crashing
    // (User can run tools/trc_pretty.py manually.)
    return 0;
}
