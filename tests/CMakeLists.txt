# 仅在启用测试时包含
if(BUILD_TESTING)
    # 查找 GoogleTest
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.12.1
    )
    FetchContent_MakeAvailable(googletest)
    
    # 添加测试可执行文件
    add_executable(ihelper_tests
        # 列出测试源文件
        core_tests.cpp
        platform_tests.cpp
        info_collector_tests.cpp
    )
    
    # 包含目录
    target_include_directories(ihelper_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/core/include
        ${CMAKE_SOURCE_DIR}/platform/include
        ${CMAKE_SOURCE_DIR}/modules/info_collector/include
    )
    
    # 链接库
    target_link_libraries(ihelper_tests PRIVATE
        ihelper_core
        ihelper_platform
        ihelper_info_collector
        GTest::gtest_main
    )
    
    # 添加测试
    include(GoogleTest)
    gtest_discover_tests(ihelper_tests)
    
    # 安装规则
    install(TARGETS ihelper_tests
        RUNTIME DESTINATION tests
    )
endif()