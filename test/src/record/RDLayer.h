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

#include "tgfx/layers/Layer.h"

namespace tgfx {


struct Command {

  explicit Command() {
  }
  virtual ~Command() = default;

  virtual void execute(std::shared_ptr<Layer>& layer) = 0;
};

struct MakeCommand : Command {

  std::string id;

  MakeCommand(const std::string& uniqueId) : id(uniqueId) {
  }

  std::shared_ptr<tgfx::Layer> layer_ptr;

  void execute(std::shared_ptr<Layer>& layer) override {
    layer_ptr = Layer::Make();
    layer = layer_ptr;
  }
};

struct SetScrollRectCommand : Command {
  Rect rect;

  SetScrollRectCommand(const Rect& r) : rect(r) {
  }

  void execute(std::shared_ptr<Layer>& layer) override {
    if (layer) {
      layer->setScrollRect(rect);
    }
  }
};

struct AddChildCommand : Command {
  std::string childId;

  AddChildCommand(const std::string& inChildId) : childId(inChildId) {
    // ...existing code...
  }

  void execute(std::shared_ptr<Layer>& ) override {

  }
};

class RDLayer {

 public:
  static std::shared_ptr<RDLayer> Replay(const std::vector<std::unique_ptr<Command>>& commands);
  static std::shared_ptr<RDLayer> Make();
  static std::vector<std::unique_ptr<Command>> extractCommands();

  static std::vector<std::unique_ptr<Command>> commands;

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
  std::string id_;

  // 添加获取ID的方法
  const std::string& getId() const;

};
}  // namespace tgfx
