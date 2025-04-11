#!/bin/bash

# 设置错误时立即退出
set -e

echo "开始运行SingleRectRender测试用例..."

# 当前工作目录
WORKSPACE=$(pwd)

# 创建构建目录
mkdir -p build
cd build

# 使用CMake配置项目，启用测试
echo "配置项目..."
cmake -DTGFX_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release ..

# 构建项目
echo "构建项目..."
cmake --build . --target TGFXUnitTest -j $(sysctl -n hw.ncpu)

# 运行特定测试用例
echo "运行SingleRectRender测试用例..."
./TGFXUnitTest --gtest_filter=RenderPerformanceTest.SingleRectRender 2>/dev/null

echo "测试完成！" 