#ifndef TRACESTREAM_HPP
#define TRACESTREAM_HPP

/**
 * @file tracestream.hpp
 * @brief TraceStream struct definition
 */

struct TraceStream {
    std::ostringstream ss;  ///< Stream buffer for collecting output
    const char* file;       ///< Source file
    int line;               ///< Source line
    
    /**
     * @brief Construct a stream logger.
     * @param f Source file path
     * @param l Source line number
     */
    TraceStream(const char* f, int l) : file(f), line(l) {}
    
    /**
     * @brief Destructor writes the collected stream to trace output.
     */
    ~TraceStream() {
#if TRC_ENABLED
        trace_msgf(file, line, "%s", ss.str().c_str());
#endif
    }
    
    /**
     * @brief Stream insertion operator.
     * @tparam T Type of value to stream
     * @param val Value to append to the stream
     * @return Reference to this for chaining
     */
    template<typename T>
    TraceStream& operator<<(const T& val) {
        ss << val;
        return *this;
    }
}

#endif // TRACESTREAM_HPP
