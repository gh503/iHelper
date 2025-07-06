# Core 模块配置
add_library(ihelper_core STATIC)

# 添加源文件
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS *.cpp *.c)
file(GLOB_RECURSE CORE_HEADERS CONFIGURE_DEPENDS *.h *.hpp)

# 添加源文件分组
add_source_group("Core Sources" ${CORE_SOURCES})
add_source_group("Core Headers" ${CORE_HEADERS})

# 设置源文件
target_sources(ihelper_core PRIVATE
    ${CORE_SOURCES}
)

# 包含目录
target_include_directories(ihelper_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 链接库
target_link_libraries(ihelper_core PRIVATE
    ${PLATFORM_LIBS}
)

# 安装规则
install(TARGETS ihelper_core
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# 设置目标属性
set_target_properties(ihelper_core PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)