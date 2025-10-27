#ifndef ASYNC_QUEUE_HPP
#define ASYNC_QUEUE_HPP

/**
 * @file async_queue.hpp
 * @brief AsyncQueue struct definition
 */

namespace trace {

struct AsyncQueue {
    std::queue<Event> queue;
    std::mutex mutex;
    std::condition_variable cv;
    std::atomic<bool> running{false};
    std::thread worker;
    
    AsyncQueue();
    ~AsyncQueue();
    
    void push(const Event& event);
    void flush_now();
    void start();
    void stop();
};

} // namespace trace

#endif // ASYNC_QUEUE_HPP
