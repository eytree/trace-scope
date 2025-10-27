#ifndef LINGER_HPP
#define LINGER_HPP

/**
 * @file linger.hpp
 * @brief linger struct definition
 */

#ifdef __linux__ || defined(__APPLE__)
        int flags = O_RDWR;
        if (create) flags |= O_CREAT | O_EXCL;
 

#endif // LINGER_HPP
