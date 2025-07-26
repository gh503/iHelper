cmake_minimum_required(VERSION 3.15)
project(CorePlatformTests)

# 启用测试
enable_testing()

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
    set(PLATFORM_SOURCES
        RegistryTest.cpp
    )
elseif(PLATFORM_MACOS OR PLATFORM_LINUX)
    set(PLATFORM_SOURCES
    )
endif()
set(COMMON_SOURCES
    test_main.cpp
    FileSystemTest.cpp
    JsonUtilsTest.cpp
    NetworkTest.cpp
    ProcessTest.cpp
    SystemInfoTest.cpp
    LoggerTest.cpp
    StringUtilsTest.cpp
    TimeUtilsTest.cpp
)

# 添加测试可执行文件
add_executable(CorePlatformTests
    ${COMMON_SOURCES}
    ${PLATFORM_SOURCES}
)

# 设置版本信息
set(CPTests_VERSION_FILE ${CMAKE_CURRENT_SOURCE_DIR}/VERSION)
setup_version(CorePlatformTests ${CPTests_VERSION_FILE}
    AUTHOR "gh503"
    EMAIL "angus_robot@163.com"
    COPYRIGHT "Copyright (C) 2024-2025 gh503"
)

# 目标包含目录
target_include_directories(CorePlatformTests PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>
)

# 链接库
target_link_libraries(CorePlatformTests PRIVATE
    CorePlatform_static
    GTest::gtest
    GTest::gmock
)

# 确保静态库先构建
add_dependencies(CorePlatformTests CorePlatform_static)

# 安装测试可执行文件（可选）
install(TARGETS CorePlatformTests
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/unittest
)