# Platform 模块配置
add_library(ihelper_platform OBJECT)

# 添加公共源文件
file(GLOB COMMON_SOURCES CONFIGURE_DEPENDS common/*.cpp common/*.c)

# 平台特定源文件
if(PLATFORM_WINDOWS)
    file(GLOB PLATFORM_SOURCES CONFIGURE_DEPENDS windows/*.cpp windows/*.c)
    message(STATUS "Building Windows platform layer")
    
elseif(PLATFORM_MACOS)
    file(GLOB PLATFORM_SOURCES CONFIGURE_DEPENDS macos/*.cpp macos/*.c)
    message(STATUS "Building macOS platform layer")
    
elseif(PLATFORM_LINUX)
    file(GLOB PLATFORM_SOURCES CONFIGURE_DEPENDS linux/*.cpp linux/*.c)
    message(STATUS "Building Linux platform layer")
    
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

# 添加源文件分组
add_source_group("Platform Sources" ${COMMON_SOURCES} ${PLATFORM_SOURCES})
add_source_group("Platform Headers" ${PLATFORM_HEADERS})

# 设置源文件
target_sources(ihelper_platform PRIVATE
    ${COMMON_SOURCES}
    ${PLATFORM_SOURCES}
)

# 包含目录
target_include_directories(ihelper_platform PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 链接库
target_link_libraries(ihelper_platform PRIVATE
    ihelper_core
    ${PLATFORM_LIBS}
)

# 安装规则
install(TARGETS ihelper_platform
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# 设置目标属性
set_target_properties(ihelper_platform PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)