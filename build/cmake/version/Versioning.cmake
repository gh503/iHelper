# cmake/Versioning.cmake - 版本管理模块

# 函数: setup_version
# 参数:
#   TARGET: 目标名称
#   VERSION_FILE: 版本文件路径
#   [AUTHOR]: 作者名称 (可选)
#   [EMAIL]: 作者邮箱 (可选)
#   [COPYRIGHT]: 版权信息 (可选)
function(setup_version TARGET VERSION_FILE)
    # 解析可选参数
    set(options)
    set(oneValueArgs AUTHOR EMAIL COPYRIGHT)
    set(multiValueArgs)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # 读取或创建版本文件
    if(NOT EXISTS ${VERSION_FILE})
        file(WRITE ${VERSION_FILE} "1.0.0")
        message(STATUS "Created version file: ${VERSION_FILE}")
    endif()
    
    file(STRINGS ${VERSION_FILE} VERSION_STRING)
    string(REPLACE "." ";" VERSION_LIST ${VERSION_STRING})
    list(LENGTH VERSION_LIST VERSION_LENGTH)
    
    if(VERSION_LENGTH GREATER 0)
        list(GET VERSION_LIST 0 VERSION_MAJOR)
    else()
        set(VERSION_MAJOR 1)
    endif()
    
    if(VERSION_LENGTH GREATER 1)
        list(GET VERSION_LIST 1 VERSION_MINOR)
    else()
        set(VERSION_MINOR 0)
    endif()
    
    if(VERSION_LENGTH GREATER 2)
        list(GET VERSION_LIST 2 VERSION_PATCH)
    else()
        set(VERSION_PATCH 0)
    endif()
    
    set(VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
    
    # 设置作者信息
    if(NOT ARG_AUTHOR)
        set(ARG_AUTHOR "Unknown")
    endif()
    
    if(NOT ARG_EMAIL)
        set(ARG_EMAIL "unknown@example.com")
    endif()
    
    if(NOT ARG_COPYRIGHT)
        set(ARG_COPYRIGHT "Copyright (C) ${CMAKE_SYSTEM_NAME} ${PROJECT_NAME}")
    endif()
    
    # 获取Git信息
    find_package(Git)
    if(GIT_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_COMMIT_HASH
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        execute_process(
            COMMAND ${GIT_EXECUTABLE} log -1 --format=%cd --date=short
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_COMMIT_DATE
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
    else()
        set(GIT_COMMIT_HASH "unknown")
        set(GIT_COMMIT_DATE "unknown")
    endif()
    
    # 生成版本头文件
    set(VERSION_HEADER ${CMAKE_CURRENT_BINARY_DIR}/version_${TARGET}.h)
    configure_file(
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/version.h.in
        ${VERSION_HEADER}
    )
    
    # 添加到目标包含目录
    target_include_directories(${TARGET} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    
    # 平台特定处理
    if(WIN32)
        # 生成Windows资源文件
        set(VERSION_RC ${CMAKE_CURRENT_BINARY_DIR}/version_${TARGET}.rc)
        configure_file(
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/version.rc.in
            ${VERSION_RC}
        )
        target_sources(${TARGET} PRIVATE ${VERSION_RC})
        
        # 设置版本资源
        set_target_properties(${TARGET} PROPERTIES
            LINK_FLAGS "/VERSION:${VERSION_MAJOR}.${VERSION_MINOR}"
        )
    elseif(APPLE)
        # 生成macOS Info.plist
        set(INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/Info_${TARGET}.plist)
        configure_file(
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Info.plist.in
            ${INFO_PLIST}
        )
        set_target_properties(${TARGET} PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST ${INFO_PLIST}
        )
    endif()
    
    # 设置CPack变量
    set(CPACK_PACKAGE_VERSION_MAJOR ${VERSION_MAJOR} PARENT_SCOPE)
    set(CPACK_PACKAGE_VERSION_MINOR ${VERSION_MINOR} PARENT_SCOPE)
    set(CPACK_PACKAGE_VERSION_PATCH ${VERSION_PATCH} PARENT_SCOPE)
    set(CPACK_PACKAGE_VENDOR "${ARG_AUTHOR} <${ARG_EMAIL}>" PARENT_SCOPE)
    
    message(STATUS "Configured version for ${TARGET}: ${VERSION}")
endfunction()