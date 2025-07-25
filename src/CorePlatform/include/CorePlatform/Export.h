#pragma once
#include "CorePlatform/Internal/PlatformDetection.h"

#if defined(CP_PLATFORM_WINDOWS)
    #if defined(CORE_PLATFORM_BUILD_SHARED)
        #define CORE_PLATFORM_API __declspec(dllexport)
    #elif defined(CORE_PLATFORM_USE_SHARED)
        #define CORE_PLATFORM_API __declspec(dllimport)
    #else
        #define CORE_PLATFORM_API
    #endif
#else
    #if __GNUC__ >= 4
        #define CORE_PLATFORM_API __attribute__ ((visibility ("default")))
    #else
        #define CORE_PLATFORM_API
    #endif
#endif