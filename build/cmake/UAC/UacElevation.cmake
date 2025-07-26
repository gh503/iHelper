# UacElevation.cmake - 模块化 UAC 提权设置
# 用法: enable_uac_elevation(<target> [LEVEL <level>] [ICON <shield_icon>])
#
# 参数:
#   target - 要设置提权的目标
#   LEVEL - UAC 执行级别（requireAdministrator, highestAvailable, asInvoker）
#   ICON - 可选的盾牌图标文件路径

function(enable_uac_elevation TARGET)
    # 解析参数
    set(options)
    set(oneValueArgs LEVEL ICON)
    set(multiValueArgs)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    if(NOT WIN32)
        message(WARNING "UAC elevation is only supported on Windows")
        return()
    endif()
    
    # 设置默认权限级别
    if(NOT ARG_LEVEL)
        set(ARG_LEVEL "requireAdministrator")
    endif()
    
    # 创建清单文件内容
    set(MANIFEST_CONTENT 
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">\n"
        "  <trustInfo xmlns=\"urn:schemas-microsoft-com:asm.v3\">\n"
        "    <security>\n"
        "      <requestedPrivileges>\n"
        "        <requestedExecutionLevel level=\"${ARG_LEVEL}\" uiAccess=\"false\"/>\n"
        "      </requestedPrivileges>\n"
        "    </security>\n"
        "  </trustInfo>\n"
        "  <compatibility xmlns=\"urn:schemas-microsoft-com:compatibility.v1\">\n"
        "    <application>\n"
        "      <!-- Windows 11 -->\n"
        "      <supportedOS Id=\"{e2011457-1546-43c5-a5fe-008deee3d3f0}\"/>\n"
        "      <!-- Windows 10 -->\n"
        "      <supportedOS Id=\"{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}\"/>\n"
        "      <!-- Windows 8.1 -->\n"
        "      <supportedOS Id=\"{1f676c76-80e1-4239-95bb-83d0f6d0da78}\"/>\n"
        "      <!-- Windows 8 -->\n"
        "      <supportedOS Id=\"{4a2f28e3-53b9-4441-ba9c-d69d4a4a6e38}\"/>\n"
        "      <!-- Windows 7 -->\n"
        "      <supportedOS Id=\"{35138b9a-5d96-4fbd-8e2d-a2440225f93a}\"/>\n"
        "      <!-- Windows Vista -->\n"
        "      <supportedOS Id=\"{e2011457-1546-43c5-a5fe-008deee3d3f0}\"/>\n"
        "    </application>\n"
        "  </compatibility>\n"
        "</assembly>"
    )
    
    # 创建清单文件
    set(MANIFEST_FILE "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.manifest")
    file(WRITE ${MANIFEST_FILE} ${MANIFEST_CONTENT})
    
    # 创建资源文件
    set(RC_FILE "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_uac.rc")
    
    # 预设盾牌图标位置
    set(DEFAULT_SHIELD_ICON "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/shield.ico")
    
    # 确定使用的图标
    if(ARG_ICON)
        set(SHIELD_ICON "${ARG_ICON}")
    elseif(EXISTS "${DEFAULT_SHIELD_ICON}")
        set(SHIELD_ICON "${DEFAULT_SHIELD_ICON}")
    else()
        set(SHIELD_ICON "")
    endif()
    
    # 生成资源文件
    file(WRITE ${RC_FILE} 
        "#include <winuser.h>\n\n"
        "CREATEPROCESS_MANIFEST_RESOURCE_ID RT_MANIFEST \"${MANIFEST_FILE}\"\n"
    )
    
    # 添加盾牌图标
    if(SHIELD_ICON AND EXISTS "${SHIELD_ICON}")
        file(APPEND ${RC_FILE} "\nIDI_SHIELD ICON \"${SHIELD_ICON}\"\n")
    endif()
    
    # 添加资源文件到目标
    target_sources(${TARGET} PRIVATE ${RC_FILE})
    set_source_files_properties(${RC_FILE} PROPERTIES LANGUAGE RC)
    
    # 添加清单文件依赖
    set_source_files_properties(${MANIFEST_FILE} 
        PROPERTIES 
        HEADER_FILE_ONLY TRUE
        GENERATED TRUE
    )
    
    # MSVC 特定设置
    if(MSVC)
        set_target_properties(${TARGET} PROPERTIES
            LINK_FLAGS "/MANIFEST:NO"
        )
    endif()
    
    message(STATUS "Enabled UAC elevation for target: ${TARGET} (level=${ARG_LEVEL})")
endfunction()