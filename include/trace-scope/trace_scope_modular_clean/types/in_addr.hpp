#ifndef IN_ADDR_HPP
#define IN_ADDR_HPP

/**
 * @file in_addr.hpp
 * @brief in_addr struct definition
 */

OMMENDED: Simple one-line setup with TRC_SETUP_DLL_SHARED() macro:
 *
 *   In your main() function:
 *   @code
 *   #include <trace-scope/trace_scope.hpp>
 *   
 *   int main() {
 *       TRC_SETUP_DLL_SHARED();  // One line - automatic setup & cleanup!
 *       trace::get_config().out = std::fopen("trace.log", "w");
 *       // ... your code, including DLL calls
 *       return 0;  // Automatic cleanup via RAII
 *   }
 

#endif // IN_ADDR_HPP
