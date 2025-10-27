#ifndef _MODEMSETTINGS_HPP
#define _MODEMSETTINGS_HPP

/**
 * @file _modemsettings.hpp
 * @brief _MODEMSETTINGS struct definition
 */

red Memory Mode (shared_memory_mode):
 *   - "auto": Auto-detect if shared memory is needed (default)
 *   - "disabled": Force thread_local mode even in DLL contexts
 *   - "enabled": Always use shared memory mode
 *
 *   Example config file:
 *   @code
 *   [modes]
 *   flush_mode = outermost
 *   shared_memory_mode = auto
 *   @endcode
 */

#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#

#endif // _MODEMSETTINGS_HPP
