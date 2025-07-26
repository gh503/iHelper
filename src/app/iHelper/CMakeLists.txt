cmake_minimum_required(VERSION 3.15)
set(BIN iHelper)
project(${BIN} LANGUAGES CXX)

# 添加主应用程序
add_executable(${BIN} ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

# 设置版本信息
set(iHelper_VERSION_FILE ${CMAKE_CURRENT_SOURCE_DIR}/VERSION)
setup_version(${BIN} ${iHelper_VERSION_FILE}
    AUTHOR "gh503"
    EMAIL "angus_robot@163.com"
    COPYRIGHT "Copyright (C) 2024-2025 gh503"
)
# 启用 UAC 提权
enable_uac_elevation(${BIN}
    LEVEL "requireAdministrator"
)

# 链接依赖库
target_link_libraries(${BIN} PRIVATE
    CorePlatform_static
)

# 安装目标
install(TARGETS ${BIN}
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION bundle
)

# 安装 PDB 文件
if(WIN32)
    install(FILES $<TARGET_PDB_FILE:${BIN}>
        DESTINATION bin
        CONFIGURATIONS RelWithDebInfo Debug
        OPTIONAL
    )
endif()

# 安装配置文件
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/config
    DESTINATION etc
    FILES_MATCHING PATTERN "*.conf"
)