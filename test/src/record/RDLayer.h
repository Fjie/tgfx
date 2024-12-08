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

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "tgfx/layers/Layer.h"
#include <nlohmann/json.hpp>

namespace tgfx {
// 定义 CommandType 枚举
enum class CommandType {
  MakeCommand,
  SetScrollRectCommand,
  AddChildCommand,
  SetNameCommand,
  SetAlphaCommand
};

class RDLayer;

struct Command {
  std::string id;  // 记录父类 ID

  explicit Command() {
  }
  virtual ~Command() = default;

    // 获取命令类型
    virtual CommandType getType() const = 0;

    // 序列化命令为 JSON
    virtual nlohmann::json toJson() const = 0;

    // 执行命令
    virtual void execute(std::unordered_map<std::string, std::shared_ptr<RDLayer>>& idToRDLayerMap) = 0;

    // 反序列化命令
    static std::unique_ptr<Command> fromJson(const nlohmann::json& j);
};

// MakeCommand 的定义
struct MakeCommand : Command {
    explicit MakeCommand(const std::string& uniqueId) {
        this->id = uniqueId;
    }

    CommandType getType() const override {
        return CommandType::MakeCommand;
    }

    nlohmann::json toJson() const override {
        return {
            {"type", static_cast<int>(getType())},
            {"id", id}
        };
    }

    void execute(std::unordered_map<std::string, std::shared_ptr<RDLayer>>& idToRDLayerMap) override;
};

// SetScrollRectCommand 的定义
struct SetScrollRectCommand : Command {
    Rect rect;

    SetScrollRectCommand(const std::string& uniqueId, const Rect& r) : rect(r) {
        this->id = uniqueId;
    }

    CommandType getType() const override {
        return CommandType::SetScrollRectCommand;
    }

    nlohmann::json toJson() const override {
        return {
            {"type", static_cast<int>(getType())},
            {"id", id},
            {"rect", {
                {"x", rect.x()},
                {"y", rect.y()},
                {"width", rect.width()},
                {"height", rect.height()}
            }}
        };
    }

    void execute(std::unordered_map<std::string, std::shared_ptr<RDLayer>>& idToRDLayerMap) override;
};

// AddChildCommand 的定义
struct AddChildCommand : Command {
    std::string childId;

    AddChildCommand(const std::string& inParentId, const std::string& inChildId)
        : childId(inChildId) {
        this->id = inParentId;
    }

    CommandType getType() const override {
        return CommandType::AddChildCommand;
    }

    nlohmann::json toJson() const override {
        return {
            {"type", static_cast<int>(getType())},
            {"parentId", id},
            {"childId", childId}
        };
    }

    void execute(std::unordered_map<std::string, std::shared_ptr<RDLayer>>& idToRDLayerMap) override;
};

// SetNameCommand 的定义
struct SetNameCommand : Command {
    std::string name;

    SetNameCommand(const std::string& uniqueId, const std::string& n) : name(n) {
        this->id = uniqueId;
    }

    CommandType getType() const override {
        return CommandType::SetNameCommand;
    }

    nlohmann::json toJson() const override {
        return {
            {"type", static_cast<int>(getType())},
            {"id", id},
            {"name", name}
        };
    }

    void execute(std::unordered_map<std::string, std::shared_ptr<RDLayer>>& idToRDLayerMap) override;
};

// SetAlphaCommand 的定义
struct SetAlphaCommand : Command {
    float alpha;

    SetAlphaCommand(const std::string& uniqueId, float a) : alpha(a) {
        this->id = uniqueId;
    }

    CommandType getType() const override {
        return CommandType::SetAlphaCommand;
    }

    nlohmann::json toJson() const override {
        return {
            {"type", static_cast<int>(getType())},
            {"id", id},
            {"alpha", alpha}
        };
    }

    void execute(std::unordered_map<std::string, std::shared_ptr<RDLayer>>& idToRDLayerMap) override;
};

class RDLayer {

public:
    static std::shared_ptr<RDLayer> Replay(const std::string& jsonStr);
    static std::shared_ptr<RDLayer> Replay(const std::vector<std::unique_ptr<Command>>& commands);
    static std::shared_ptr<RDLayer> Make();
    static std::vector<std::unique_ptr<Command>> ExtractCommands();
    static std::string SerializeCommands();

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
