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

#include <unordered_map>  // 添加此行以使用 unordered_map
#include "tgfx/layers/Layer.h"

namespace tgfx {
class RDLayer;

struct Command {
  std::string id;  // 新增成员变量记录父类 ID

  explicit Command() {
  }
  virtual ~Command() = default;

  // 修改 execute 方法，添加 idToRDLayerMap 参数
  virtual void execute(

      std::unordered_map<std::string, std::shared_ptr<RDLayer>>& idToRDLayerMap) = 0;
};

// 修改 MakeCommand 的 execute 方法签名
struct MakeCommand : Command {

  MakeCommand(const std::string& uniqueId) {
    this->id = uniqueId;
  }

  void execute(std::unordered_map<std::string, std::shared_ptr<RDLayer>>& idToRDLayerMap) override;
};

struct SetScrollRectCommand : Command {
  Rect rect;

  SetScrollRectCommand(const std::string& uniqueId, const Rect& r) : rect(r) {
    this->id = uniqueId;
  }

  void execute(std::unordered_map<std::string, std::shared_ptr<RDLayer>>&) override;
};

// 修改 AddChildCommand，添加 parentId
struct AddChildCommand : Command {
  std::string childId;

  AddChildCommand(const std::string& inParentId, const std::string& inChildId)
      : childId(inChildId) {
    this->id = inParentId;
  }

  void execute(std::unordered_map<std::string, std::shared_ptr<RDLayer>>& idToRDLayerMap) override;
};

struct SetNameCommand : Command {
  std::string name;

  SetNameCommand(const std::string& uniqueId, const std::string& n) : name(n) {
    this->id = uniqueId;
  }

  void execute(std::unordered_map<std::string, std::shared_ptr<RDLayer>>& idToRDLayerMap) override;
};

struct SetAlphaCommand : Command {
  float alpha;

  SetAlphaCommand(const std::string& uniqueId, float a) : alpha(a) {
    this->id = uniqueId;
  }

  void execute(std::unordered_map<std::string, std::shared_ptr<RDLayer>>& idToRDLayerMap) override;
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
