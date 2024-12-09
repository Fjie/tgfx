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

#include "RDLayer.h"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <unordered_map>

#include "record/Command.h"


using json = nlohmann::json;

namespace tgfx {

// 静态计数器
static int s_idCounter = 0;

// 实现 Command::fromJson 方法
std::unique_ptr<Command> Command::fromJson(const json& j) {
  CommandType type = static_cast<CommandType>(j.at("type").get<int>());
  switch (type) {
    case CommandType::MakeCommand:
      return std::make_unique<MakeCommand>(j.at("id").get<std::string>());
    case CommandType::SetScrollRectCommand: {
      Rect rect = Rect::MakeXYWH(
          j.at("rect").at("x").get<float>(), j.at("rect").at("y").get<float>(),
          j.at("rect").at("width").get<float>(), j.at("rect").at("height").get<float>());
      return std::make_unique<SetScrollRectCommand>(j.at("id").get<std::string>(), rect);
    }
    case CommandType::AddChildCommand:
      return std::make_unique<AddChildCommand>(j.at("parentId").get<std::string>(),
                                               j.at("childId").get<std::string>());
    case CommandType::SetNameCommand:
      return std::make_unique<SetNameCommand>(j.at("id").get<std::string>(),
                                              j.at("name").get<std::string>());
    case CommandType::SetAlphaCommand:
      return std::make_unique<SetAlphaCommand>(j.at("id").get<std::string>(),
                                               j.at("alpha").get<float>());
    default:
      throw std::runtime_error("未知的命令类型");
  }
}

// MakeCommand 的实现
void MakeCommand::execute(
    std::unordered_map<std::string, std::shared_ptr<RDLayer>>& idToRDLayerMap) {
  auto rdLayer = std::make_shared<RDLayer>();
  rdLayer->layer_ = Layer::Make();
  rdLayer->id_ = id;
  idToRDLayerMap[id] = rdLayer;
}

nlohmann::json MakeCommand::toJson() const {
  return {{"type", static_cast<int>(getType())}, {"id", id}};
}

// SetScrollRectCommand 的实现
void SetScrollRectCommand::execute(
    std::unordered_map<std::string, std::shared_ptr<RDLayer>>& idToRDLayerMap) {

  if (auto rd_layer = idToRDLayerMap[this->id]) {
    rd_layer->layer_->setScrollRect(rect);
  }
}

nlohmann::json SetScrollRectCommand::toJson() const {
  return {{"type", static_cast<int>(getType())},
          {"id", id},
          {"rect",
           {{"x", rect.x()}, {"y", rect.y()}, {"width", rect.width()}, {"height", rect.height()}}}};
}

// AddChildCommand 的实现
void AddChildCommand::execute(
    std::unordered_map<std::string, std::shared_ptr<RDLayer>>& idToRDLayerMap) {
  auto rd_layer = idToRDLayerMap[this->id];
  auto childRDLayer = idToRDLayerMap[childId];
  if (rd_layer && childRDLayer) {
    rd_layer->layer_->addChild(childRDLayer->layer_);
  }
}

nlohmann::json AddChildCommand::toJson() const {
  return {{"type", static_cast<int>(getType())}, {"parentId", id}, {"childId", childId}};
}
// SetNameCommand 的实现
void SetNameCommand::execute(
    std::unordered_map<std::string, std::shared_ptr<RDLayer>>& idToRDLayerMap) {
  if (auto rd_layer = idToRDLayerMap[this->id]) {
    rd_layer->layer_->setName(name);
  }
}

nlohmann::json SetNameCommand::toJson() const {
  return {{"type", static_cast<int>(getType())}, {"id", id}, {"name", name}};
}
// SetAlphaCommand 的实现
void SetAlphaCommand::execute(
    std::unordered_map<std::string, std::shared_ptr<RDLayer>>& idToRDLayerMap) {
  if (auto rd_layer = idToRDLayerMap[this->id]) {
    rd_layer->layer_->setAlpha(alpha);
  }
}

nlohmann::json SetAlphaCommand::toJson() const {
  return {{"type", static_cast<int>(getType())}, {"id", id}, {"alpha", alpha}};
}
// RDLayer 的静态方法：Replay（根据 JSON 字符串）
std::shared_ptr<RDLayer> RDLayer::Replay(const std::string& jsonStr) {
  json j = json::parse(jsonStr);
  std::unordered_map<std::string, std::shared_ptr<RDLayer>> idToRDLayerMap;

  // 反序列化并执行 commands
  std::vector<std::unique_ptr<Command>> deserializedCommands;
  for (const auto& cmdJson : j["commands"]) {
    deserializedCommands.emplace_back(Command::fromJson(cmdJson));
  }

  std::shared_ptr<RDLayer> rootRDLayer = nullptr;

  for (const auto& command : deserializedCommands) {
    command->execute(idToRDLayerMap);
    if (!rootRDLayer && command->getType() == CommandType::MakeCommand) {
      rootRDLayer = idToRDLayerMap[command->id];
    }
  }

  // 递归反序列化并添加子层
  for (const auto& childJson : j["children"]) {
    std::string childStr = childJson.dump();
    std::shared_ptr<RDLayer> childLayer = Replay(childStr);
    if (rootRDLayer && childLayer) {
      rootRDLayer->addChild(childLayer);
    }
  }

  return rootRDLayer;
}

// 修改 Make 方法，分配唯一ID并传递给 MakeCommand
std::shared_ptr<RDLayer> RDLayer::Make() {
  auto layer = std::make_shared<RDLayer>();
  layer->id_ = "RDLayer_" + std::to_string(++s_idCounter);
  layer->commands_.emplace_back(std::make_unique<MakeCommand>(layer->id_));
  layer->layer_ = Layer::Make();
  return layer;
}

// 实现 getId 方法
const std::string& RDLayer::getId() const {
  return id_;
}


// SerializeCommands 方法
std::string RDLayer::serializeCommands() {
    json j_object;
    
    // 添加 commands 字段
    j_object["commands"] = json::array();
    for (const auto& cmd : commands_) {
        j_object["commands"].push_back(cmd->toJson());
    }

    // 添加 children 字段，并递归序列化子层
    j_object["children"] = json::array();
    for (const auto& child : children_) {
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
  commands_.emplace_back(std::make_unique<SetNameCommand>(id_, value));
}

void RDLayer::setAlpha(float value) {
  layer_->setAlpha(value);
  commands_.emplace_back(std::make_unique<SetAlphaCommand>(id_, value));
}

void RDLayer::setScrollRect(const Rect& rect) {
  commands_.emplace_back(std::make_unique<SetScrollRectCommand>(id_, rect));
  layer_->setScrollRect(rect);
}

// 修改 addChild 方法，确保传递的是子层的 ID，并存储子层
bool RDLayer::addChild(const std::shared_ptr<RDLayer>& child) {
  layer_->addChild(child->layer_);
  children_.emplace_back(child);
  return true;
}

}  // namespace tgfx
