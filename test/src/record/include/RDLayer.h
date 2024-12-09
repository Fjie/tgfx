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

#pragma once

#include <map>
#include <string>
#include "tgfx/layers/Layer.h"

namespace tgfx {
struct Command;

class RDLayer {

 public:
  static std::shared_ptr<RDLayer> MakeFrom(const std::string& jsonStr);
  static std::shared_ptr<RDLayer> Make();

  void configureFrom(const std::string& jsonStr);

  std::string serialize();

  ~RDLayer();

  const std::string& name() const {
    return layer_->name();
  }

  void setName(const std::string& value);

  float alpha() const {
    return layer_->alpha();
  }

  void setAlpha(float value);

  Rect scrollRect() const {
    return layer_->scrollRect();
  }

  void setScrollRect(const Rect& rect);

  bool addChild(const std::shared_ptr<RDLayer>& child);

  std::shared_ptr<Layer> layer_;

  // 添加唯一ID成员变量
  int id_;

 private:
  // 添加命令队列作为成员变量
  std::vector<std::unique_ptr<Command>> commands_;
  // 使用有序的map存储子层
  std::map<int, std::shared_ptr<RDLayer>> childrenMap_;
};
}  // namespace tgfx
