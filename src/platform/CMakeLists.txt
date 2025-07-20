cmake_minimum_required(VERSION 3.15)
project(CorePlatform LANGUAGES CXX)

# 单元测试
option(BUILD_TESTS "Build unit tests" ON)

# 添加模块路径
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# 包含平台配置
include(PlatformConfig)

# 项目版本
set(CorePlatform_VERSION_MAJOR 1)
set(CorePlatform_VERSION_MINOR 0)
set(CorePlatform_VERSION_PATCH 0)

# 平台特定源文件
if(PLATFORM_WINDOWS)
    set(PLATFORM_SOURCES
        src/Windows/FileSystemWin.cpp
        src/Windows/NetworkWin.cpp
        src/Windows/ProcessWin.cpp
        src/Windows/Registry.cpp
        src/Windows/SystemInfoWin.cpp
        src/Windows/WindowsUtils.cpp
    )
elseif(PLATFORM_MACOS)
    set(PLATFORM_SOURCES
        src/MacOS/FileSystemMac.cpp
        src/MacOS/NetworkMac.cpp
        src/MacOS/ProcessMac.cpp
        src/MacOS/SystemInfoMac.cpp
    )
elseif(PLATFORM_LINUX)
    set(PLATFORM_SOURCES
        src/Linux/FileSystemLinux.cpp
        src/Linux/NetworkLinux.cpp
        src/Linux/ProcessLinux.cpp
        src/Linux/SystemInfoLinux.cpp
    )
endif()

# 源文件
set(SOURCES ${COMMON_SOURCES} ${PLATFORM_SOURCES})

# 头文件
set(PUBLIC_HEADERS
    include/CorePlatform/Export.h
    include/CorePlatform/FileSystem.h
    include/CorePlatform/Network.h
    include/CorePlatform/Process.h
    include/CorePlatform/SystemInfo.h
    include/CorePlatform/Internal/PlatformDetection.h
)
if(WIN32)
    list(APPEND PUBLIC_HEADERS include/CorePlatform/Windows/WindowsUtils.h)
    list(APPEND PUBLIC_HEADERS include/CorePlatform/Windows/Registry.h)
endif()

# 创建静态库
add_library(CorePlatform_static STATIC ${SOURCES})

# 目标包含目录
target_include_directories(CorePlatform_static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# 设置目标属性
set_target_properties(CorePlatform_static PROPERTIES
    VERSION ${CorePlatform_VERSION_MAJOR}.${CorePlatform_VERSION_MINOR}.${CorePlatform_VERSION_PATCH}
    SOVERSION ${CorePlatform_VERSION_MAJOR}
    OUTPUT_NAME CorePlatform
)

# 平台特定配置
if(WIN32)
    # 链接Windows库
    target_link_libraries(CorePlatform_static PRIVATE 
        # FileSystem
        Shlwapi
        # Network
        ws2_32
        iphlpapi
        # SystemInfo
        pdh
        advapi32
    )
    
elseif(APPLE)
    # macOS框架
    find_library(FOUNDATION Foundation REQUIRED)
    find_library(SYSTEMCONFIGURATION SystemConfiguration REQUIRED)
    find_library(IOKIT IOKit REQUIRED)
    target_link_libraries(CorePlatform_static PRIVATE 
        ${FOUNDATION}
        ${SYSTEMCONFIGURATION}
        ${IOKIT}
    )
    
elseif(LINUX)
    # Linux库
    target_link_libraries(CorePlatform_static PRIVATE 
        pthread
        rt
    )
endif()

# 安装规则
install(TARGETS CorePlatform_static EXPORT CorePlatformTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/CorePlatform
)
install(
    DIRECTORY include/CorePlatform/
    DESTINATION include/CorePlatform/
    FILES_MATCHING
    PATTERN "*.h"
    PATTERN "Export.h.in" EXCLUDE
    PATTERN "Windows" EXCLUDE   # 排除已单独安装的子目录
)
if(WIN32)
    install(DIRECTORY include/CorePlatform/Windows 
        DESTINATION include/CorePlatform/
    )
endif()

# 测试
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# 包配置文件生成
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

# 生成配置文件
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CorePlatformConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/CorePlatformConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CorePlatform
    PATH_VARS CMAKE_INSTALL_INCLUDEDIR
)

# 生成版本文件
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/CorePlatformConfigVersion.cmake
    VERSION ${CorePlatform_VERSION_MAJOR}.${CorePlatform_VERSION_MINOR}.${CorePlatform_VERSION_PATCH}
    COMPATIBILITY SameMajorVersion
)

# 安装配置文件
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/CorePlatformConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/CorePlatformConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CorePlatform
)

# 导出目标
install(EXPORT CorePlatformTargets
    FILE CorePlatformTargets.cmake
    NAMESPACE CorePlatform::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CorePlatform
)

# 生成导出头文件（可选）
configure_file(
    include/CorePlatform/Export.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/CorePlatform/Export.h
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/CorePlatform/Export.h
    DESTINATION include/CorePlatform
)