cmake_minimum_required(VERSION 3.15)

set(PROJECT_NAME iHelperCP)
# 读取 VERSION 文件
if(EXISTS "${CMAKE_SOURCE_DIR}/VERSION")
    file(STRINGS "${CMAKE_SOURCE_DIR}/VERSION" PROJECT_VERSION)
    string(STRIP "${PROJECT_VERSION}" PROJECT_VERSION)
else()
    set(PROJECT_VERSION "1.0.0")
    message(WARNING "VERSION file not found, using default version: ${PROJECT_VERSION}")
endif()

project(${PROJECT_NAME} VERSION ${PROJECT_VERSION} LANGUAGES CXX)

# 全局签名开关
option(ENABLE_CODE_SIGNING "Enable code signing" ON)

# 调试签名开关
option(ENABLE_DEBUG_SIGNING "Enable debug signing mode" ON)

# 打包控制选项
option(ENABLE_COMPRESSED_PACKAGE "Enable compressed package creation" OFF)

# 构建安装包控制选项
option(ENABLE_INSTALLER_PACKAGE "Enable installer package creation" OFF)

# 设置签名证书信息
if(WIN32)
    # 调试模式使用特殊证书路径，生产模式使用正式证书
    if(ENABLE_DEBUG_SIGNING)
        set(SIGN_CERT_FILE "${CMAKE_BINARY_DIR}/debug_cert.pfx")
    else()
        set(SIGN_CERT_FILE "C:/certs/production_cert.pfx")
        set(SIGN_CERT_PASSWORD_VAR "PROD_CERT_PASSWORD")
    endif()
elseif(APPLE)
    set(SIGN_IDENTITY "Developer ID Application: gh503 (ABCD1234EF)")
endif()

# 设置全局签名参数
set(GLOBAL_SIGN_PARAMS)
if(ENABLE_CODE_SIGNING)
    # Windows 签名参数
    if(WIN32)
        if(ENABLE_DEBUG_SIGNING)
            set(GLOBAL_SIGN_CERT_FILE "${CMAKE_SOURCE_DIR}/build/cmake/sign/debug_cert.pfx")
        else()
            set(GLOBAL_SIGN_CERT_FILE "C:/certs/production_cert.pfx")
            set(GLOBAL_SIGN_CERT_PASSWORD_VAR "MY_APP_CERT_PASSWORD")
        endif()
    endif()
    
    # macOS 签名参数
    if(APPLE)
        if(ENABLE_DEBUG_SIGNING)
            set(GLOBAL_MAC_IDENTITY "ad-hoc")
        else()
            set(GLOBAL_MAC_IDENTITY "Developer ID Application: My Company (ABCD1234EF)")
        endif()
    endif()
    
    # 将参数保存为全局变量
    set(GLOBAL_SIGN_PARAMS
        WIN_CERT_FILE ${GLOBAL_SIGN_CERT_FILE}
        WIN_CERT_PASSWORD_VAR ${GLOBAL_SIGN_CERT_PASSWORD_VAR}
        MAC_IDENTITY ${GLOBAL_MAC_IDENTITY}
        DEBUG_MODE ${ENABLE_DEBUG_SIGNING}
    )
endif()

# 设置默认构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING 
        "Choose the type of build: Debug, Release, MinSizeRel, or RelWithDebInfo" FORCE)
endif()

# 添加模块路径
list(APPEND CMAKE_MODULE_PATH
    ${CMAKE_SOURCE_DIR}/build/cmake
    ${CMAKE_SOURCE_DIR}/build/cmake/version
    ${CMAKE_SOURCE_DIR}/build/cmake/UAC
    ${CMAKE_SOURCE_DIR}/build/cmake/sign
    ${CMAKE_SOURCE_DIR}/build/cmake/package
)

# 包含平台配置
include(PlatformConfig)
# 包含版本管理
include(Versioning)
# windows提权
include(UacElevation)
# 签名
include(Signing)
# 打包
include(Packaging)

# 添加子目录
add_subdirectory(src/CorePlatform)
add_subdirectory(src/app/iHelper)

# 安装项目关键文件（统一安装）
install(
    FILES 
        ${CMAKE_SOURCE_DIR}/VERSION
        ${CMAKE_SOURCE_DIR}/LICENSE
    DESTINATION .
)

# 安装文档目录（如果存在）
if(EXISTS "${CMAKE_SOURCE_DIR}/docs")
    install(
        DIRECTORY ${CMAKE_SOURCE_DIR}/docs/
        DESTINATION docs
        PATTERN ".gitkeep" EXCLUDE
        PATTERN ".DS_Store" EXCLUDE
    )
else()
    message(WARNING "docs directory not found: ${CMAKE_SOURCE_DIR}/docs")
endif()

# 打包配置
configure_packaging(
    "${PROJECT_NAME}"
    "gh503"
    "iHelper Application Suite"
)

# 安装后步骤 - 创建成功标志
install(CODE [[
    # 获取时间戳
    if(WIN32)
        execute_process(
            COMMAND cmd /c "echo %DATE% %TIME%"
            OUTPUT_VARIABLE TIMESTAMP
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    else()
        execute_process(
            COMMAND date "+%Y-%m-%d %H:%M:%S"
            OUTPUT_VARIABLE TIMESTAMP
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    endif()
    
    # 获取主机名
    if(WIN32)
        set(HOSTNAME "$ENV{COMPUTERNAME}")
    else()
        # Linux 和 macOS
        execute_process(
            COMMAND hostname
            OUTPUT_VARIABLE HOSTNAME
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    endif()
    
    # 创建构建成功标志
    file(WRITE "${CMAKE_INSTALL_PREFIX}/build_success.flag" 
        "Project: ${PROJECT_NAME}\n"
        "Version: ${PROJECT_VERSION}\n"
        "Timestamp: ${TIMESTAMP}\n"
        "Host: ${HOSTNAME}\n"
        "Platform: ${PLATFORM_NAME}\n"
        "Build Type: ${CMAKE_BUILD_TYPE}\n"
    )
    
    # 验证关键文件
    set(REQUIRED_FILES 
        "VERSION"
        "LICENSE"
        "build_success.flag"
    )
    
    foreach(FILE IN LISTS REQUIRED_FILES)
        if(NOT EXISTS "${CMAKE_INSTALL_PREFIX}/${FILE}")
            message(WARNING "Required file missing: ${CMAKE_INSTALL_PREFIX}/${FILE}")
        endif()
    endforeach()
]])