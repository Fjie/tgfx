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

// 初始化静态成员
std::vector<std::unique_ptr<Command>> commands;

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
  // 从 jsonStr 中反序列化出 Command 列表，并执行
  json j = json::parse(jsonStr);
  std::vector<std::unique_ptr<Command>> deserializedCommands;

  for (const auto& cmdJson : j) {
    deserializedCommands.emplace_back(Command::fromJson(cmdJson));
  }

  std::unordered_map<std::string, std::shared_ptr<RDLayer>> idToRDLayerMap;

  std::shared_ptr<RDLayer> rootRDLayer = nullptr;

  for (const auto& command : deserializedCommands) {
    command->execute(idToRDLayerMap);
    if (!rootRDLayer && command->getType() == CommandType::MakeCommand) {
      rootRDLayer = idToRDLayerMap[command->id];
    }
  }

  return rootRDLayer;
}

// 修改 Make 方法，分配唯一ID并传递给 MakeCommand
std::shared_ptr<RDLayer> RDLayer::Make() {
  auto layer = std::make_shared<RDLayer>();
  layer->id_ = "RDLayer_" + std::to_string(++s_idCounter);
  commands.emplace_back(std::make_unique<MakeCommand>(layer->id_));
  layer->layer_ = Layer::Make();
  return layer;
}

// 实现 getId 方法
const std::string& RDLayer::getId() const {
  return id_;
}


// SerializeCommands 方法
std::string RDLayer::SerializeCommands() {
  auto commandsToSerialize = std::move(commands);
  commands.clear();
  json j_commands = json::array();
  for (const auto& cmd : commandsToSerialize) {
    j_commands.push_back(cmd->toJson());
  }
  // 将 commands 序列化成 JSON 字符串，返回
  return j_commands.dump();
}

// RDLayer 的析构函数
RDLayer::~RDLayer() {
  layer_ = nullptr;
}
void RDLayer::setName(const std::string& value) {
  layer_->setName(value);
  commands.emplace_back(std::make_unique<SetNameCommand>(id_, value));
}

void RDLayer::setAlpha(float value) {
  layer_->setAlpha(value);
  commands.emplace_back(std::make_unique<SetAlphaCommand>(id_, value));
}

void RDLayer::setScrollRect(const Rect& rect) {
  commands.emplace_back(std::make_unique<SetScrollRectCommand>(id_, rect));
  layer_->setScrollRect(rect);
}

// 修改 addChild 方法，确保传递的是子层的 ID
bool RDLayer::addChild(const std::shared_ptr<RDLayer>& child) {
  commands.emplace_back(std::make_unique<AddChildCommand>(this->id_, child->getId()));
  layer_->addChild(child->layer_);
  return true;
}

}  // namespace tgfx
