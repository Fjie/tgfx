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
SUMMARY_OUTPUT="../traces/performance_summary_${TIMESTAMP}.txt"

# 导出调用树信息
xcrun xctrace export --input "$TRACE_FILE" --xpath '/trace-toc/run[@number="1"]/data/table[@schema="time-profile"]' --output ../traces/time_profile_${TIMESTAMP}.xml

# 提取测试输出信息
TEST_OUTPUT=$(./TGFXUnitTest --gtest_filter=RenderPerformanceTest.SingleRectRender 2>&1)
RENDERING_TIME=$(echo "$TEST_OUTPUT" | grep "SingleRectRender: Rendered" | grep -o '[0-9]* ms' | cut -d' ' -f1)

# 使用AWK提取关键性能数据并生成摘要报告
if [ -f "../traces/time_profile_${TIMESTAMP}.xml" ]; then
  XML_FILE="../traces/time_profile_${TIMESTAMP}.xml"

  # 提取性能数据并生成摘要报告
  {
    echo "⚡️ TGFX 性能分析摘要 ⚡️"
    echo "======================================================"
    echo "测试：RenderPerformanceTest.SingleRectRender"
    echo "渲染耗时：$RENDERING_TIME ms (渲染1,000,000个矩形)"
    echo "时间戳：$(date)"
    echo ""

    echo "热点调用栈分析："
    echo "------------------------------------------------------"
    # 提取最热点的调用栈（包含tgfx的函数）
    # 使用grep和awk从XML中提取关键调用栈
    grep -A 15 -B 5 '<row>' "$XML_FILE" |
      grep -A 20 'tgfx::' |
      grep -B 10 -A 10 'frame.*name' |
      grep 'name=' |
      sed 's/.*name="\([^"]*\)".*/\1/' |
      grep "tgfx::" |
      sort | uniq -c | sort -nr | head -10 |
      awk '{printf "  • %-60s %5d 次调用\n", $2, $1}'
  } > "$SUMMARY_OUTPUT"

  # 生成详细分析报告（保留原有详细信息）
  {
    echo "性能分析报告 - 生成时间: $(date)"
    echo "======================================================"
    echo ""

    echo "1. 测试程序输出:"
    echo "------------------------------------------------------"
    echo "$TEST_OUTPUT"

    echo ""
    echo "2. 性能分析:"
    echo "------------------------------------------------------"
    echo "详细性能分析请使用Instruments打开trace文件: $TRACE_FILE"
    echo ""
    echo "导出的性能数据保存在: $XML_FILE"

    # 使用grep从导出的XML中提取关键信息
    if [ -f "$XML_FILE" ]; then
      echo ""
      echo "3. TGFX相关热点函数 (从XML提取):"
      echo "------------------------------------------------------"
      grep -A 3 "tgfx::" "$XML_FILE" | head -n 100
    fi

  } > "$ANALYSIS_OUTPUT"
fi

echo ""
cat "$SUMMARY_OUTPUT"
echo ""
