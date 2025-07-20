# 查找 GoogleTest 包
find_package(GTest REQUIRED)

# 创建测试可执行文件
add_executable(comm_system_tests
    test_node.cpp
    test_publisher.cpp
    test_subscriber.cpp
)

# 链接库
target_link_libraries(comm_system_tests
    comm_system
    GTest::gtest_main
)

# 添加测试
add_test(NAME comm_system_tests COMMAND comm_system_tests)