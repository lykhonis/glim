#pragma once

#if GLIM_DEBUG

#include <cstdlib>
#include <iostream>

#define GLIM_ASSERT(condition, message)                                 \
    do {                                                                \
        if (!(condition)) {                                             \
            std::cerr << "Glim Error: " << (message) << " in " << __FILE__ \
                      << " line " << std::dec << __LINE__ << std::endl; \
            std::abort();                                               \
        }                                                               \
    } while (false)

#else

#define GLIM_ASSERT(condition, message) \
    do {                                \
        (void)(condition);              \
        (void)(message);                \
    } while (false)

#endif
