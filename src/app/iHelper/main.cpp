#include <iostream>
#include "version_iHelper.h" // 自动生成的头文件

int main(const int argc, const char* argv[])
{
    std::cout << "hello iHelper" << std::endl;
    std::cout << "====================================\n";
    std::cout << " " << iHelper_NAME << " v" << iHelper_VERSION_STRING << "\n";
    std::cout << " Build: " << iHelper_BUILD_DATE 
              << " (" << iHelper_GIT_COMMIT_HASH << ")\n";
    std::cout << " " << iHelper_COPYRIGHT << "\n";
    std::cout << "====================================\n\n";
    return 0;
}