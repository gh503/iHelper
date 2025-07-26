#include "CorePlatform/GTestLog.h"

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    testing::TestEventListeners& listeners = 
        testing::UnitTest::GetInstance()->listeners();
    
    // 移除默认输出
    delete listeners.Release(listeners.default_result_printer());

    // 打开日志文件
    std::ofstream log_file("test_log.txt");
    if (!log_file.is_open()) {
        std::cerr << "Failed to open log file. Logging to console only." << std::endl;
    }
    TimestampedListener combined_listener(&std::cout, &log_file);
    listeners.Append(&combined_listener);

    return RUN_ALL_TESTS();
}