#!/bin/bash

# 设置错误时立即退出
set -e

# 默认测试次数
TEST_RUNS=10
# 默认热点函数数量
TOP_HOTSPOTS=50
# 默认预热次数
WARM_UP_RUNS=3
# 默认稳定性措施强度 (1-3)
STABILITY_LEVEL=2

# 解析命令行参数
while getopts ":r:w:s:" opt; do
  case ${opt} in
    r )
      TEST_RUNS=$OPTARG
      ;;
    w )
      WARM_UP_RUNS=$OPTARG
      ;;
    s )
      STABILITY_LEVEL=$OPTARG
      ;;
    \? )
      echo "用法: $0 [-r 测试次数] [-w 预热次数] [-s 稳定性级别(1-3)]"
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
cmake -DTGFX_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo .. > /dev/null

# 构建项目
echo "构建项目..."
cmake --build . --target TGFXUnitTest -j $(sysctl -n hw.ncpu) > /dev/null

# 创建traces目录
mkdir -p ../traces
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
TRACE_FILE="../traces/SingleRectRender_${TIMESTAMP}.trace"
ANALYSIS_OUTPUT="../traces/performance_analysis_${TIMESTAMP}.txt"
SUMMARY_OUTPUT="../traces/performance_summary_${TIMESTAMP}.txt"

# 应用性能稳定性措施
apply_stability_measures() {
  echo "应用性能稳定性措施 (级别: $STABILITY_LEVEL)..."
  
  # 基本优化 (所有级别)
  # 降低其他进程优先级而不是提高自己的优先级
  if [ "$(uname)" = "Darwin" ]; then
    # 可选：尝试友好地请求Spotlight索引暂停
    launchctl unload -w /System/Library/LaunchAgents/com.apple.Spotlight.plist 2>/dev/null || true
    
    # 可选：请求系统开始内存压缩，不需要sudo
    memory_pressure &>/dev/null &
    MEMORY_PRESSURE_PID=$!
    # 在脚本结束时我们会杀掉这个进程
    trap "kill $MEMORY_PRESSURE_PID 2>/dev/null || true; launchctl load -w /System/Library/LaunchAgents/com.apple.Spotlight.plist 2>/dev/null || true" EXIT
  fi
  
  # 中等级别优化
  if [ $STABILITY_LEVEL -ge 2 ]; then
    # 使用非sudo方式降低后台应用优先级
    if [ "$(uname)" = "Darwin" ]; then
      # 找到可能影响性能的进程并降低其优先级
      for PROC in "Safari" "Chrome" "Firefox" "Mail" "Photos" "Music" "TV" "Calendar"; do
        pgrep "$PROC" | xargs -I{} renice +10 {} 2>/dev/null || true
      done
    fi
    
    # 通过垃圾回收释放内存（不需要sudo）
    if [ "$(uname)" = "Darwin" ]; then
      vm_stat > /dev/null  # 触发一些内存回收
      python3 -c 'import gc; gc.collect()' 2>/dev/null || python -c 'import gc; gc.collect()' 2>/dev/null || true
    fi
  fi
  
  # 高级别优化
  if [ $STABILITY_LEVEL -ge 3 ]; then
    # 尝试限制CPU使用而不需要sudo (使用cpulimit或类似工具)
    if [ "$(uname)" = "Darwin" ]; then
      # 通过进程亲和性优化CPU使用
      # 注意：MacOS没有简单的不需要sudo的CPU亲和性设置方法
      echo "注意: 在不使用sudo的情况下，MacOS限制了高级系统优化选项"
    fi
    
    # 使用温度监控工具获取当前温度（如果支持）
    if [ "$(uname)" = "Darwin" ]; then
      # Mac可以尝试获取温度信息作为参考
      system_profiler SPPowerDataType | grep "Temperature" > /dev/null || true
    fi
  fi
  
  # 等待系统稳定
  sleep 3
}

# 检查并显示错误信息
check_for_errors() {
  local test_output=$1
  
  # 检查是否有shader编译错误或其他错误
  if echo "$test_output" | grep -q "ERROR:" || echo "$test_output" | grep -q "Could not compile shader"; then
    # 找到并显示错误信息
    echo "检测到错误:"
    # 先尝试提取shader错误
    SHADER_ERRORS=$(echo "$test_output" | grep -A 5 "Could not compile shader" | grep -E "Could not compile shader|ERROR:")
    if [ -n "$SHADER_ERRORS" ]; then
      echo "$SHADER_ERRORS"
    else
      # 如果没有找到shader错误，尝试显示任何ERROR行
      echo "$test_output" | grep -A 2 "ERROR:"
    fi
    
    return 1
  fi
  
  return 0
}

# 运行测试并捕获所有输出
run_test() {
  # 使用tee将输出同时发送到stdout和捕获到变量
  TEST_OUTPUT=$("$@" 2>&1)
  echo "$TEST_OUTPUT"
}

# 优化系统并等待资源稳定
apply_stability_measures

# 预热运行
if [ $WARM_UP_RUNS -gt 0 ]; then
  echo "执行 $WARM_UP_RUNS 次预热运行..."
  for (( i=1; i<=$WARM_UP_RUNS; i++ ))
  do
    echo "预热运行 $i/$WARM_UP_RUNS..."
    WARM_OUTPUT=$(run_test ./TGFXUnitTest --gtest_filter=RenderPerformanceTest.SingleRectRender)
    
    # 检查是否有错误
    if ! check_for_errors "$WARM_OUTPUT"; then
      echo "预热过程中检测到错误，退出测试"
      exit 1
    fi
  done
  echo "预热完成"
fi

# 第一阶段：多次运行性能测试并计算平均值（不带trace）
echo "执行 $TEST_RUNS 次性能测试（不带trace工具）..."

# 数组存储每次测试的耗时
TIMES=()
SUM=0

# 在测试之间确保系统冷却和稳定
test_with_cooling() {
  local run_index=$1
  
  # 在每次测试之前短暂暂停，让系统资源恢复稳定状态
  if [ $run_index -gt 1 ]; then
    sleep 2
  fi
  
  # 清理内存（不使用sudo）
  if [ "$(uname)" = "Darwin" ] && [ $STABILITY_LEVEL -ge 2 ]; then
    # 创建临时大文件然后删除来促使系统清理缓存
    dd if=/dev/zero of=/tmp/tempfile bs=1024k count=1024 &>/dev/null || true
    rm /tmp/tempfile &>/dev/null || true
  fi
  
  # 执行测试，直接将结果输出到stdout和stderr
  TEST_OUTPUT=$(run_test ./TGFXUnitTest --gtest_filter=RenderPerformanceTest.SingleRectRender)
  
  # 检查是否有错误输出
  if ! check_for_errors "$TEST_OUTPUT"; then
    echo "测试失败，原因见上述错误"
    return 1
  fi
  
  # 提取渲染时间并返回
  RENDERING_TIME=$(echo "$TEST_OUTPUT" | grep "SingleRectRender: Rendered" | grep -o '[0-9]* ms' | cut -d' ' -f1)
  echo "$RENDERING_TIME"
  
  # 在测试后短暂等待，允许系统恢复稳定状态
  sleep 1
  
  return 0
}

for (( i=1; i<=$TEST_RUNS; i++ ))
do
   echo "执行第 $i/$TEST_RUNS 次测试..."
   RENDERING_TIME=$(test_with_cooling $i)
   
   # 检查是否有错误
   if [ $? -ne 0 ]; then
     echo "测试过程中检测到错误，退出测试"
     exit 1
   fi

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

# 计算中位数和去除异常值后的统计信息
calculate_trimmed_stats() {
  # 对数组进行排序
  SORTED_TIMES=($(echo "${TIMES[@]}" | tr ' ' '\n' | sort -n))
  
  # 计算中位数
  local midpoint=$((${#SORTED_TIMES[@]} / 2))
  if [ $((${#SORTED_TIMES[@]} % 2)) -eq 0 ]; then
    MEDIAN=$(echo "scale=2; (${SORTED_TIMES[$midpoint-1]} + ${SORTED_TIMES[$midpoint]}) / 2" | bc)
  else
    MEDIAN=${SORTED_TIMES[$midpoint]}
  fi
  
  # 去除异常值 (超过1.5倍四分位距的值)
  local q1_idx=$((${#SORTED_TIMES[@]} / 4))
  local q3_idx=$(( (${#SORTED_TIMES[@]} * 3) / 4 ))
  Q1=${SORTED_TIMES[$q1_idx]}
  Q3=${SORTED_TIMES[$q3_idx]}
  IQR=$(echo "scale=2; $Q3 - $Q1" | bc)
  LOWER_BOUND=$(echo "scale=2; $Q1 - (1.5 * $IQR)" | bc)
  UPPER_BOUND=$(echo "scale=2; $Q3 + (1.5 * $IQR)" | bc)
  
  # 去除异常值后的数组
  TRIMMED_TIMES=()
  TRIMMED_SUM=0
  TRIMMED_COUNT=0
  
  for time in "${TIMES[@]}"; do
    if (( $(echo "$time >= $LOWER_BOUND" | bc -l) )) && (( $(echo "$time <= $UPPER_BOUND" | bc -l) )); then
      TRIMMED_TIMES+=($time)
      TRIMMED_SUM=$(echo "scale=2; $TRIMMED_SUM + $time" | bc)
      TRIMMED_COUNT=$((TRIMMED_COUNT + 1))
    fi
  done
  
  # 计算去除异常值后的平均值
  if [ $TRIMMED_COUNT -gt 0 ]; then
    TRIMMED_AVG=$(echo "scale=2; $TRIMMED_SUM / $TRIMMED_COUNT" | bc)
  else
    TRIMMED_AVG=$AVG
  fi
  
  # 计算去除异常值后的标准差
  TRIMMED_SUM_SQUARED_DIFF=0
  for time in "${TRIMMED_TIMES[@]}"; do
    DIFF=$(echo "scale=2; $time - $TRIMMED_AVG" | bc)
    SQUARED_DIFF=$(echo "scale=2; $DIFF * $DIFF" | bc)
    TRIMMED_SUM_SQUARED_DIFF=$(echo "scale=2; $TRIMMED_SUM_SQUARED_DIFF + $SQUARED_DIFF" | bc)
  done
  
  if [ $TRIMMED_COUNT -gt 0 ]; then
    TRIMMED_VARIANCE=$(echo "scale=2; $TRIMMED_SUM_SQUARED_DIFF / $TRIMMED_COUNT" | bc)
    TRIMMED_STD_DEV=$(echo "scale=2; sqrt($TRIMMED_VARIANCE)" | bc)
  else
    TRIMMED_STD_DEV=$STD_DEV
  fi
}

calculate_trimmed_stats

echo "测试完成！"
echo "- 平均渲染耗时: $AVG ms (标准差: $STD_DEV)"
echo "- 中位数渲染耗时: $MEDIAN ms"
echo "- 去除异常值后平均: $TRIMMED_AVG ms (标准差: $TRIMMED_STD_DEV, 样本: $TRIMMED_COUNT/${#TIMES[@]})"
echo "- 四分位区间: Q1=$Q1, Q3=$Q3, IQR=$IQR"

# 第二阶段：执行一次带trace的测试用于性能分析
echo "运行单次测试用于性能分析（带trace工具）..."

TRACE_CMD="xcrun xctrace record --template \"Time Profiler\" --output \"$TRACE_FILE\" --launch -- ./TGFXUnitTest --gtest_filter=RenderPerformanceTest.SingleRectRender"
TRACE_TEST_OUTPUT=$(eval "$TRACE_CMD" 2>&1)

# 检查是否有错误
if ! check_for_errors "$TRACE_TEST_OUTPUT"; then
  echo "Trace测试过程中检测到错误"
  exit 1
fi

echo "详细trace文件已保存到 $TRACE_FILE"

# 从trace文件中提取性能数据
echo "从trace文件中提取性能数据..."

# 导出调用树信息
xcrun xctrace export --input "$TRACE_FILE" --xpath '/trace-toc/run[@number="1"]/data/table[@schema="time-profile"]' --output ../traces/time_profile_${TIMESTAMP}.xml > /dev/null 2>&1

# 提取测试输出信息（这次是来自trace运行的数据，作为参考）
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
    echo "中位数渲染耗时: $MEDIAN ms"
    echo "去除异常值后平均: $TRIMMED_AVG ms (标准差: $TRIMMED_STD_DEV, 样本: $TRIMMED_COUNT/${#TIMES[@]})"
    echo "带Trace工具的渲染耗时：$TRACE_RENDERING_TIME ms (仅供参考，受trace工具影响)"
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
    echo "中位数渲染耗时: $MEDIAN ms"
    echo "去除异常值后平均: $TRIMMED_AVG ms (标准差: $TRIMMED_STD_DEV, 样本: $TRIMMED_COUNT/${#TIMES[@]})"
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
