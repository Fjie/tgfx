#!/bin/bash

# 设置错误时立即退出
set -e

# 默认测试次数
TEST_RUNS=10
# 默认热点函数数量
TOP_HOTSPOTS=50

# 解析命令行参数
while getopts ":r:" opt; do
  case ${opt} in
    r )
      TEST_RUNS=$OPTARG
      ;;
    \? )
      echo "用法: $0 [-r 测试次数]"
      exit 1
      ;;
  esac
done

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
ANALYSIS_OUTPUT="../traces/performance_analysis_${TIMESTAMP}.txt"
SUMMARY_OUTPUT="../traces/performance_summary_${TIMESTAMP}.txt"

# 第一阶段：多次运行性能测试并计算平均值（不带trace）
echo "执行 $TEST_RUNS 次性能测试（不带trace工具）..."

# 数组存储每次测试的耗时
TIMES=()
SUM=0

for (( i=1; i<=$TEST_RUNS; i++ ))
do
   echo "执行第 $i/$TEST_RUNS 次测试..."
   TEST_OUTPUT=$(./TGFXUnitTest --gtest_filter=RenderPerformanceTest.SingleRectRender 2>&1)
   RENDERING_TIME=$(echo "$TEST_OUTPUT" | grep "SingleRectRender: Rendered" | grep -o '[0-9]* ms' | cut -d' ' -f1)

   if [ -z "$RENDERING_TIME" ]; then
     echo "警告：无法获取渲染时间，使用 0"
     RENDERING_TIME=0
   fi

   echo "  - 渲染耗时: $RENDERING_TIME ms"
   TIMES+=($RENDERING_TIME)
   SUM=$((SUM + RENDERING_TIME))
done

# 计算平均值
AVG=$(echo "scale=2; $SUM / $TEST_RUNS" | bc)

# 计算标准差
SUM_SQUARED_DIFF=0
for time in "${TIMES[@]}"; do
  DIFF=$(echo "scale=2; $time - $AVG" | bc)
  SQUARED_DIFF=$(echo "scale=2; $DIFF * $DIFF" | bc)
  SUM_SQUARED_DIFF=$(echo "scale=2; $SUM_SQUARED_DIFF + $SQUARED_DIFF" | bc)
done
VARIANCE=$(echo "scale=2; $SUM_SQUARED_DIFF / $TEST_RUNS" | bc)
STD_DEV=$(echo "scale=2; sqrt($VARIANCE)" | bc)

echo "测试完成！平均渲染耗时: $AVG ms (标准差: $STD_DEV)"

# 第二阶段：执行一次带trace的测试用于性能分析
echo "运行单次测试用于性能分析（带trace工具）..."
xcrun xctrace record --template "Time Profiler" --output "$TRACE_FILE" --launch -- ./TGFXUnitTest --gtest_filter=RenderPerformanceTest.SingleRectRender --gtest_brief=1

echo "详细trace文件已保存到 $TRACE_FILE"

# 从trace文件中提取性能数据
echo "从trace文件中提取性能数据..."

# 导出调用树信息
xcrun xctrace export --input "$TRACE_FILE" --xpath '/trace-toc/run[@number="1"]/data/table[@schema="time-profile"]' --output ../traces/time_profile_${TIMESTAMP}.xml

# 提取测试输出信息（这次是来自trace运行的数据，作为参考）
TRACE_TEST_OUTPUT=$(./TGFXUnitTest --gtest_filter=RenderPerformanceTest.SingleRectRender 2>&1)
TRACE_RENDERING_TIME=$(echo "$TRACE_TEST_OUTPUT" | grep "SingleRectRender: Rendered" | grep -o '[0-9]* ms' | cut -d' ' -f1)

# 使用AWK提取关键性能数据并生成摘要报告
if [ -f "../traces/time_profile_${TIMESTAMP}.xml" ]; then
  XML_FILE="../traces/time_profile_${TIMESTAMP}.xml"

  # 提取性能数据并生成摘要报告
  {
    echo "⚡️ TGFX 性能分析摘要 ⚡️"
    echo "======================================================"
    echo "测试：RenderPerformanceTest.SingleRectRender"
    echo "平均渲染耗时：$AVG ms (标准差: $STD_DEV, $TEST_RUNS 次测试)"
    echo "带Trace工具的渲染耗时：$TRACE_RENDERING_TIME ms (仅供参考，受trace工具影响)"
    echo "时间戳：$(date)"
    echo ""

    echo "测试详情："
    for (( i=0; i<${#TIMES[@]}; i++ )); do
      echo "  - 测试 $((i+1)): ${TIMES[i]} ms"
    done
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
      sort | uniq -c | sort -nr | head -$TOP_HOTSPOTS |
      awk '{printf "  • %-60s %5d 次调用\n", $2, $1}'
  } > "$SUMMARY_OUTPUT"

  # 生成详细分析报告
  {
    echo "性能分析报告 - 生成时间: $(date)"
    echo "======================================================"
    echo ""

    echo "1. 测试程序平均性能:"
    echo "------------------------------------------------------"
    echo "平均渲染耗时: $AVG ms (标准差: $STD_DEV, $TEST_RUNS 次测试)"
    echo "各次测试耗时: ${TIMES[*]} ms"
    echo ""

    echo "2. 测试程序输出 (带trace的运行):"
    echo "------------------------------------------------------"
    echo "$TRACE_TEST_OUTPUT"

    echo ""
    echo "3. 性能分析:"
    echo "------------------------------------------------------"
    echo "详细性能分析请使用Instruments打开trace文件: $TRACE_FILE"
    echo ""
    echo "导出的性能数据保存在: $XML_FILE"

    # 使用grep从导出的XML中提取关键信息
    if [ -f "$XML_FILE" ]; then
      echo ""
      echo "4. TGFX相关热点函数 (从XML提取):"
      echo "------------------------------------------------------"
      grep -A 3 "tgfx::" "$XML_FILE" | head -n 100
    fi

  } > "$ANALYSIS_OUTPUT"
fi

echo ""
cat "$SUMMARY_OUTPUT"
echo ""
echo "详细分析报告已保存到: $ANALYSIS_OUTPUT"
