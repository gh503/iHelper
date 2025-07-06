#!/bin/bash
# 统一构建脚本 (支持 macOS 和 Linux)

# 参数解析
PLATFORM=""
ARCH=""
BUILD_TYPE="Release"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --platform=*)
            PLATFORM="${1#*=}"
            ;;
        --arch=*)
            ARCH="${1#*=}"
            ;;
        --build-type=*)
            BUILD_TYPE="${1#*=}"
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
    shift
done

# 设置默认值
if [[ -z "$PLATFORM" ]]; then
    PLATFORM=$(uname -s | tr '[:upper:]' '[:lower:]')
    # Windows 特殊处理
    if [[ "$PLATFORM" == *"_nt"* ]]; then PLATFORM="windows"; fi
fi

if [[ -z "$ARCH" ]]; then
    ARCH=$(uname -m)
    # 规范化架构名称
    case "$ARCH" in
        x86_64|amd64) 
            if [[ "$PLATFORM" == "macos" ]]; then
                ARCH="universal"
            else
                ARCH="x64"
            fi
            ;;
        arm64|aarch64) 
            if [[ "$PLATFORM" == "macos" ]]; then
                ARCH="universal"
            else
                ARCH="aarch64"
            fi
            ;;
        i[3-6]86) ARCH="x86" ;;
    esac
fi

# macOS 强制使用 universal 架构
if [[ "$PLATFORM" == "macos" ]]; then
    ARCH="universal"
fi

# 验证参数
VALID_PLATFORMS=("macos" "linux")
if [[ ! " ${VALID_PLATFORMS[@]} " =~ " ${PLATFORM} " ]]; then
    echo "Invalid platform: $PLATFORM. Valid: ${VALID_PLATFORMS[*]}"
    exit 1
fi

VALID_ARCHS=("x64" "aarch64" "universal")
if [[ ! " ${VALID_ARCHS[@]} " =~ " ${ARCH} " ]]; then
    echo "Invalid architecture: $ARCH. Valid: ${VALID_ARCHS[*]}"
    exit 1
fi

VALID_BUILD_TYPES=("Debug" "Release" "MinSizeRel" "RelWithDebInfo")
if [[ ! " ${VALID_BUILD_TYPES[@]} " =~ " ${BUILD_TYPE} " ]]; then
    echo "Invalid build type: $BUILD_TYPE. Valid: ${VALID_BUILD_TYPES[*]}"
    exit 1
fi

# 设置预设名称
PRESET="${PLATFORM}-${ARCH}"

# 设置路径
PROJECT_ROOT=$(cd "$(dirname "$0")/../.."; pwd)
BUILD_DIR="${PROJECT_ROOT}/build/build/${PRESET}-${BUILD_TYPE}"
TARGETS_DIR="${PROJECT_ROOT}/targets/${PRESET}-${BUILD_TYPE}"

echo "[iHelper] Building for ${PLATFORM} ${ARCH} ${BUILD_TYPE} with Clang..."
echo "Using preset: ${PRESET}"

# 创建构建目录
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# 配置项目
echo "Configuring project..."
cmake "${PROJECT_ROOT}/src" --preset "${PRESET}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"

# 构建项目
echo "Building project..."
if [[ "$PLATFORM" == "macos" ]]; then
    cmake --build . --config "${BUILD_TYPE}" --parallel $(sysctl -n hw.ncpu)
else
    cmake --build . --config "${BUILD_TYPE}" --parallel $(nproc)
fi

# 安装项目
echo "Installing project..."
cmake --install . --config "${BUILD_TYPE}"

echo "[iHelper] Build completed! Output in ${TARGETS_DIR}"