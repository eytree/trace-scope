#ifndef TAGNC_ADDRESS_HPP
#define TAGNC_ADDRESS_HPP

/**
 * @file tagnc_address.hpp
 * @brief tagNC_ADDRESS struct definition
 */

        std::string m = ini_parser::trim(value);
                // Convert to lowercase for case-insensitive comparison
                for (char& c : m) {
                    c = (char)std::tolower((unsigned char)c);
                }
 

#endif // TAGNC_ADDRESS_HPP
