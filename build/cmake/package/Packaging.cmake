# cmake/Packaging.cmake

# 模块化打包函数
# 参数:
#   PROJECT_NAME - 项目名称
#   VENDOR - 供应商名称
#   DESCRIPTION - 项目描述
function(configure_packaging PROJECT_NAME VENDOR DESCRIPTION)
    # 默认关闭打包行为
    if(NOT DEFINED ENABLE_COMPRESSED_PACKAGE)
        set(ENABLE_COMPRESSED_PACKAGE OFF)
    endif()
    
    if(NOT DEFINED ENABLE_INSTALLER_PACKAGE)
        set(ENABLE_INSTALLER_PACKAGE OFF)
    endif()

    # 获取当前时间戳
    string(TIMESTAMP TIMESTAMP "%Y%m%d-%H%M%S")
    
    # 从安装路径中提取目录名
    get_filename_component(INSTALL_DIR_NAME "${CMAKE_INSTALL_PREFIX}" NAME)
    
    # 设置资源目录
    set(PACKAGING_RESOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/packaging_resources")
    
    # 使用项目根目录的 LICENSE 文件
    set(PROJECT_LICENSE_FILE "${CMAKE_SOURCE_DIR}/LICENSE")
    if(NOT EXISTS "${PROJECT_LICENSE_FILE}")
        message(WARNING "Project LICENSE file not found: ${PROJECT_LICENSE_FILE}")
        set(PROJECT_LICENSE_FILE "${PACKAGING_RESOURCE_DIR}/common/license.txt")
    endif()
    
    # 压缩包生成
    if(ENABLE_COMPRESSED_PACKAGE)
        # 确保安装目录存在
        if(NOT EXISTS "${CMAKE_INSTALL_PREFIX}")
            message(WARNING "[${PROJECT_NAME}] Install directory does not exist: ${CMAKE_INSTALL_PREFIX}")
        else()
            # 设置压缩包名称
            set(PACKAGE_NAME "${PROJECT_NAME}-${INSTALL_DIR_NAME}-${PROJECT_VERSION}-${TIMESTAMP}")
            
            # 创建压缩包目标
            add_custom_target(${PROJECT_NAME}_compress
                COMMENT "Creating compressed package for ${PROJECT_NAME}"
                VERBATIM
            )
            
            # 设置输出目录
            set(PACKAGE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/packages" CACHE PATH "Package output directory")
            file(MAKE_DIRECTORY "${PACKAGE_OUTPUT_DIR}")
            
            # 生成ZIP压缩包
            add_custom_command(TARGET ${PROJECT_NAME}_compress
                COMMAND ${CMAKE_COMMAND} -E tar "cfv" "${PACKAGE_OUTPUT_DIR}/${PACKAGE_NAME}.zip" 
                    --format=zip 
                    -- "${CMAKE_INSTALL_PREFIX}/*"
                COMMENT "Creating ZIP package: ${PACKAGE_NAME}.zip"
            )
            
            # 生成TGZ压缩包 (非Windows平台)
            if(NOT WIN32)
                add_custom_command(TARGET ${PROJECT_NAME}_compress
                    COMMAND ${CMAKE_COMMAND} -E tar "cfv" "${PACKAGE_OUTPUT_DIR}/${PACKAGE_NAME}.tar.gz" 
                        --gzip 
                        -- "${CMAKE_INSTALL_PREFIX}/*"
                    COMMENT "Creating TGZ package: ${PACKAGE_NAME}.tar.gz"
                )
            endif()
        endif()
    endif()
    
    # 安装包生成
    if(ENABLE_INSTALLER_PACKAGE)
        # 检查安装目录是否存在
        if(NOT EXISTS "${CMAKE_INSTALL_PREFIX}")
            message(WARNING "[${PROJECT_NAME}] Skipping installer - install directory missing")
            return()
        endif()
        
        # 检查成功构建标志
        set(SUCCESS_FLAG "${CMAKE_INSTALL_PREFIX}/build_success.flag")
        if(NOT EXISTS "${SUCCESS_FLAG}")
            message(WARNING "[${PROJECT_NAME}] Skipping installer - build success flag not found")
            return()
        endif()
        
        # 配置CPack
        include(InstallRequiredSystemLibraries)
        
        # 设置基本包信息
        set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
        set(CPACK_PACKAGE_VENDOR "${VENDOR}")
        set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${DESCRIPTION}")
        set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
        set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
        set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
        set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_LICENSE_FILE}")
        set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${INSTALL_DIR_NAME}-${PROJECT_VERSION}-${TIMESTAMP}")
        
        # 设置安装源目录
        set(CPACK_INSTALLED_DIRECTORIES 
            "${CMAKE_INSTALL_PREFIX}" "."
        )
        
        # 平台特定配置
        if(WIN32)
            set(CPACK_GENERATOR "NSIS;ZIP")
            
            # Windows资源
            set(CPACK_NSIS_MUI_ICON "${PACKAGING_RESOURCE_DIR}/windows/installer.ico")
            set(CPACK_NSIS_MUI_UNIICON "${PACKAGING_RESOURCE_DIR}/windows/uninstaller.ico")
            set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
            set(CPACK_NSIS_PACKAGE_NAME "${PROJECT_NAME} ${PROJECT_VERSION}")
            set(CPACK_NSIS_DISPLAY_NAME "${PROJECT_NAME} Application Suite")
            set(CPACK_NSIS_CONTRIB "${PACKAGING_RESOURCE_DIR}/windows")
            
            # 添加标准菜单项
            set(CPACK_NSIS_MENU_LINKS
                "bin/${PROJECT_NAME}.exe" "${PROJECT_NAME}"
                "bin/unittest" "Unit Tests"
            )
        elseif(APPLE)
            set(CPACK_GENERATOR "DragNDrop;ZIP")
            
            # macOS资源
            set(CPACK_DMG_BACKGROUND_IMAGE "${PACKAGING_RESOURCE_DIR}/macos/background.png")
            set(CPACK_DMG_VOLUME_NAME "${PROJECT_NAME} ${PROJECT_VERSION}")
            set(CPACK_DMG_DS_STORE "${PACKAGING_RESOURCE_DIR}/macos/DS_Store")
        else()
            set(CPACK_GENERATOR "TGZ;DEB")
            
            # Linux资源
            set(CPACK_DEBIAN_PACKAGE_NAME "${PROJECT_NAME}")
            set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${VENDOR}")
            set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "${DESCRIPTION}")
            set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA 
                "${PACKAGING_RESOURCE_DIR}/linux/postinst"
                "${PACKAGING_RESOURCE_DIR}/linux/prerm"
            )
        endif()
        
        # 包含CPack
        include(CPack)
        
        # 添加安装包目标
        add_custom_target(${PROJECT_NAME}_installer
            COMMAND ${CMAKE_CPACK_COMMAND} --config "${CMAKE_BINARY_DIR}/CPackConfig.cmake"
            COMMENT "Building installer package for ${PROJECT_NAME}"
            VERBATIM
        )
    endif()
endfunction()