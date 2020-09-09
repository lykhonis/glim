#pragma once

#if GLIM_DEBUG

#include <iostream>

#define GLIM_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "Glim Error: " << message \
                      << " in " << __FILE__ \
                      << " line "<< std::dec << __LINE__ \
                      << std::endl; \
            std::abort(); \
        } \
    } while (false)

#define GLIM_ASSERT_GL_ERROR() \
    do { \
        auto glError = glGetError(); \
        if (glError != GL_NO_ERROR) { \
            std::cerr << "Glim GL Error: " << std::hex << "0x" << glError \
                      << " in " << __FILE__ \
                      << " line "<< std::dec << __LINE__ \
                      << std::endl; \
            std::abort(); \
        } \
    } while (false)

#else

#define GLIM_ASSERT(condition, message) \
        do { \
            (void)(condition); \
        } while (false)

#define GLIM_ASSERT_GL_ERROR() do {} while (false)

#endif
