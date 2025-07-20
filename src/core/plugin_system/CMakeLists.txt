set(LIBNAME "PluginSystem")
set(BINNAME "plugin_system")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 输出目录
set(PLUGIN_OUTPUT_DIR ${CMAKE_BINARY_DIR}/plugins)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)

# 包含目录
include_directories(include)

# 构建静态库
add_library(${LIBNAME} STATIC
    src/PluginManager.cpp
    src/PluginSystemAPI.cpp
)

# 平台特定的链接
if(WIN32)
    target_link_libraries(${LIBNAME} PRIVATE kernel32.lib)
else()
    target_link_libraries(${LIBNAME} PRIVATE dl pthread)
endif()

# 主程序
add_executable(${BINNAME} main.cpp)
target_link_libraries(${BINNAME} PRIVATE ${LIBNAME})

# 包含插件目录
add_subdirectory(plugins/ExamplePlugin)