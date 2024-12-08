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
#include <unordered_map> // 添加此行以使用 unordered_map

namespace tgfx {

// 添加静态计数器
static int s_idCounter = 0;

std::vector<std::unique_ptr<Command>> RDLayer::commands;

// RDLAYER.CPP

void MakeCommand::execute(std::shared_ptr<Layer>& layer,
                          std::unordered_map<std::string, std::shared_ptr<RDLayer>>& idToRDLayerMap) {
  layer = Layer::Make();
  auto rdLayer = std::make_shared<RDLayer>();
  rdLayer->layer_ = layer;
  rdLayer->id_ = id;
  idToRDLayerMap[id] = rdLayer;
}

void AddChildCommand::execute(std::shared_ptr<Layer>& ,
                              std::unordered_map<std::string, std::shared_ptr<RDLayer>>& idToRDLayerMap) {
  auto parentRDLayer = idToRDLayerMap[parentId];
  auto childRDLayer = idToRDLayerMap[childId];
  if (parentRDLayer && childRDLayer) {
    parentRDLayer->layer_->addChild(childRDLayer->layer_);
  }
}

std::shared_ptr<RDLayer> RDLayer::Replay(const std::vector<std::unique_ptr<Command>>& commands) {
  std::shared_ptr<Layer> layer = nullptr;
  std::unordered_map<std::string, std::shared_ptr<RDLayer>> idToRDLayerMap;

  std::shared_ptr<RDLayer> rootRDLayer = nullptr;

  for (const auto& command : commands) {
    command->execute(layer, idToRDLayerMap);
    if (!rootRDLayer && dynamic_cast<MakeCommand*>(command.get())) {
      rootRDLayer = idToRDLayerMap[static_cast<MakeCommand*>(command.get())->id];
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

std::vector<std::unique_ptr<Command>> RDLayer::extractCommands() {
  auto extractedCommands = std::move(commands);
  commands.clear();
  return extractedCommands;
}

RDLayer::~RDLayer() {
  layer_ = nullptr;
}
void RDLayer::setName(const std::string& value) {
  layer_->setName(value);
}
void RDLayer::setAlpha(float value) {
  layer_->setAlpha(value);
}

void RDLayer::setScrollRect(const Rect& rect) {
  commands.emplace_back(std::make_unique<SetScrollRectCommand>(rect));
  layer_->setScrollRect(rect);
}

// 修改 addChild 方法，确保传递的是子层的 ID
bool RDLayer::addChild(const std::shared_ptr<RDLayer>& child) {
  commands.emplace_back(std::make_unique<AddChildCommand>(this->id_, child->getId()));
  layer_->addChild(child->layer_);
  return true;
}

}  // namespace tgfx
