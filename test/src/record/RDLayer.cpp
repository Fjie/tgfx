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

#include "include/RDLayer.h"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include "record/Command.h"

using json = nlohmann::json;

namespace tgfx {

// 静态计数器
static int s_idCounter = 0;

// RDLayer 的静态方法：Replay（根据 JSON 字符串）
std::shared_ptr<RDLayer> RDLayer::MakeFrom(const std::string& jsonStr) {
  std::shared_ptr<RDLayer> rootRDLayer = Make();
  rootRDLayer->configFrom(jsonStr);
  return rootRDLayer;
}

// 修改 Make 方法，分配唯一ID并传递给 MakeCommand
std::shared_ptr<RDLayer> RDLayer::Make() {
  auto layer = std::make_shared<RDLayer>();
  layer->id_ = "RDLayer_" + std::to_string(++s_idCounter);
  layer->layer_ = Layer::Make();
  return layer;
}

void RDLayer::configFrom(const std::string& jsonStr) {

  json j = json::parse(jsonStr);

  // 反序列化并执行 commands
  for (const auto& cmdJson : j["commands"]) {
    auto command = Command::fromJson(cmdJson);
    command->execute(this);
  }
  // 递归反序列化并添加子层
  for (const auto& childJson : j["children"]) {
    std::string childId = childJson["id"].get<std::string>();
    auto it = childrenMap_.find(childId);
    if (it != childrenMap_.end()) {
      it->second->configFrom(childJson.dump());
    } else {
      auto child = MakeFrom(childJson.dump());
      addChild(child);
    }
  }
}

// 实现 getId 方法
const std::string& RDLayer::getId() const {
  return id_;
}

// SerializeCommands 方法
std::string RDLayer::serializeCommands() {
  json j_object;

  // 添加自己的 id
  j_object["id"] = id_;

  // 添加 commands 字段
  j_object["commands"] = json::array();
  for (const auto& cmd : commands_) {
    j_object["commands"].push_back(cmd->toJson());
  }
  // 命令要清空
  commands_.clear();

  j_object["children"] = json::array();
  for (const auto& [childId, child] : childrenMap_) {
    j_object["children"].push_back(json::parse(child->serializeCommands()));
  }

  return j_object.dump();
}

// RDLayer 的析构函数
RDLayer::~RDLayer() {
  layer_ = nullptr;
}

void RDLayer::setName(const std::string& value) {
  layer_->setName(value);
  commands_.emplace_back(std::make_unique<SetNameCommand>(value)); // 移除 id 参数
}

void RDLayer::setAlpha(float value) {
  layer_->setAlpha(value);
  commands_.emplace_back(std::make_unique<SetAlphaCommand>(value)); // 移除 id 参数
}

void RDLayer::setScrollRect(const Rect& rect) {
  commands_.emplace_back(std::make_unique<SetScrollRectCommand>(rect)); // 移除 id 参数
  layer_->setScrollRect(rect);
}

// 修改 addChild 方法，使用有序的map存储子层
bool RDLayer::addChild(const std::shared_ptr<RDLayer>& child) {
  layer_->addChild(child->layer_);
  // 使用 childrenMap_ 进行管理
  childrenMap_[child->getId()] = child;
  return true;
}

}  // namespace tgfx
