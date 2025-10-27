#ifndef THIS_THREAD_HPP
#define THIS_THREAD_HPP

/**
 * @file this_thread.hpp
 * @brief namespace this_thread
 */

    size_t start = 0;
    size_t end = str.length();
    
    while (start < end && std::isspace((unsigned char)str[start])) ++start;
    while (end > start && std::isspace((unsigned char)str[end - 1])) --end;
    
    return str.substr(start, end - start);
}

/**
 * @brief Parse boolean value from string.
 * 
 * Accepts: true/false, 1/0, on/off, yes/no (case-insensitive)
 */
inline bool parse_bool(const std::string& value) {
    std::string v = trim(value);
    
    // Convert to lowercase for case-insensitive comparison
    for (char& c : v) {
        c = (char)std::tolower((unsigned char)c);
    }
    
    if (v == "true" || v == "1" || v == "on" || v == "yes") return true;
    if (v == "false" || v == "0" || v == "off" || v == "no") return false;
    
 

#endif // THIS_THREAD_HPP
