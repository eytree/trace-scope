#ifndef IP_MREQ_HPP
#define IP_MREQ_HPP

/**
 * @file ip_mreq.hpp
 * @brief ip_mreq struct definition
 */

    AUTO,      ///< Auto-detect: use shared if DLL detected (default)
    DISABLED,  ///< Never use shared memory (force thread_local)
    ENABLED    ///< Always use shared memory
}

#endif // IP_MREQ_HPP
