#pragma once

// 平台架构检测
#if defined(_M_X64) || defined(__x86_64__) || defined(__ppc64__) || defined(__aarch64__)
    #define CP_ARCH_64 1
#else
    #define CP_ARCH_32 1
#endif

// 操作系统平台检测
#if defined(_WIN32)
    #define CP_PLATFORM_WINDOWS 1
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
#elif defined(__APPLE__)
    #include "TargetConditionals.h"
    #if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
        #define CP_PLATFORM_IOS 1
    #else
        #define CP_PLATFORM_MACOS 1
    #endif
#elif defined(__ANDROID__)
    #define CP_PLATFORM_ANDROID 1
#elif defined(__linux__)
    #define CP_PLATFORM_LINUX 1
#elif defined(__FreeBSD__)
    #define CP_PLATFORM_FREEBSD 1
#elif defined(__OpenBSD__)
    #define CP_PLATFORM_OPENBSD 1
#elif defined(__NetBSD__)
    #define CP_PLATFORM_NETBSD 1
#elif defined(__EMSCRIPTEN__)
    #define CP_PLATFORM_EMSCRIPTEN 1
#elif defined(__unix__)
    #define CP_PLATFORM_UNIX 1
#else
    #error "Unsupported platform"
#endif

// 编译器检测
#if defined(_MSC_VER)
    #define CP_COMPILER_MSVC 1
#elif defined(__clang__)
    #define CP_COMPILER_CLANG 1
#elif defined(__GNUC__) || defined(__GNUG__)
    #define CP_COMPILER_GCC 1
#elif defined(__INTEL_COMPILER)
    #define CP_COMPILER_INTEL 1
#else
    #define CP_COMPILER_UNKNOWN 1
#endif

// C++标准版本检测
#if __cplusplus >= 202002L
    #define CP_CXX_STANDARD 20
#elif __cplusplus >= 201703L
    #define CP_CXX_STANDARD 17
#elif __cplusplus >= 201402L
    #define CP_CXX_STANDARD 14
#elif __cplusplus >= 201103L
    #define CP_CXX_STANDARD 11
#else
    #define CP_CXX_STANDARD 98
#endif

// 字节序检测
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define CP_LITTLE_ENDIAN 1
#elif defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    #define CP_BIG_ENDIAN 1
#elif defined(_WIN32) || defined(__i386__) || defined(__x86_64__) || defined(__amd64__)
    #define CP_LITTLE_ENDIAN 1
#elif defined(__powerpc__) || defined(__ppc__) || defined(__arm__) || defined(__aarch64__)
    #define CP_BIG_ENDIAN 1
#else
    #error "Unable to determine endianness"
#endif

// 平台特性检测
#if defined(CP_PLATFORM_WINDOWS)
    #define CP_PATH_SEPARATOR '\\'
    #define CP_PATH_SEPARATOR_STR "\\"
    #define CP_PATH_ALT_SEPARATOR '/'
    #define CP_PATH_VOLUME_SEPARATOR ':'
#else
    #define CP_PATH_SEPARATOR '/'
    #define CP_PATH_SEPARATOR_STR "/"
    #define CP_PATH_ALT_SEPARATOR '\\'
    #define CP_PATH_VOLUME_SEPARATOR '\0'
#endif

// 调试模式检测
#if !defined(NDEBUG) || defined(_DEBUG) || defined(DEBUG)
    #define CP_DEBUG_MODE 1
#else
    #define CP_RELEASE_MODE 1
#endif

// 内联函数宏
#if defined(CP_COMPILER_MSVC)
    #define CP_FORCE_INLINE __forceinline
#elif defined(CP_COMPILER_CLANG) || defined(CP_COMPILER_GCC)
    #define CP_FORCE_INLINE inline __attribute__((always_inline))
#else
    #define CP_FORCE_INLINE inline
#endif

// 禁用复制和移动
#define CP_DISABLE_COPY(ClassName) \
    ClassName(const ClassName&) = delete; \
    ClassName& operator=(const ClassName&) = delete

#define CP_DISABLE_MOVE(ClassName) \
    ClassName(ClassName&&) = delete; \
    ClassName& operator=(ClassName&&) = delete

#define CP_DISABLE_COPY_MOVE(ClassName) \
    CP_DISABLE_COPY(ClassName); \
    CP_DISABLE_MOVE(ClassName)

// 导出/导入符号
#if defined(CP_PLATFORM_WINDOWS)
    #if defined(CP_COMPILER_MSVC)
        #define CP_EXPORT __declspec(dllexport)
        #define CP_IMPORT __declspec(dllimport)
    #else
        #define CP_EXPORT __attribute__((dllexport))
        #define CP_IMPORT __attribute__((dllimport))
    #endif
    #define CP_LOCAL
#else
    #if __GNUC__ >= 4
        #define CP_EXPORT __attribute__((visibility("default")))
        #define CP_IMPORT __attribute__((visibility("default")))
        #define CP_LOCAL  __attribute__((visibility("hidden")))
    #else
        #define CP_EXPORT
        #define CP_IMPORT
        #define CP_LOCAL
    #endif
#endif

// 静态库符号可见性
#ifndef CP_STATIC
    #ifdef CP_BUILD_SHARED
        #define CP_API CP_EXPORT
    #else
        #define CP_API CP_IMPORT
    #endif
#else
    #define CP_API
#endif

// 当前平台字符串
#if defined(CP_PLATFORM_WINDOWS)
    #define CP_CURRENT_PLATFORM_STR "Windows"
#elif defined(CP_PLATFORM_MACOS)
    #define CP_CURRENT_PLATFORM_STR "macOS"
#elif defined(CP_PLATFORM_LINUX)
    #define CP_CURRENT_PLATFORM_STR "Linux"
#elif defined(CP_PLATFORM_IOS)
    #define CP_CURRENT_PLATFORM_STR "iOS"
#elif defined(CP_PLATFORM_ANDROID)
    #define CP_CURRENT_PLATFORM_STR "Android"
#elif defined(CP_PLATFORM_FREEBSD)
    #define CP_CURRENT_PLATFORM_STR "FreeBSD"
#elif defined(CP_PLATFORM_EMSCRIPTEN)
    #define CP_CURRENT_PLATFORM_STR "Emscripten"
#else
    #define CP_CURRENT_PLATFORM_STR "Unknown"
#endif

// 当前编译器字符串
#if defined(CP_COMPILER_MSVC)
    #define CP_CURRENT_COMPILER_STR "MSVC"
#elif defined(CP_COMPILER_CLANG)
    #define CP_CURRENT_COMPILER_STR "Clang"
#elif defined(CP_COMPILER_GCC)
    #define CP_CURRENT_COMPILER_STR "GCC"
#elif defined(CP_COMPILER_INTEL)
    #define CP_CURRENT_COMPILER_STR "Intel"
#else
    #define CP_CURRENT_COMPILER_STR "Unknown"
#endif

// 调试断言
#if defined(CP_DEBUG_MODE)
    #include <cassert>
    #define CP_ASSERT(expr) assert(expr)
    #define CP_ASSERT_MSG(expr, msg) assert((expr) && (msg))
#else
    #define CP_ASSERT(expr) ((void)0)
    #define CP_ASSERT_MSG(expr, msg) ((void)0)
#endif

// 不使用的参数
#define CP_UNUSED(x) (void)(x)

// 禁用警告（跨编译器）
#if defined(CP_COMPILER_MSVC)
    #define CP_DISABLE_WARNING_PUSH __pragma(warning(push))
    #define CP_DISABLE_WARNING_POP __pragma(warning(pop))
    #define CP_DISABLE_WARNING(warningNumber) __pragma(warning(disable : warningNumber))
#elif defined(CP_COMPILER_CLANG) || defined(CP_COMPILER_GCC)
    #define CP_DISABLE_WARNING_PUSH _Pragma("GCC diagnostic push")
    #define CP_DISABLE_WARNING_POP _Pragma("GCC diagnostic pop")
    #define CP_DISABLE_WARNING(warningName) _Pragma(CP_STRINGIFY(GCC diagnostic ignored warningName))
#else
    #define CP_DISABLE_WARNING_PUSH
    #define CP_DISABLE_WARNING_POP
    #define CP_DISABLE_WARNING(warningNumber)
#endif

// 字符串化宏
#define CP_STRINGIFY_HELPER(x) #x
#define CP_STRINGIFY(x) CP_STRINGIFY_HELPER(x)

// 函数签名
#if defined(CP_PLATFORM_WINDOWS)
    #define CP_FUNCTION_SIG __FUNCSIG__
#elif defined(CP_PLATFORM_MACOS) || (defined(CP_COMPILER_CLANG) && __has_builtin(__builtin_FUNCTION))
    #define CP_FUNCTION_SIG __PRETTY_FUNCTION__
#elif defined(CP_COMPILER_GCC) || (defined(CP_COMPILER_CLANG) && __has_builtin(__builtin_FUNCTION))
    #define CP_FUNCTION_SIG __PRETTY_FUNCTION__
#else
    #define CP_FUNCTION_SIG __func__
#endif