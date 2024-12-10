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
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
//////////////////////////////////////////////////////////////////////////////////////////////

#include <tgfx/layers/record/RDLayer.h>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include "Command.h"

using json = nlohmann::json;

namespace tgfx {

static int s_idCounter = 0;

std::shared_ptr<RDLayer> RDLayer::MakeFrom(const std::string& jsonStr) {
  std::shared_ptr<RDLayer> layer = Make();
  layer->configureFrom(jsonStr);
  return layer;
}

std::shared_ptr<RDLayer> RDLayer::Make() {
  auto layer = std::make_shared<RDLayer>();
  layer->id_ = ++s_idCounter;
  layer->layer_ = Layer::Make();
  return layer;
}

void RDLayer::configureFrom(const std::string& jsonStr) {

  json j = json::parse(jsonStr);

  for (const auto& cmdJson : j["commands"]) {
    auto command = Command::fromJson(cmdJson);
    command->execute(this);
  }

  for (const auto& childJson : j["children"]) {
    int childId = childJson["id"].get<int>();
    auto it = childrenMap_.find(childId);
    if (it != childrenMap_.end()) {
      it->second->configureFrom(childJson.dump());
    } else {
      auto child = MakeFrom(childJson.dump());
      addChild(child);
    }
  }
}

std::string RDLayer::serialize() {
  json j_object;

  j_object["id"] = id_;

  j_object["commands"] = json::array();
  for (const auto& cmd : commands_) {
    j_object["commands"].push_back(cmd->toJson());
  }
  commands_.clear();

  j_object["children"] = json::array();
  for (const auto& [childId, child] : childrenMap_) {
    j_object["children"].push_back(json::parse(child->serialize()));
  }

  return j_object.dump();
}

RDLayer::~RDLayer() {
  layer_ = nullptr;
}

void RDLayer::setName(const std::string& value) {
  layer_->setName(value);
  commands_.emplace_back(std::make_unique<SetName>(value));
}

void RDLayer::setAlpha(float value) {
  layer_->setAlpha(value);
  commands_.emplace_back(std::make_unique<SetAlpha>(value));
}

void RDLayer::setScrollRect(const Rect& rect) {
  commands_.emplace_back(std::make_unique<SetScrollRect>(rect));
  layer_->setScrollRect(rect);
}

bool RDLayer::addChild(const std::shared_ptr<RDLayer>& child) {
  layer_->addChild(child->layer_);
  childrenMap_[child->id_] = child;
  return true;
}

}  // namespace tgfx
