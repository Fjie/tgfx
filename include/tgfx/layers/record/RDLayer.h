///////////////////////////////////////////////////////////////////////////////////////////////
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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, without warranties or conditions of any kind,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
///////////////////////////////////////////////////////////////////////////////////////////////

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

  int id_;

 private:
  std::vector<std::unique_ptr<Command>> commands_;
  std::map<int, std::shared_ptr<RDLayer>> childrenMap_;
};
}  // namespace tgfx
