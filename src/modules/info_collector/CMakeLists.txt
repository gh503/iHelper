# 信息采集模块
add_library(ihelper_info_collector SHARED)

# 添加源文件
file(GLOB SOURCES CONFIGURE_DEPENDS *.cpp *.c)
file(GLOB HEADERS CONFIGURE_DEPENDS *.h *.hpp)

# 添加源文件分组
add_source_group("Sources" ${SOURCES})
add_source_group("Headers" ${HEADERS})

# 设置源文件
target_sources(ihelper_info_collector PRIVATE
    ${SOURCES}
)

# 包含目录
target_include_directories(ihelper_info_collector PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 链接库
target_link_libraries(ihelper_info_collector PRIVATE
    ihelper_core
    ihelper_platform
)

# 安装规则
install(TARGETS ihelper_info_collector
    LIBRARY DESTINATION modules
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
)

# 设置目标属性
set_target_properties(ihelper_info_collector PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
    VERSION ${iHelper_VERSION}
    SOVERSION ${iHelper_VERSION_MAJOR}
)

# Windows 特定设置
if(WIN32)
    set_target_properties(ihelper_info_collector PROPERTIES
        SUFFIX ".dll"
        PREFIX ""
    )
else()
    set_target_properties(ihelper_info_collector PROPERTIES
        SUFFIX ".so"
        PREFIX "lib"
    )
endif()