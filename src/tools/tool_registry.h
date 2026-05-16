#pragma once

#include "core/chat_types.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace jarvis {

class PlatformServices;

struct ToolResult {
    bool success = false;
    std::string output;
};

struct Tool {
    std::string name;
    std::string description;
    util::JsonValue parameters;
    std::function<ToolResult(const util::JsonValue::Object&)> execute;
};

class ToolRegistry {
public:
    static ToolRegistry createDefault(const PlatformServices& platform_services);

    std::vector<ToolDefinition> definitions() const;
    std::vector<std::string> names() const;
    ToolResult execute(const std::string& name, const util::JsonValue::Object& arguments) const;

private:
    std::map<std::string, Tool> tools_;
};

}  // namespace jarvis
