#ifndef FILTER_UTILS_HPP
#define FILTER_UTILS_HPP

/**
 * @file filter_utils.hpp
 * @brief namespace filter_utils
 * 
 * Generated from trace_scope_original.hpp using AST extraction
 */

namespace trace {

namespace filter_utils {

/**
 * @brief Simple wildcard pattern matching (* matches zero or more characters).
 * 
 * @param pattern Pattern with * wildcards (e.g., "test_*", "*_func", "*mid*")
 * @param text Text to match against
 * @return true if text matches pattern
 */
inline bool wildcard_match(const char* pattern, const char* text) {
    if (!pattern || !text) return false;
    
    // Iterate through pattern and text
    while (*pattern && *text) {
        if (*pattern == '*') {
            // Skip consecutive wildcards
            while (*pattern == '*') ++pattern;
            
            // If wildcard is at end, match succeeds
            if (!*pattern) return true;
            
            // Try matching rest of pattern with each position in text
            while (*text) {
                if (wildcard_match(pattern, text)) return true;
                ++text;
            }
            return false;
        }
        else if (*pattern == *text) {
            ++pattern;
            ++text;
        }
        else {
            return false;
        }
    }
    
    // Handle trailing wildcards in pattern
    while (*pattern == '*') ++pattern;
    
    return (*pattern == '\0' && *text == '\0');
}

/**
 * @brief Check if string matches any pattern in list.
 * 
 * @param text Text to match against patterns
 * @param patterns List of wildcard patterns
 * @return true if text matches at least one pattern
 */
inline bool matches_any(const char* text, const std::vector<std::string>& patterns) {
    if (!text) return false;
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
 * 2. Check function filters (exclude wins over include)
 * 3. Check file filters (exclude wins over include)
 * 
 * @param func Function name (can be null for Msg events)
 * @param file File path
 * @param depth Current call depth
 * @return true if event should be traced, false if filtered out
 */
inline bool should_trace(const char* func, const char* file, int depth);

}
} // namespace trace


#endif // FILTER_UTILS_HPP
