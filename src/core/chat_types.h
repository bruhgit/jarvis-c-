#pragma once

#include "util/json.h"

#include <string>
#include <vector>

namespace jarvis {

struct Message {
    std::string role;
    std::string content;
};

struct ToolDefinition {
    std::string name;
    std::string description;
    util::JsonValue parameters;
};

struct ToolCall {
    std::string id;
    std::string name;
    util::JsonValue arguments;
};

struct ChatRequest {
    std::vector<Message> messages;
    std::vector<ToolDefinition> tools;
    std::string system_prompt;
    int max_tokens = 1024;
    double temperature = 0.2;
    std::string reasoning_effort;
};

struct ChatResponse {
    std::string provider;
    std::string model;
    std::string text;
    std::vector<ToolCall> tool_calls;
    std::string raw_json;
};

}  // namespace jarvis
