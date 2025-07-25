cmake_minimum_required(VERSION 3.15)
project(iHelperCP LANGUAGES CXX)

# 设置默认构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING 
        "Choose the type of build: Debug, Release, MinSizeRel, or RelWithDebInfo" FORCE)
endif()

# 添加模块路径
list(APPEND CMAKE_MODULE_PATH
    ${CMAKE_SOURCE_DIR}/build/cmake
    ${CMAKE_SOURCE_DIR}/build/cmake/version
)

# 包含平台配置
include(PlatformConfig)
# 包含版本管理模块
include(Versioning)

# 添加子目录
add_subdirectory(src/CorePlatform)
add_subdirectory(src/app/iHelper)

# 打包支持
# ====================== CPack 打包配置 ======================
# include(InstallRequiredSystemLibraries)
set(CPACK_PACKAGE_NAME "iHelper")
set(CPACK_PACKAGE_VENDOR "gh503 angus_robot@163.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "iHelper - Cross-platform Helper Agent")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
# 包文件格式
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}")
# 平台特定打包配置
if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")
    
    # NSIS 配置
    set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/installer.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/uninstaller.ico")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH OFF)
    set(CPACK_NSIS_INSTALLED_ICON_NAME "iHelper.exe")
    set(CPACK_NSIS_MENU_LINKS
        "README.md" "Readme"
        "LICENSE" "License"
    )
    set(CPACK_NSIS_SIGN_COMMAND "signtool sign /a /t http://timestamp.digicert.com /fd sha256")
    
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    
    # DMG 配置
    set(CPACK_DMG_VOLUME_NAME "iHelper")
    set(CPACK_DMG_FORMAT "UDBZ")
    set(CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/assets/mac-background.png")
    set(CPACK_DMG_DS_STORE_SETUP_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/scripts/setup_ds_store.scpt")
    set(CPACK_DMG_SIGN "Developer ID Application: gh503 (TEAMID)")
    
else()
    set(CPACK_GENERATOR "TGZ;DEB")
    
    # Debian 包配置
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "gh503 <angus_robot@163.com>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.14)")
    set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA 
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/postinstall.sh"
    )
endif()

# 添加组件支持
set(CPACK_COMPONENTS_ALL libraries headers docs)

include(CPack)
cpack_add_component(libraries
    DISPLAY_NAME "Core Libraries"
    DESCRIPTION "CorePlatform static libraries"
    REQUIRED
)

cpack_add_component(headers
    DISPLAY_NAME "Header Files"
    DESCRIPTION "C++ header files for development"
    REQUIRED
)

cpack_add_component(docs
    DISPLAY_NAME "Documentation"
    DESCRIPTION "License and readme files"
)
