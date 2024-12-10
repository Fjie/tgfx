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

#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include "tgfx/layers/Layer.h"

namespace tgfx {

enum class CommandType { SetScrollRectCommand, SetNameCommand, SetAlphaCommand };

class RDLayer;

struct Command {
  explicit Command() {
  }
  virtual ~Command() = default;

  virtual CommandType getType() const = 0;
  virtual nlohmann::json toJson() const = 0;
  virtual void execute(RDLayer* rdLayer) = 0;

  static std::unique_ptr<Command> fromJson(const nlohmann::json& j);
};

struct SetScrollRect : Command {
  Rect rect;

  SetScrollRect(const Rect& r) : rect(r) {
  }

  CommandType getType() const override {
    return CommandType::SetScrollRectCommand;
  }

  nlohmann::json toJson() const override;
  void execute(RDLayer* rdLayer) override;
};

struct SetName : Command {
  std::string name;

  SetName(const std::string& n) : name(n) {
  }

  CommandType getType() const override {
    return CommandType::SetNameCommand;
  }

  nlohmann::json toJson() const override;
  void execute(RDLayer* rdLayer) override;
};

struct SetAlpha : Command {
  float alpha;

  SetAlpha(float a) : alpha(a) {
  }

  CommandType getType() const override {
    return CommandType::SetAlphaCommand;
  }

  nlohmann::json toJson() const override;
  void execute(RDLayer* rdLayer) override;
};

}  // namespace tgfx