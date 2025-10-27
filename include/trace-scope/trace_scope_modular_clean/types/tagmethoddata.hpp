#ifndef TAGMETHODDATA_HPP
#define TAGMETHODDATA_HPP

/**
 * @file tagmethoddata.hpp
 * @brief tagMETHODDATA struct definition
 */

const char* current_func = nullptr;
        int d = depth > 0 ? depth - 1 : 0;
        if (d < TRC_DEPTH_MAX) {
            current_func = func_stack[d];
        }
        
        // Hybrid mode: buffer AND print immediately
        if (get_config().mode == TracingMode::Hybrid) {
            // Create event and format message
 

#endif // TAGMETHODDATA_HPP
