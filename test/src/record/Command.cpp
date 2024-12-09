#include <nlohmann/json.hpp>
#include <stdexcept>
#include "record/Command.h"
#include "record/RDLayer.h"

using json = nlohmann::json;

namespace tgfx {
// 实现 Command::fromJson 方法
std::unique_ptr<Command> Command::fromJson(const json& j) {
  CommandType type = static_cast<CommandType>(j.at("type").get<int>());
  switch (type) {
    case CommandType::SetScrollRectCommand: {
      Rect rect = Rect::MakeXYWH(
          j.at("rect").at("x").get<float>(), j.at("rect").at("y").get<float>(),
          j.at("rect").at("width").get<float>(), j.at("rect").at("height").get<float>());
      return std::make_unique<SetScrollRectCommand>(rect); // 移除 id 参数
    }
    case CommandType::SetNameCommand:
      return std::make_unique<SetNameCommand>(j.at("name").get<std::string>()); // 移除 id 参数
    case CommandType::SetAlphaCommand:
      return std::make_unique<SetAlphaCommand>(j.at("alpha").get<float>()); // 移除 id 参数
    default:
      throw std::runtime_error("未知的命令类型");
  }
}

nlohmann::json SetScrollRectCommand::toJson() const {
  return {{"type", static_cast<int>(getType())},
          {"rect",
           {{"x", rect.x()}, {"y", rect.y()}, {"width", rect.width()}, {"height", rect.height()}}}};
}

// SetScrollRectCommand 的实现
void SetScrollRectCommand::execute(RDLayer* rdLayer) {
  rdLayer->setScrollRect(rect);
}

nlohmann::json SetNameCommand::toJson() const {
  return {{"type", static_cast<int>(getType())}, {"name", name}};
}


// SetNameCommand 的实现
void SetNameCommand::execute(RDLayer* rdLayer) {
  rdLayer->setName(name);
}

nlohmann::json SetAlphaCommand::toJson() const {
  return {{"type", static_cast<int>(getType())}, {"alpha", alpha}};
}


// SetAlphaCommand 的实现
void SetAlphaCommand::execute(RDLayer* rdLayer) {
  rdLayer->setAlpha(alpha);
}

}