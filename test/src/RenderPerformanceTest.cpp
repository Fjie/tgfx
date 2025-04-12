/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making tgfx available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
//
//  Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
//  in compliance with the License. You may obtain a copy of the License at
//
//      https://opensource.org/licenses/BSD-3-Clause
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <ctime>
#include <fstream>
#include <nlohmann/json.hpp>
#include "tgfx/core/Canvas.h"
#include "tgfx/core/Clock.h"
#include "tgfx/core/Recorder.h"
#include "tgfx/core/Surface.h"
#include "utils/TestUtils.h"

using json = nlohmann::json;

namespace tgfx {

// 用于存储矩形和颜色信息的结构
struct RectData {
  float x;
  float y;
  float width;
  float height;
  float radius;
  float r;
  float g;
  float b;
  float a;
};

// 生成性能测试所需的随机数据并保存到JSON文件
TGFX_TEST(RenderPerformanceTest, GenerateTestData) {
  // 创建随机数生成器，使用固定种子以便比较结果
  std::srand(12345);

  // 画布尺寸和矩形数量
  int width = 2048;
  int height = 2048;
  const int rectCount = 10 * 10000;
  const float rectWidth = 50.0f;   // 固定矩形宽度
  const float rectHeight = 50.0f;  // 固定矩形高度

  json testData;
  testData["width"] = width;
  testData["height"] = height;
  testData["rectCount"] = rectCount;

  json rectangles = json::array();

  // 生成随机矩形和颜色
  for (int i = 0; i < rectCount; ++i) {
    float x = static_cast<float>(std::rand() % width);
    float y = static_cast<float>(std::rand() % height);
    float radius = rectWidth * 0.25f;

    float r = static_cast<float>(std::rand() % 255) / 255.0f;
    float g = static_cast<float>(std::rand() % 255) / 255.0f;
    float b = static_cast<float>(std::rand() % 255) / 255.0f;

    json rect;
    rect["x"] = x;
    rect["y"] = y;
    rect["width"] = rectWidth;
    rect["height"] = rectHeight;
    rect["radius"] = radius;
    rect["r"] = r;
    rect["g"] = g;
    rect["b"] = b;
    rect["a"] = 1.0f;

    rectangles.push_back(rect);
  }

  testData["rectangles"] = rectangles;

  // 保存到JSON文件
  std::string filePath = ProjectPath::Absolute("./test/test_data.json");
  std::ofstream outputFile(filePath);
  if (outputFile.is_open()) {
    outputFile << testData.dump(2);
    outputFile.close();
    std::cout << "Generated test data saved to: " << filePath << std::endl;
  } else {
    std::cerr << "Failed to open file for writing: " << filePath << std::endl;
  }
}

// 从JSON文件加载测试数据并执行渲染性能测试
TGFX_TEST(RenderPerformanceTest, SingleRectRender) {
  ContextScope scope;
  auto context = scope.getContext();
  ASSERT_TRUE(context != nullptr);

  // 从JSON文件加载测试数据
  std::string filePath = ProjectPath::Absolute("./test/test_data.json");
  std::ifstream inputFile(filePath);
  ASSERT_TRUE(inputFile.is_open()) << "Failed to open test data file: " << filePath;

  json testData = json::parse(inputFile);
  inputFile.close();

  int width = testData["width"];
  int height = testData["height"];
  int rectCount = testData["rectCount"];

  auto surface = Surface::Make(context, width, height);
  auto canvas = surface->getCanvas();
  canvas->clear(Color::White());

  std::vector<Rect> rects;
  std::vector<Paint> paints;
  std::vector<float> radii;

  // 从JSON解析矩形和颜色数据
  for (const auto& rectJson : testData["rectangles"]) {
    float x = rectJson["x"];
    float y = rectJson["y"];
    float rectWidth = rectJson["width"];
    float rectHeight = rectJson["height"];
    float radius = rectJson["radius"];

    rects.push_back(Rect::MakeXYWH(x, y, rectWidth, rectHeight));
    radii.push_back(radius);

    float r = rectJson["r"];
    float g = rectJson["g"];
    float b = rectJson["b"];
    float a = rectJson["a"];

    Paint paint;
    paint.setColor(Color{r, g, b, a});
    paints.push_back(paint);
  }

  // 记录渲染开始时间
  auto startTime = Clock::Now();

  // 绘制所有矩形
  for (size_t i = 0; i < rects.size(); ++i) {
    canvas->drawRoundRect(rects[i], radii[i], radii[i], paints[i]);
  }

  // 完成渲染并测量时间
  context->flush();
  auto endTime = Clock::Now();
  auto elapsedTime = (endTime - startTime) / 1000;  // 转换为毫秒

  std::cout << "SingleRectRender: Rendered " << rectCount << " rectangles in " << elapsedTime
            << " ms" << std::endl;

  EXPECT_TRUE(Baseline::Compare(surface, "RenderPerformanceTest/SingleRectRender"));


}

}  // namespace tgfx
