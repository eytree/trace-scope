#ifndef SCOPE_HPP
#define SCOPE_HPP

/**
 * @file scope.hpp
 * @brief Scope struct definition
 */

namespace trace {

struct Scope {
    const char* function;
    const char* file;
    uint32_t line;
    uint64_t start_time;
    
    Scope(const char* func, const char* f, uint32_t l);
    ~Scope();
};

} // namespace trace

#endif // SCOPE_HPP