#ifndef TRACE_SCOPE_ASYNC_QUEUE_HPP
#define TRACE_SCOPE_ASYNC_QUEUE_HPP

/**
 * @file AsyncQueue.hpp
 * @brief AsyncQueue struct definition for asynchronous event writing
 */

#include <cstdio>  // For FILE*
#include <vector>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <chrono>

namespace trace {

// Forward declarations
struct Event;
inline void print_event(const Event& e, FILE* out);

/**
 * @brief Asynchronous event queue for immediate mode tracing.
 * 
 * Provides non-blocking event enqueueing with background thread writing.
 * Used in immediate and hybrid tracing modes to avoid blocking traced threads.
 */
struct AsyncQueue {
    std::mutex mtx;                             ///< Protects queue access
    std::vector<Event> queue;                   ///< Event queue
    std::condition_variable cv;                 ///< Notifies writer thread of new events
    std::atomic<bool> running{false};           ///< Writer thread running flag
    std::thread writer_thread;                  ///< Background writer thread
    FILE* output_file = nullptr;                ///< Output file stream
    std::atomic<uint64_t> enqueue_count{0};     ///< Total events enqueued (for flush_now)
    std::atomic<uint64_t> write_count{0};       ///< Total events written (for flush_now)
    
    // Configuration (copied from Config on start())
    int flush_interval_ms = 1;                  ///< Flush interval in milliseconds
    size_t batch_size = 128;                    ///< Max events per batch write
    
    /**
     * @brief Constructor (does nothing - call start() to begin).
     */
    AsyncQueue() = default;
    
    /**
     * @brief Destructor: Stops writer thread and flushes remaining events.
     */
    ~AsyncQueue() {
        stop();
    }
    
    /**
     * @brief Start the async writer thread.
     * @param out Output file stream
     */
    inline void start(FILE* out) {
        if (running.load()) return;  // Already started
        
        output_file = out;
        running.store(true);
        writer_thread = std::thread([this]() { writer_loop(); });
    }
    
    /**
     * @brief Stop the writer thread and flush remaining events.
     */
    inline void stop() {
        if (!running.load()) return;  // Not running
        
        running.store(false);
        cv.notify_one();
        
        if (writer_thread.joinable()) {
            writer_thread.join();
        }
    }
    
    /**
     * @brief Enqueue an event (non-blocking, called from traced threads).
     * @param e Event to enqueue
     */
    inline void enqueue(const Event& e) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            queue.push_back(e);
        }
        enqueue_count.fetch_add(1, std::memory_order_relaxed);
        cv.notify_one();
    }
    
    /**
     * @brief Force immediate flush of queue (blocks until empty).
     * 
     * Waits up to 1 second for queue to drain. Used when synchronous
     * semantics are needed (e.g., before crash, in tests).
     */
    inline void flush_now() {
        // Wake up writer thread
        cv.notify_one();
        
        // Spin-wait until queue is drained (or timeout)
        auto start = std::chrono::steady_clock::now();
        while (enqueue_count.load(std::memory_order_relaxed) != 
               write_count.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            
            // Timeout after 1 second
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (elapsed > std::chrono::seconds(1)) {
                std::fprintf(stderr, "trace-scope: Warning: flush_immediate_queue() timeout after 1s\n");
                break;
            }
        }
    }
    
private:
    /**
     * @brief Background writer thread loop.
     * 
     * Waits for events with timeout (flush_interval_ms), drains queue,
     * and writes all events to file in a batch.
     */
    inline void writer_loop() {
        while (running.load(std::memory_order_relaxed)) {
            std::vector<Event> local;
            
            {
                std::unique_lock<std::mutex> lock(mtx);
                
                // Wait with timeout for new events
                cv.wait_for(lock, std::chrono::milliseconds(flush_interval_ms),
                           [this]() { 
                               return !queue.empty() || !running.load(std::memory_order_relaxed); 
                           });
                
                // Swap queues (fast, O(1))
                local.swap(queue);
            }
            
            // Write all events (outside lock - no contention with enqueue)
            for (const auto& e : local) {
                if (output_file) {
                    print_event(e, output_file);
                }
            }
            
            // Flush to disk and update write counter
            if (!local.empty() && output_file) {
                std::fflush(output_file);
                write_count.fetch_add(local.size(), std::memory_order_relaxed);
            }
        }
        
        // Final flush on shutdown - ensure no events lost
        std::vector<Event> remaining;
        {
            std::lock_guard<std::mutex> lock(mtx);
            remaining.swap(queue);
        }
        
        for (const auto& e : remaining) {
            if (output_file) {
                print_event(e, output_file);
            }
        }
        
        if (!remaining.empty() && output_file) {
            std::fflush(output_file);
            write_count.fetch_add(remaining.size(), std::memory_order_relaxed);
        }
    }
};

} // namespace trace

#endif // TRACE_SCOPE_ASYNC_QUEUE_HPP



