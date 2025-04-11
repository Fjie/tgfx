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

    echo "1. 按模块分类的耗时函数 TOP10："
    echo "------------------------------------------------------"
    
    # 提取和处理tgfx相关函数
    echo "## 核心渲染 (Core Rendering)："
    grep -A 3 "tgfx::Rect\|tgfx::Canvas\|tgfx::Paint\|tgfx::Fill\|tgfx::Shape" "$XML_FILE" | 
      grep -B 3 'name=' | 
      grep 'name=' | 
      sed 's/.*name="\([^"]*\)".*/\1/' | 
      grep "tgfx::" | 
      sort | uniq -c | sort -nr | head -5 | 
      awk '{printf "  • %-60s %5d 次调用\n", $2, $1}'
    
    echo ""
    echo "## GPU操作 (GPU Operations)："
    grep -A 3 "tgfx::GL\|tgfx::Context\|tgfx::Device\|tgfx::Surface\|tgfx::Texture" "$XML_FILE" | 
      grep -B 3 'name=' | 
      grep 'name=' | 
      sed 's/.*name="\([^"]*\)".*/\1/' | 
      grep "tgfx::" | 
      sort | uniq -c | sort -nr | head -5 | 
      awk '{printf "  • %-60s %5d 次调用\n", $2, $1}'
    
    echo ""
    echo "## 内存管理 (Memory Operations)："
    grep -A 3 "allocator\|vector\|shared_ptr" "$XML_FILE" | 
      grep -B 3 'name=' | 
      grep 'name=' | 
      sed 's/.*name="\([^"]*\)".*/\1/' | 
      grep -v "backtrace" | 
      sort | uniq -c | sort -nr | head -5 | 
      awk '{printf "  • %-60s %5d 次调用\n", $2, $1}'

    echo ""
    echo "2. 热点调用栈分析："
    echo "------------------------------------------------------"
    # 提取最热点的调用栈（包含tgfx的函数）
    echo "## 最耗时的TGFX相关调用："
    
    # 使用grep和awk从XML中提取关键调用栈
    grep -A 15 -B 5 '<row>' "$XML_FILE" | 
      grep -A 20 'tgfx::' | 
      grep -B 10 -A 10 'frame.*name' | 
      grep 'name=' | 
      sed 's/.*name="\([^"]*\)".*/\1/' | 
      grep "tgfx::" | 
      sort | uniq -c | sort -nr | head -10 | 
      awk '{printf "  • %-60s %5d 次调用\n", $2, $1}'
    
    echo ""
    echo "3. 性能瓶颈分析："
    echo "------------------------------------------------------"
    echo "根据调用频次分析，主要性能瓶颈可能在于："
    
    # 总结最频繁调用的函数类型
    TOP_FUNCS=$(grep -A 3 "tgfx::" "$XML_FILE" | 
      grep -B 3 'name=' | 
      grep 'name=' | 
      sed 's/.*name="\([^"]*\)".*/\1/' | 
      grep "tgfx::" | 
      sort | uniq -c | sort -nr | head -3 | 
      awk '{print "  • " $2}')
    
    echo "$TOP_FUNCS"
    
    echo ""
    echo "4. 改进建议："
    echo "------------------------------------------------------"
    echo "• 使用Instruments查看完整调用图，定位具体耗时点"
    echo "• 检查GPU操作批处理和合并优化空间"
    echo "• 分析内存分配和管理模式，减少分配次数"
    echo "• 考虑多线程并行处理大量相似渲染操作"
    
    echo ""
    echo "完整性能分析文件："
    echo "• Trace文件: $TRACE_FILE"
    echo "• XML数据: $XML_FILE"
    echo "• 详细分析: $ANALYSIS_OUTPUT"
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

echo "分析完成！简明性能摘要已保存到 $SUMMARY_OUTPUT"
echo ""
echo "性能摘要预览:"
echo "================="
cat "$SUMMARY_OUTPUT"
echo ""
echo "更详细的分析请查看 $ANALYSIS_OUTPUT 或使用Instruments打开 $TRACE_FILE" 