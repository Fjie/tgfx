/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making tgfx available.
//
//  Copyright (C) 2024 THL A29 Limited, a Tencent company. All rights reserved.
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

#include <math.h>
#include <vector>
#include "RDLayer.h"
#include "core/filters/BlurImageFilter.h"
#include "core/utils/Profiling.h"
#include "tgfx/core/PathEffect.h"
#include "tgfx/layers/DisplayList.h"
#include "tgfx/layers/Gradient.h"
#include "tgfx/layers/ImageLayer.h"
#include "tgfx/layers/Layer.h"
#include "tgfx/layers/ShapeLayer.h"
#include "tgfx/layers/SolidLayer.h"
#include "tgfx/layers/TextLayer.h"
#include "tgfx/layers/filters/BlendFilter.h"
#include "tgfx/layers/filters/BlurFilter.h"
#include "tgfx/layers/filters/ColorMatrixFilter.h"
#include "tgfx/layers/filters/DropShadowFilter.h"
#include "tgfx/layers/filters/InnerShadowFilter.h"
#include "utils/TestUtils.h"
#include "utils/common.h"

namespace tgfx {
TGFX_TEST(LayerTest, LayerRecord) {

  auto rdLayer = RDLayer::Make();
  rdLayer->setName("RDLayer");
  rdLayer->setAlpha(0.5f);
  rdLayer->setScrollRect({100, 200, 300, 400});

  auto rdLayer2 = RDLayer::Make();
  rdLayer2->setScrollRect({200, 300, 400, 500});

  rdLayer->addChild(rdLayer2);

  EXPECT_EQ(rdLayer->layer_->alpha(), 0.5f);
  EXPECT_EQ(rdLayer->layer_->name(), "RDLayer");
  EXPECT_EQ(rdLayer->layer_->scrollRect(), Rect::MakeLTRB(100, 200, 300, 400));

  std::string json_str = rdLayer->serializeCommands();
  // 打印json
  std::cout << json_str << std::endl;
  auto replayRDLayer = RDLayer::MakeFrom(json_str);

  EXPECT_EQ(replayRDLayer->layer_->scrollRect(), Rect::MakeLTRB(100, 200, 300, 400));
  EXPECT_EQ(replayRDLayer->layer_->children().size(),
            static_cast<std::vector<Layer>::size_type>(1));

  EXPECT_EQ(replayRDLayer->layer_->children()[0]->scrollRect(), Rect::MakeLTRB(200, 300, 400, 500));

  // 新增验证 name 和 alpha 属性
  EXPECT_EQ(replayRDLayer->layer_->name(), "RDLayer");
  EXPECT_EQ(replayRDLayer->layer_->alpha(), 0.5f);

  // 再次操作 rdLayer
  rdLayer->setAlpha(0.8f);
  rdLayer->setName("UpdatedRDLayer");

  // 再次序列化
  std::string updated_json_str = rdLayer->serializeCommands();
  // 打印更新后的json
  std::cout << updated_json_str << std::endl;

  // 调用 replayRDLayer 的 configFromJson 方法
  replayRDLayer->configFrom(updated_json_str);

  // 验证命令正常还原
  EXPECT_EQ(replayRDLayer->layer_->alpha(), 0.8f);
  EXPECT_EQ(replayRDLayer->layer_->name(), "UpdatedRDLayer");

}

}  // namespace tgfx
