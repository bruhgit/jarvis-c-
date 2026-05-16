#include "tools/tool_registry.h"

#include "platform/platform_services.h"
#include "util/common.h"

namespace jarvis {

namespace {

std::string readStringArg(const util::JsonValue::Object& arguments, std::string_view key, std::string fallback = {}) {
    const auto it = arguments.find(std::string(key));
    if (it == arguments.end() || !it->second.isString()) {
        return fallback;
    }
    return it->second.asString();
}

}  // namespace

ToolRegistry ToolRegistry::createDefault(const PlatformServices& platform_services) {
    ToolRegistry registry;

    registry.tools_.emplace("sys_info", Tool{
        "sys_info",
        "Cross-platform sistem ozeti verir.",
        util::JsonValue::Object{
            {"type", "object"},
            {"properties", util::JsonValue::Object{
                {"query", util::JsonValue::Object{
                    {"type", "string"},
                    {"description", "battery|cpu|ram|disk|time|date|network|all"},
                }},
            }},
            {"required", util::JsonValue::Array{util::JsonValue("query")}},
        },
        [&platform_services](const util::JsonValue::Object& arguments) {
            return ToolResult{true, platform_services.systemInfo(readStringArg(arguments, "query", "all"))};
        },
    });

    registry.tools_.emplace("shell_run", Tool{
        "shell_run",
        "Guvenli shell komutu calistirir.",
        util::JsonValue::Object{
            {"type", "object"},
            {"properties", util::JsonValue::Object{
                {"command", util::JsonValue::Object{
                    {"type", "string"},
                    {"description", "Calistirilacak komut"},
                }},
            }},
            {"required", util::JsonValue::Array{util::JsonValue("command")}},
        },
        [&platform_services](const util::JsonValue::Object& arguments) {
            return ToolResult{true, platform_services.runShellCommand(readStringArg(arguments, "command"))};
        },
    });

    registry.tools_.emplace("browser_control", Tool{
        "browser_control",
        "URL acar veya arama yapar.",
        util::JsonValue::Object{
            {"type", "object"},
            {"properties", util::JsonValue::Object{
                {"action", util::JsonValue::Object{{"type", "string"}}},
                {"url", util::JsonValue::Object{{"type", "string"}}},
                {"query", util::JsonValue::Object{{"type", "string"}}},
            }},
            {"required", util::JsonValue::Array{util::JsonValue("action")}},
        },
        [&platform_services](const util::JsonValue::Object& arguments) {
            return ToolResult{
                true,
                platform_services.browserControl(
                    readStringArg(arguments, "action"),
                    readStringArg(arguments, "url"),
                    readStringArg(arguments, "query"))
            };
        },
    });

    registry.tools_.emplace("speak_text", Tool{
        "speak_text",
        "Metni sistem TTS ile okur.",
        util::JsonValue::Object{
            {"type", "object"},
            {"properties", util::JsonValue::Object{
                {"text", util::JsonValue::Object{
                    {"type", "string"},
                    {"description", "Okunacak metin"},
                }},
            }},
            {"required", util::JsonValue::Array{util::JsonValue("text")}},
        },
        [&platform_services](const util::JsonValue::Object& arguments) {
            return ToolResult{true, platform_services.speakText(readStringArg(arguments, "text"))};
        },
    });

    registry.tools_.emplace("open_app", Tool{
        "open_app",
        "Hedef uygulama veya URL acar.",
        util::JsonValue::Object{
            {"type", "object"},
            {"properties", util::JsonValue::Object{
                {"app_name", util::JsonValue::Object{
                    {"type", "string"},
                    {"description", "Uygulama adi veya acilacak hedef"},
                }},
            }},
            {"required", util::JsonValue::Array{util::JsonValue("app_name")}},
        },
        [&platform_services](const util::JsonValue::Object& arguments) {
            return ToolResult{true, platform_services.openTarget(readStringArg(arguments, "app_name"))};
        },
    });

    return registry;
}

std::vector<ToolDefinition> ToolRegistry::definitions() const {
    std::vector<ToolDefinition> items;
    for (const auto& [name, tool] : tools_) {
        items.push_back(ToolDefinition{name, tool.description, tool.parameters});
    }
    return items;
}

std::vector<std::string> ToolRegistry::names() const {
    std::vector<std::string> items;
    for (const auto& [name, tool] : tools_) {
        items.push_back(name);
    }
    return items;
}

ToolResult ToolRegistry::execute(const std::string& name, const util::JsonValue::Object& arguments) const {
    const auto it = tools_.find(name);
    if (it == tools_.end()) {
        return ToolResult{false, "Arac bulunamadi: " + name};
    }
    return it->second.execute(arguments);
}

}  // namespace jarvis
