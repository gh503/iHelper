project(ExamplePlugin)

# 创建共享库
add_library(${PROJECT_NAME} SHARED ExamplePlugin.cpp)

# 设置平台特定的扩展名
if(APPLE)
    set(PLUGIN_EXT ".dylib")
elseif(UNIX)
    set(PLUGIN_EXT ".so")
elseif(WIN32)
    set(PLUGIN_EXT ".dll")
endif()

# 设置插件属性
set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX ""
    OUTPUT_NAME "${PROJECT_NAME}"
    LIBRARY_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
    SUFFIX "${PLUGIN_EXT}"
)

# 添加导出定义（仅Windows）
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE 
        -D_EXPORTING
    )
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# 确保在 Windows 上使用 UTF-8 编码
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE "/utf-8")
endif()