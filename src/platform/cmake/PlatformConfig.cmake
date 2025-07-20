# 平台检测
if(WIN32)
    message(STATUS "Platform: Windows")
    set(PLATFORM_WINDOWS ON)
    set(PLATFORM_NAME "windows")
    
    # Windows 上使用 Clang-CL
    set(CMAKE_C_COMPILER "clang-cl" CACHE STRING "C compiler")
    set(CMAKE_CXX_COMPILER "clang-cl" CACHE STRING "C++ compiler")
    
    # Windows 特定设置
    add_definitions(-D_WIN32_WINNT=0x0A00)
    add_definitions(-DUNICODE -D_UNICODE)
    
    # 架构优化
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64")
        add_compile_options(/arch:AVX2)
    endif()

    if(MSVC)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
        add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    endif()
    
    # 链接库
    # set(PLATFORM_LIBS Wbemuuid Ole32 OleAut32)

    # Windows 特定编译选项 (Clang-CL 兼容)
    add_compile_options(
        /Zc:__cplusplus  # 启用正确 __cplusplus 值
        /permissive-      # 启用标准一致性模式
        /Zc:inline        # 移除未使用的 COMDAT
        /Gw               # 全局数据优化 (等效于 -fdata-sections)
        /Gy               # 函数级链接 (等效于 -ffunction-sections)
    )
    
    # Windows 特定链接选项
    add_link_options(
        /OPT:REF  # 移除未使用的函数
        /OPT:ICF   # 相同 COMDAT 折叠
    )

elseif(APPLE)
    message(STATUS "Platform: macOS")
    set(PLATFORM_MACOS ON)
    set(PLATFORM_NAME "macos")
    
    # macOS 使用标准 Clang
    set(CMAKE_C_COMPILER "clang" CACHE STRING "C compiler")
    set(CMAKE_CXX_COMPILER "clang++" CACHE STRING "C++ compiler")
    
    # macOS 通用二进制设置
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
    
    # 通用二进制编译选项
    add_compile_options(
        -arch x86_64
        -arch arm64
    )
    
    # 链接选项
    add_link_options(
        -arch x86_64
        -arch arm64
    )
    
    # 查找框架
    find_library(COCOA_LIB Cocoa)
    find_library(IOKIT_LIB IOKit)
    set(PLATFORM_LIBS ${COCOA_LIB} ${IOKIT_LIB})

elseif(UNIX AND NOT APPLE)
    message(STATUS "Platform: Linux")
    set(PLATFORM_LINUX ON)
    set(PLATFORM_NAME "linux")
    
    # Linux 使用标准 Clang
    set(CMAKE_C_COMPILER "clang" CACHE STRING "C compiler")
    set(CMAKE_CXX_COMPILER "clang++" CACHE STRING "C++ compiler")
    
    # Linux 设置
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    
    # 架构优化
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
        add_compile_options(-mcpu=native)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
        add_compile_options(-march=native -mtune=native)
    endif()
    
    # 链接库
    set(PLATFORM_LIBS dl pthread)

endif()

# 构建类型特定设置
string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_UPPER)
if(BUILD_TYPE_UPPER STREQUAL "DEBUG")
    if(PLATFORM_WINDOWS)
        add_compile_options(/Zi /Od)  # Windows 调试选项
    else()
        add_compile_options(-g -O0)   # 其他平台调试选项
    endif()
    add_definitions(-DDEBUG)
    message(STATUS "Build type: Debug (optimization disabled)")
    
elseif(BUILD_TYPE_UPPER STREQUAL "RELEASE")
    if(PLATFORM_WINDOWS)
        add_compile_options(/O2 /Ob2)  # Windows 发布优化
    else()
        add_compile_options(-O3)       # 其他平台发布优化
    endif()
    add_definitions(-DNDEBUG)
    message(STATUS "Build type: Release (max optimization)")
    
elseif(BUILD_TYPE_UPPER STREQUAL "MINSIZEREL")
    if(PLATFORM_WINDOWS)
        add_compile_options(/O1 /Os)   # Windows 最小大小优化
    else()
        add_compile_options(-Oz)       # 其他平台最小大小优化
    endif()
    add_definitions(-DNDEBUG)
    message(STATUS "Build type: MinSizeRel (size optimization)")
    
elseif(BUILD_TYPE_UPPER STREQUAL "RELWITHDEBINFO")
    if(PLATFORM_WINDOWS)
        add_compile_options(/Zi /O2)   # Windows 带调试的优化
    else()
        add_compile_options(-g -O2)   # 其他平台带调试的优化
    endif()
    add_definitions(-DNDEBUG)
    message(STATUS "Build type: RelWithDebInfo (optimized with debug info)")
    
else()
    message(WARNING "Unknown build type: ${CMAKE_BUILD_TYPE}. Using default settings.")
endif()

# 公共编译选项
if(PLATFORM_WINDOWS)
    # Windows 特定公共选项 (Clang-CL)
    add_compile_options(
        /W4           # 高级别警告
        # /WX           # 将警告视为错误（但未使用参数除外）
        /Zc:rvalueCast # 启用正确的右值转换
        /EHsc         # 启用C++异常
        
        # 禁用特定警告（使其不视为错误）
        # /clang:-Wno-error=unused-parameter  # 未使用参数不视为错误
        /clang:-Wno-unused-parameter        # 完全禁用未使用参数警告（可选）

        /permissive-          # 严格标准符合
    )
else()
    # 非 Windows 平台的公共选项
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -fcolor-diagnostics
        -fstrict-aliasing
        -ffunction-sections
        -fdata-sections
        -fexceptions                 # 启用c++异常
        
        # 禁用未使用参数视为错误
        -Wno-error=unused-parameter  # 未使用参数不视为错误
        -Wno-unused-parameter        # 完全禁用未使用参数警告（可选）

        -fstack-protector-strong  # 增强栈保护
    )
    
    # 非 Windows 平台的链接选项
    add_link_options(
        -Wl,--gc-sections
        -Wl,-z,relro,-z,now      # 安全加固
    )
endif()

# 所有平台的公共选项
add_compile_options(
    # 启用 C++20 特性
    /std:c++20    # Windows 风格
    $<$<NOT:$<PLATFORM_ID:Windows>>:-std=c++20>  # 其他平台
    
    # 其他通用选项
    -mno-avx512f  # 禁用 AVX-512 以兼容更多硬件
)

# 启用 C++20 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_DEBUG_POSTFIX "d")

# 启用 Clang 的静态分析
set(CMAKE_CXX_CLANG_TIDY clang-tidy)

# 设置输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 安装后修复 macOS 二进制路径
if(APPLE)
    set(CMAKE_INSTALL_NAME_DIR "@executable_path/../lib")
    set(CMAKE_INSTALL_RPATH "@executable_path/../lib")
endif()

# 添加源码分组辅助函数
function(add_source_group GROUP_NAME)
    foreach(FILE ${ARGN})
        get_filename_component(PARENT_DIR "${FILE}" DIRECTORY)
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" GROUP "${PARENT_DIR}")
        
        if(WIN32)
            string(REPLACE "/" "\\" GROUP "${GROUP}")
        endif()
        
        source_group("${GROUP}" FILES "${FILE}")
    endforeach()
endfunction()

# 添加编译器兼容性检查
include(CheckCXXCompilerFlag)
function(add_compiler_flag_if_supported flag)
    string(REPLACE "-" "_" flag_var ${flag})
    string(REPLACE "=" "_" flag_var ${flag_var})
    string(REPLACE "+" "P" flag_var ${flag_var})
    string(TOUPPER ${flag_var} flag_var)
    
    check_cxx_compiler_flag(${flag} COMPILER_SUPPORTS_${flag_var})
    if(COMPILER_SUPPORTS_${flag_var})
        message(STATUS "Adding compiler flag: ${flag}")
        add_compile_options(${flag})
    else()
        message(STATUS "Compiler does not support: ${flag}")
    endif()
endfunction()

# 在 Windows 上检查 Clang-CL 特定标志
if(PLATFORM_WINDOWS)
    add_compiler_flag_if_supported("/clang:-fdeclspec")  # 确保 declspec 支持
    add_compiler_flag_if_supported("/clang:-fms-compatibility-version=19.30")  # 设置 MSVC 兼容版本
endif()