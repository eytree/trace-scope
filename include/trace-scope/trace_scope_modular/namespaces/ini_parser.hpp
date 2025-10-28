#ifndef INI_PARSER_HPP
#define INI_PARSER_HPP

/**
 * @file ini_parser.hpp
 * @brief INI file parsing utilities
 * 
 * Features:
 * - String trimming and cleaning
 * - Type-safe value parsing (bool, int, float)
 * - Quote handling for string values
 * - Error handling with sensible defaults
 */

#include <string>
#include <cctype>

namespace trace {

/**
 * @brief INI file parsing utilities.
 * 
 * Provides helper functions for parsing configuration values from INI files.
 */
namespace ini_parser {

/**
 * @brief Trim whitespace from both ends of a string.
 */
inline std::string trim(const std::string& str) {
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
    
    return false;  // Default to false on parse error
}

/**
 * @brief Parse integer value from string.
 */
inline int parse_int(const std::string& value) {
    try {
        return std::stoi(trim(value));
    } catch (...) {
        return 0;
    }
}

/**
 * @brief Parse float value from string.
 */
inline float parse_float(const std::string& value) {
    try {
        return std::stof(trim(value));
    } catch (...) {
        return 0.0f;
    }
}

/**
 * @brief Remove quotes from string if present.
 */
inline std::string unquote(const std::string& str) {
    std::string s = trim(str);
    if (s.length() >= 2 && s[0] == '"' && s[s.length()-1] == '"') {
        return s.substr(1, s.length() - 2);
    }
    return s;
}

} // namespace ini_parser

} // namespace trace

#endif // INI_PARSER_HPP
