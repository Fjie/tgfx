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
#include "tgfx/core/Canvas.h"
#include "tgfx/core/Clock.h"
#include "tgfx/core/Recorder.h"
#include "tgfx/core/Surface.h"
#include "utils/TestUtils.h"

namespace tgfx {

TGFX_TEST(RenderPerformanceTest, SingleRectRender) {
  ContextScope scope;
  auto context = scope.getContext();
  ASSERT_TRUE(context != nullptr);

  // 创建一个2048x2048的画布
  int width = 2048;
  int height = 2048;
  auto surface = Surface::Make(context, width, height);
  auto canvas = surface->getCanvas();
  canvas->clear(Color::White());

  // 创建随机数生成器，使用固定种子以便比较结果
  std::srand(12345);

  const int rectCount = 5 * 10000;
  std::vector<Rect> rects;
  std::vector<Paint> paints;

  // 生成随机矩形和颜色
  const float rectWidth = 50.0f;   // 固定矩形宽度
  const float rectHeight = 50.0f;  // 固定矩形高度

  for (int i = 0; i < rectCount; ++i) {
    float x = static_cast<float>(std::rand() % width);
    float y = static_cast<float>(std::rand() % height);

    rects.push_back(Rect::MakeXYWH(x, y, rectWidth, rectHeight));

    float r = static_cast<float>(std::rand() % 255) / 255.0f;
    float g = static_cast<float>(std::rand() % 255) / 255.0f;
    float b = static_cast<float>(std::rand() % 255) / 255.0f;

    Paint paint;
    paint.setColor(Color{r, g, b, 1.0f});  // 完全不透明
    paints.push_back(paint);
  }

  // 记录渲染开始时间
  auto startTime = Clock::Now();

  // 绘制所有矩形
  for (size_t i = 0; i < static_cast<size_t>(rectCount); ++i) {
    const float radius = rects[i].width() * 0.25f;
    canvas->drawRoundRect(rects[i], radius, radius, paints[i]);
  }

  // 完成渲染并测量时间
  context->flush();
  auto endTime = Clock::Now();
  auto elapsedTime = (endTime - startTime) / 1000;  // 转换为毫秒

  std::cout << "SingleRectRender: Rendered " << rectCount << " rectangles in " << elapsedTime
            << " ms" << std::endl;

  // 保存结果，方便查看
  Baseline::Compare(surface, "RenderPerformanceTest/SingleRectRender");
}

}  // namespace tgfx
