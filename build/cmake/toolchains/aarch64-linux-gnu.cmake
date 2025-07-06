# AArch64 Linux 交叉编译工具链
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# 工具链路径
set(TOOLCHAIN_PREFIX /usr/bin/aarch64-linux-gnu)
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)

# 目标环境配置
set(CMAKE_FIND_ROOT_PATH /usr/aarch64-linux-gnu)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# 系统库路径
set(CMAKE_SYSROOT /usr/aarch64-linux-gnu)

# 使用 Clang 作为编译器
set(CMAKE_C_COMPILER clang CACHE STRING "C compiler")
set(CMAKE_CXX_COMPILER clang++ CACHE STRING "C++ compiler")