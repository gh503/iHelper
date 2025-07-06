cmake_minimum_required(VERSION 3.15)
project(iHelper LANGUAGES CXX)

# 设置项目版本
set(iHelper_VERSION_MAJOR 1)
set(iHelper_VERSION_MINOR 0)
set(iHelper_VERSION_PATCH 0)
set(iHelper_VERSION ${iHelper_VERSION_MAJOR}.${iHelper_VERSION_MINOR}.${iHelper_VERSION_PATCH})

# 设置默认构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING 
        "Choose the type of build: Debug, Release, MinSizeRel, or RelWithDebInfo" FORCE)
endif()

# 添加模块路径
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/../build/cmake)

# 包含平台配置
include(PlatformConfig)

# 添加子目录
add_subdirectory(core)
add_subdirectory(platform)
add_subdirectory(modules/info_collector)

# 添加主应用程序
add_executable(ihelper main.cpp)

# 链接依赖库
target_link_libraries(ihelper PRIVATE
    ihelper_core
    ihelper_platform
    ihelper_info_collector
)

# 安装目标
install(TARGETS ihelper
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION bundle
)

# 安装配置文件
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/config
    DESTINATION etc
    FILES_MATCHING PATTERN "*.conf"
)

# 单元测试支持
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()