# Signing.cmake - 跨平台自动签名模块
include_guard()

# 签名函数
function(auto_sign_target)
    set(options DEBUG_MODE)
    set(oneValueArgs TARGET_NAME WIN_CERT_FILE WIN_CERT_PASSWORD_VAR MAC_IDENTITY)
    set(multiValueArgs)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # 全局签名开关检查
    if(DEFINED ENABLE_CODE_SIGNING AND NOT ENABLE_CODE_SIGNING)
        message(STATUS "Code signing disabled globally for ${ARG_TARGET_NAME}")
        return()
    endif()

    if(NOT ARG_TARGET_NAME)
        message(FATAL_ERROR "auto_sign_target: TARGET_NAME is required")
    endif()

    # 验证目标存在
    if(NOT TARGET ${ARG_TARGET_NAME})
        message(FATAL_ERROR "Signing target not found: ${ARG_TARGET_NAME}")
    endif()

    # 调试模式警告
    if(ARG_DEBUG_MODE)
        message(WARNING "******************************************************")
        message(WARNING "DEBUG SIGNING MODE ENABLED - USING UNSECURE DEBUG CERTS")
        message(WARNING "THIS IS NOT SUITABLE FOR PRODUCTION USE!")
        message(WARNING "******************************************************")
    endif()

    # Windows 签名
    if(WIN32)
        if(NOT SIGNTOOL_EXE)
            # 可能的根路径
            set(WINDOWS_KITS_ROOTS
                "C:/Program Files (x86)/Windows Kits/10/bin"
                "C:/Program Files/Windows Kits/10/bin"
            )
            
            # 尝试查找最新版本的路径
            foreach(kit_root ${WINDOWS_KITS_ROOTS})
                if(IS_DIRECTORY "${kit_root}")
                    # 获取所有版本子目录
                    file(GLOB version_dirs LIST_DIRECTORIES true "${kit_root}/*")
                    
                    # 按版本号排序（最新的在前）
                    list(SORT version_dirs)
                    list(REVERSE version_dirs)
                    
                    # 尝试每个版本目录
                    foreach(version_dir ${version_dirs})
                        # 检查是否是有效的版本目录
                        get_filename_component(version_name "${version_dir}" NAME)
                        if(version_name MATCHES "^[0-9]")
                            # 尝试每个平台
                            foreach(platform x64 x86 arm arm64)
                                set(test_path "${version_dir}/${platform}/signtool.exe")
                                if(EXISTS "${test_path}")
                                    set(SIGNTOOL_EXE "${test_path}")
                                    break() # 跳出平台循环
                                endif()
                            endforeach()
                            
                            if(SIGNTOOL_EXE)
                                break() # 跳出版本循环
                            endif()
                        endif()
                    endforeach()
                    
                    if(SIGNTOOL_EXE)
                        break() # 跳出根目录循环
                    endif()
                endif()
            endforeach()
        endif()

        # 查找 signtool
        find_program(SIGNTOOL_EXE signtool
            PATHS
            "C:/Program Files (x86)/Windows Kits/10/bin"
            "C:/Program Files/Windows Kits/10/bin"
            PATH_SUFFIXES x64 x86 arm arm64
        )
        
        if(NOT SIGNTOOL_EXE)
            message(FATAL_ERROR "signtool not found - install Windows SDK")
        endif()

        # 调试模式处理
        if(ARG_DEBUG_MODE)
            # 确保证书路径存在
            if(NOT ARG_WIN_CERT_FILE)
                set(ARG_WIN_CERT_FILE "${CMAKE_CURRENT_LIST_DIR}/debug_cert.pfx")
            else()
                # 确保证书目录存在
                get_filename_component(CERT_DIR "${ARG_WIN_CERT_FILE}" DIRECTORY)
                if(NOT EXISTS "${CERT_DIR}")
                    file(MAKE_DIRECTORY "${CERT_DIR}")
                endif()
            endif()

            # 证书
            if(NOT EXISTS "${ARG_WIN_CERT_FILE}")
                message(FATAL_ERROR "Failed to find debug certificate: ${ARG_WIN_CERT_FILE}")
            endif()

            # 固定调试密码
            set(DEBUG_CERT_PASSWORD "debug_sign_passwd")

            # 添加签名命令
            add_custom_command(TARGET ${ARG_TARGET_NAME} POST_BUILD
                COMMAND ${SIGNTOOL_EXE} sign
                    /f "${ARG_WIN_CERT_FILE}"
                    /p "${DEBUG_CERT_PASSWORD}"
                    /fd SHA256
                    /tr http://timestamp.digicert.com
                    /td SHA256
                    "$<TARGET_FILE:${ARG_TARGET_NAME}>"
                COMMENT "Signing ${ARG_TARGET_NAME} with debug certificate"
                VERBATIM
            )
            return()
        endif()

        # 生产模式处理
        # 检查证书文件
        if(NOT ARG_WIN_CERT_FILE)
            message(FATAL_ERROR "Windows signing requires WIN_CERT_FILE for ${ARG_TARGET_NAME}")
        endif()
        
        if(NOT EXISTS "${ARG_WIN_CERT_FILE}")
            message(FATAL_ERROR "Certificate file not found: ${ARG_WIN_CERT_FILE}")
        endif()

        # 获取密码
        if(NOT ARG_WIN_CERT_PASSWORD_VAR)
            message(FATAL_ERROR "Windows signing requires WIN_CERT_PASSWORD_VAR for ${ARG_TARGET_NAME}")
        endif()
        
        set(CERT_PASSWORD $ENV{${ARG_WIN_CERT_PASSWORD_VAR}})
        if("${CERT_PASSWORD}" STREQUAL "")
            message(FATAL_ERROR "Certificate password not found in environment variable: ${ARG_WIN_CERT_PASSWORD_VAR}")
        endif()

        # 添加签名命令
        add_custom_command(TARGET ${ARG_TARGET_NAME} POST_BUILD
            COMMAND ${SIGNTOOL_EXE} sign
                /f "${ARG_WIN_CERT_FILE}"
                /p "${CERT_PASSWORD}"
                /fd SHA256
                /tr http://timestamp.digicert.com
                /td SHA256
                "$<TARGET_FILE:${ARG_TARGET_NAME}>"
            COMMENT "Signing ${ARG_TARGET_NAME} for Windows (production)"
            VERBATIM
        )

    # macOS 签名
    elseif(APPLE)
        find_program(CODESIGN_EXE codesign)
        if(NOT CODESIGN_EXE)
            message(FATAL_ERROR "codesign not found - install Xcode command line tools")
        endif()

        # 调试模式处理
        if(ARG_DEBUG_MODE)
            add_custom_command(TARGET ${ARG_TARGET_NAME} POST_BUILD
                COMMAND ${CODESIGN_EXE} --force -s - "$<TARGET_FILE:${ARG_TARGET_NAME}>"
                COMMENT "Ad-hoc signing ${ARG_TARGET_NAME}"
                VERBATIM
            )
            return()
        endif()

        # 生产模式处理
        if(NOT ARG_MAC_IDENTITY)
            message(FATAL_ERROR "macOS production signing requires MAC_IDENTITY for ${ARG_TARGET_NAME}")
        endif()

        # 添加签名命令
        add_custom_command(TARGET ${ARG_TARGET_NAME} POST_BUILD
            COMMAND ${CODESIGN_EXE} --force --sign "${ARG_MAC_IDENTITY}" "$<TARGET_FILE:${ARG_TARGET_NAME}>"
            COMMAND ${CODESIGN_EXE} -v "$<TARGET_FILE:${ARG_TARGET_NAME}>"
            COMMENT "Signing and verifying ${ARG_TARGET_NAME} for macOS"
            VERBATIM
        )

    # Linux 不需要签名
    else()
        message(STATUS "Skipping signing for Linux: ${ARG_TARGET_NAME}")
    endif()
endfunction()