cmake_minimum_required(VERSION 3.15)
project(CorePlatformTests)

# 包含 FetchContent 模块
include(FetchContent)

# 配置 GTest 下载参数
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0  # 指定你需要的版本
  GIT_SHALLOW    TRUE     # 只获取最近提交（加快下载）
)

# 可选：设置 GTest 编译选项
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)  # Windows 下解决 CRT 冲突
option(INSTALL_GTEST "Install Googletest" OFF)
option(BUILD_GMOCK "Build Google Mock" ON)

# 下载并配置 GTest
FetchContent_MakeAvailable(googletest)

# 源文件
if(PLATFORM_WINDOWS)
    set(SOURCES
        test_main.cpp
        FileSystemTest.cpp
        NetworkTest.cpp
        ProcessTest.cpp
        RegistryTest.cpp
        SystemInfoTest.cpp
    )
elseif(PLATFORM_MACOS OR PLATFORM_LINUX)
    set(SOURCES
        test_main.cpp
        FileSystemTest.cpp
        FileSystemTest.cpp
        NetworkTest.cpp
        ProcessTest.cpp
        SystemInfoTest.cpp
    )
endif()

# 头文件
set(PUBLIC_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/CorePlatform/Export.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/CorePlatform/FileSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/CorePlatform/Network.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/CorePlatform/Process.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/CorePlatform/SystemInfo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/CorePlatform/Internal/PlatformDetection.h
)

if(WIN32)
    list(APPEND PUBLIC_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/../include/CorePlatform/Windows/WindowsUtils.h)
    list(APPEND PUBLIC_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/../include/CorePlatform/Windows/Registry.h)
endif()

# 添加测试可执行文件
add_executable(CorePlatformTests
    ${SOURCES}
)

# 目标包含目录
target_include_directories(CorePlatformTests PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>
)

# 链接库
target_link_libraries(CorePlatformTests PRIVATE
    CorePlatform_static
    gtest_main
    gmock
)

# 添加测试
add_test(NAME CorePlatformTests COMMAND CorePlatformTests)