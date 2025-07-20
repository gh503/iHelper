function(enable_code_signing)
    set(ENABLE_SIGNING ON PARENT_SCOPE)
    
    # 设置默认时间戳服务器
    if(NOT TIMESTAMP_SERVER)
        set(TIMESTAMP_SERVER "http://timestamp.digicert.com" PARENT_SCOPE)
    endif()
    
    # 尝试自动查找证书
    if(NOT CERTIFICATE_PATH)
        set(possible_certs
            "${CMAKE_SOURCE_DIR}/certs/company_cert.pfx"
            "${CMAKE_SOURCE_DIR}/../certs/company_cert.pfx"
            "$ENV{USERPROFILE}/certs/company_cert.pfx"
        )
        
        foreach(cert IN LISTS possible_certs)
            if(EXISTS "${cert}")
                set(CERTIFICATE_PATH "${cert}" PARENT_SCOPE)
                message(STATUS "Found certificate: ${cert}")
                break()
            endif()
        endforeach()
    endif()

    # 从环境变量获取密码
    if(NOT CERTIFICATE_PASSWORD)
        set(CERTIFICATE_PASSWORD $ENV{CERT_PASSWORD})
    endif()
endfunction()

function(sign_file file_path)
    if(NOT ENABLE_SIGNING OR NOT CERTIFICATE_PATH OR NOT CERTIFICATE_PASSWORD)
        return()
    endif()
    
    # 查找 signtool
    find_signtool()
    
    if(NOT SIGNTOOL_PATH)
        message(WARNING "signtool not found. Cannot sign ${file_path}")
        return()
    endif()
    
    message(STATUS "Signing file: ${file_path}")
    
    # 构建签名命令
    set(sign_command 
        "\"${SIGNTOOL_PATH}\" sign"
        "/f \"${CERTIFICATE_PATH}\""
        "/p \"${CERTIFICATE_PASSWORD}\""
        "/fd sha256"
        "/tr \"${TIMESTAMP_SERVER}\""
        "/td sha256"
        "\"${file_path}\""
    )
    
    # 执行签名
    execute_process(
        COMMAND ${sign_command}
        RESULT_VARIABLE sign_result
        OUTPUT_VARIABLE sign_output
        ERROR_VARIABLE sign_error
    )
    
    if(NOT sign_result EQUAL 0)
        message(WARNING "Signing failed for ${file_path}: ${sign_error}")
        return()
    endif()
    
    # 验证签名
    message(STATUS "Verifying signature: ${file_path}")
    execute_process(
        COMMAND "${SIGNTOOL_PATH}" verify /pa /v "${file_path}"
        RESULT_VARIABLE verify_result
        OUTPUT_VARIABLE verify_output
        ERROR_VARIABLE verify_error
    )
    
    if(verify_result EQUAL 0)
        message(STATUS "Signature verified successfully")
    else()
        message(WARNING "Signature verification failed: ${verify_error}")
    endif()
endfunction()

function(find_signtool)
    if(SIGNTOOL_PATH AND EXISTS "${SIGNTOOL_PATH}")
        return()
    endif()
    
    # 查找最新版本的 signtool
    file(GLOB kit_bins "C:/Program Files (x86)/Windows Kits/10/bin/*")
    list(SORT kit_bins)
    list(REVERSE kit_bins) # 从最新版本开始
    
    foreach(kit_bin ${kit_bins})
        if(IS_DIRECTORY "${kit_bin}")
            # 检查 x64 版本
            if(CMAKE_SIZEOF_VOID_P EQUAL 8) # 64位系统
                set(signtool_path "${kit_bin}/x64/signtool.exe")
            else() # 32位系统
                set(signtool_path "${kit_bin}/x86/signtool.exe")
            endif()
            
            if(EXISTS "${signtool_path}")
                set(SIGNTOOL_PATH "${signtool_path}" PARENT_SCOPE)
                message(STATUS "Found signtool: ${signtool_path}")
                return()
            endif()
        endif()
    endforeach()
    
    # 检查 PATH 环境变量
    find_program(SIGNTOOL_EXE signtool)
    if(SIGNTOOL_EXE)
        set(SIGNTOOL_PATH "${SIGNTOOL_EXE}" PARENT_SCOPE)
    else()
        message(WARNING "signtool.exe not found. Code signing disabled.")
    endif()
endfunction()

# 自动签名所有目标
function(auto_sign_all_targets)
    get_property(all_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    
    foreach(target ${all_targets})
        get_target_property(target_type ${target} TYPE)
        if(target_type STREQUAL "SHARED_LIBRARY" OR target_type STREQUAL "MODULE_LIBRARY" OR target_type STREQUAL "EXECUTABLE")
            message(STATUS "Enabling signing for target: ${target}")
            sign_target(${target})
        endif()
    endforeach()
endfunction()