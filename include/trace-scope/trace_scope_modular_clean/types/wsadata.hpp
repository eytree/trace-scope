#ifndef WSADATA_HPP
#define WSADATA_HPP

/**
 * @file wsadata.hpp
 * @brief WSAData struct definition
 */

!text) return false;
    if (patterns.empty()) return false;
    
    for (const auto& pattern : patterns) {
        if (wildcard_match(pattern.c_str(), text)) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Check if event should be traced based on current filters.
 * 
 * Filter logic:
 * 1. Check depth filter (if set)
 

#endif // WSADATA_HPP
