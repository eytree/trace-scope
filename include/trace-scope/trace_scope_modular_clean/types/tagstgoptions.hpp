#ifndef TAGSTGOPTIONS_HPP
#define TAGSTGOPTIONS_HPP

/**
 * @file tagstgoptions.hpp
 * @brief tagSTGOPTIONS struct definition
 */

MSC_VER
    FILE* file = nullptr;
    if (tmpfile_s(&file) != 0) {
        return nullptr;
    }
    return file;
#else
    return std::tmpfile();
#

#endif // TAGSTGOPTIONS_HPP
