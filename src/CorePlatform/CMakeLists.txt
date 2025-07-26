cmake_minimum_required(VERSION 3.15)
project(CorePlatform LANGUAGES CXX)

# 单元测试
option(BUILD_TESTS "Build unit tests" ON)
# 设置测试后自动运行（可选）
option(AUTO_RUN_TESTS "Automatically run tests after build" ON)

# 添加模块路径
list(APPEND CMAKE_MODULE_PATH
    ${CMAKE_SOURCE_DIR}/build/cmake
    ${CMAKE_SOURCE_DIR}/build/cmake/version
)

# 包含平台配置
include(PlatformConfig)
include(Versioning)

# 平台特定源文件
if(PLATFORM_WINDOWS)
    set(PLATFORM_SOURCES
        src/Windows/FileSystemWin.cpp
        src/Windows/NetworkWin.cpp
        src/Windows/ProcessWin.cpp
        src/Windows/Registry.cpp
        src/Windows/SystemInfoWin.cpp
        src/Windows/ThreadWin.cpp
        src/Windows/HostOperationsWin.cpp
        src/Windows/WindowsUtils.cpp
    )
elseif(PLATFORM_MACOS)
    set(PLATFORM_SOURCES
        src/MacOS/FileSystemMac.cpp
        src/MacOS/NetworkMac.cpp
        src/MacOS/ProcessMac.cpp
        src/MacOS/SystemInfoMac.cpp
        src/MacOS/HostOperationsMac.cpp
        src/MacOS/ThreadMac.cpp
    )
elseif(PLATFORM_LINUX)
    set(PLATFORM_SOURCES
        src/Linux/FileSystemLinux.cpp
        src/Linux/NetworkLinux.cpp
        src/Linux/ProcessLinux.cpp
        src/Linux/SystemInfoLinux.cpp
        src/Linux/HostOperationsLinux.cpp
        src/Linux/ThreadLinux.cpp
    )
endif()

set(COMMON_SOURCES
    src/JsonUtils.cpp
    src/Logger.cpp
    src/StringUtils.cpp
    src/TimeUtils.cpp
)

# 源文件
set(SOURCES ${COMMON_SOURCES} ${PLATFORM_SOURCES})

# 头文件
set(PUBLIC_HEADERS
    include/CorePlatform/Export.h
    include/CorePlatform/FileSystem.h
    include/CorePlatform/HostOperations.h
    include/CorePlatform/JsonUtils.h
    include/CorePlatform/Logger.h
    include/CorePlatform/StringUtils.h
    include/CorePlatform/Network.h
    include/CorePlatform/Process.h
    include/CorePlatform/SystemInfo.h
    include/CorePlatform/Thread.h
    include/CorePlatform/TimeUtils.h
    include/CorePlatform/Internal/PlatformDetection.h
)
if(WIN32)
    list(APPEND PUBLIC_HEADERS include/CorePlatform/Windows/WindowsUtils.h)
    list(APPEND PUBLIC_HEADERS include/CorePlatform/Windows/Registry.h)
endif()

# ================ 创建静态库 ================
add_library(CorePlatform_static STATIC ${SOURCES})

# 目标包含目录
target_include_directories(CorePlatform_static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# 设置目标属性
target_compile_definitions(CorePlatform_static PRIVATE CORE_PLATFORM_STATIC)
set_target_properties(CorePlatform_static PROPERTIES
    OUTPUT_NAME "CorePlatform"
    DEBUG_OUTPUT_NAME "CorePlatform"
    ARCHIVE_OUTPUT_NAME "CorePlatform"  # 明确指定静态库名称
    # POSITION_INDEPENDENT_CODE ON     # 确保静态库可用于链接到动态库
)

# ================ 创建共享库 ================
add_library(CorePlatform_shared SHARED ${SOURCES})

# 设置版本信息
set(CP_VERSION_FILE ${CMAKE_CURRENT_SOURCE_DIR}/VERSION)
setup_version(CorePlatform_shared ${CP_VERSION_FILE}
    AUTHOR "gh503"
    EMAIL "angus_robot@163.com"
    COPYRIGHT "Copyright (C) 2024-2025 gh503"
)

# 目标包含目录
target_include_directories(CorePlatform_shared PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# 设置目标属性
target_compile_definitions(CorePlatform_shared PRIVATE CORE_PLATFORM_BUILD_SHARED)
target_compile_definitions(CorePlatform_shared PUBLIC CORE_PLATFORM_USE_SHARED)
set_target_properties(CorePlatform_shared PROPERTIES
    PREFIX "lib"
    DEBUG_POSTFIX "D"
    OUTPUT_NAME "CorePlatform"
    ARCHIVE_OUTPUT_NAME "CorePlatform_import"  # 禁止生成导入库的 .lib 文件
    RUNTIME_OUTPUT_NAME "CorePlatform"  # 设置 DLL 名称
    LIBRARY_OUTPUT_NAME "CorePlatform"  # 设置 Unix 共享库名称
)

# ================ 平台特定配置 ================
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
    target_link_libraries(CorePlatform_shared PRIVATE 
        Shlwapi
        ws2_32
        iphlpapi
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
    target_link_libraries(CorePlatform_shared PRIVATE 
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
    target_link_libraries(CorePlatform_shared PRIVATE 
        pthread
        rt
    )

endif()

# 测试
if(BUILD_TESTS)
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

# 安装规则
install(TARGETS CorePlatform_static CorePlatform_shared EXPORT CorePlatformTargets
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
    PATTERN "Windows" EXCLUDE   # 排除已单独安装的子目录
)
if(WIN32)
    install(DIRECTORY include/CorePlatform/Windows 
        DESTINATION include/CorePlatform/
    )
endif()

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