#!/bin/bash

# 设置错误时立即退出
set -e

echo "开始运行SingleRectRender测试用例..."

# 当前工作目录
WORKSPACE=$(pwd)

# 创建构建目录
mkdir -p build
cd build

# 使用CMake配置项目，启用测试并启用调试符号
echo "配置项目..."
cmake -DTGFX_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo ..

# 构建项目
echo "构建项目..."
cmake --build . --target TGFXUnitTest -j $(sysctl -n hw.ncpu)

# 创建traces目录
mkdir -p ../traces
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
TRACE_FILE="../traces/SingleRectRender_${TIMESTAMP}.trace"

# 运行特定测试用例并生成性能分析文件
echo "运行SingleRectRender测试用例与性能分析..."

# 使用instruments进行Time Profiler分析
xcrun xctrace record --template "Time Profiler" --output "$TRACE_FILE" --launch -- ./TGFXUnitTest --gtest_filter=RenderPerformanceTest.SingleRectRender --gtest_brief=1

echo "详细trace文件已保存到 $TRACE_FILE"

# 从trace文件中提取性能数据
echo "从trace文件中提取性能数据..."
ANALYSIS_OUTPUT="../traces/performance_analysis_${TIMESTAMP}.txt"

# 导出调用树信息
xcrun xctrace export --input "$TRACE_FILE" --xpath '/trace-toc/run[@number="1"]/data/table[@schema="time-profile"]' --output ../traces/time_profile_${TIMESTAMP}.xml

# 提取性能数据并生成报告
{
  echo "性能分析报告 - 生成时间: $(date)"
  echo "======================================================"
  echo ""
  
  echo "1. 测试程序输出:"
  echo "------------------------------------------------------"
  # 运行测试并捕获输出
  ./TGFXUnitTest --gtest_filter=RenderPerformanceTest.SingleRectRender
  
  echo ""
  echo "2. 性能分析:"
  echo "------------------------------------------------------"
  echo "详细性能分析请使用Instruments打开trace文件: $TRACE_FILE"
  echo ""
  echo "导出的性能数据保存在: ../traces/time_profile_${TIMESTAMP}.xml"
  
  # 使用grep从导出的XML中提取关键信息
  if [ -f "../traces/time_profile_${TIMESTAMP}.xml" ]; then
    echo ""
    echo "3. TGFX相关热点函数 (从XML提取):"
    echo "------------------------------------------------------"
    grep -A 3 "tgfx::" "../traces/time_profile_${TIMESTAMP}.xml" | head -n 50
  fi
  
} > "$ANALYSIS_OUTPUT"

echo "分析完成！性能报告已保存到 $ANALYSIS_OUTPUT"
echo ""
echo "性能报告内容预览:"
echo "================="
head -n 40 "$ANALYSIS_OUTPUT"
echo "..."
echo "(完整报告请查看 $ANALYSIS_OUTPUT)"
echo ""
echo "建议使用Instruments打开 $TRACE_FILE 查看完整的调用栈和性能分析" 