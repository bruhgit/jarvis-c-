#include "core/providers.h"

#include "util/common.h"
#include "util/json.h"

#include <sstream>
#include <stdexcept>

namespace jarvis {

namespace {

std::string normalizeOpenAiEndpoint(std::string base_url) {
    while (!base_url.empty() && base_url.back() == '/') {
        base_url.pop_back();
    }
    if (base_url.size() >= 17 && base_url.substr(base_url.size() - 17) == "/chat/completions") {
        return base_url;
    }
    return base_url + "/chat/completions";
}

std::string normalizeAnthropicEndpoint(std::string base_url) {
    while (!base_url.empty() && base_url.back() == '/') {
        base_url.pop_back();
    }
    if (base_url.size() >= 12 && base_url.substr(base_url.size() - 12) == "/v1/messages") {
        return base_url;
    }
    return base_url + "/v1/messages";
}

std::string joinOpenAiContent(const util::JsonValue& content) {
    if (content.isString()) {
        return content.asString();
    }
    if (!content.isArray()) {
        return {};
    }

    std::vector<std::string> parts;
    for (const auto& item : content.asArray()) {
        if (!item.isObject()) {
            continue;
        }
        const auto* text = item.find("text");
        if (text != nullptr && text->isString()) {
            parts.push_back(text->asString());
        }
    }
    return util::join(parts, "\n");
}

std::string readErrorMessage(const util::JsonValue& root) {
    if (!root.isObject()) {
        return {};
    }

    const auto* error = root.find("error");
    if (error == nullptr) {
        return {};
    }

    if (error->isString()) {
        return error->asString();
    }
    if (error->isObject()) {
        if (const auto* message = error->find("message"); message != nullptr && message->isString()) {
            return message->asString();
        }
        if (const auto* detail = error->find("type"); detail != nullptr && detail->isString()) {
            return detail->asString();
        }
    }
    return {};
}

util::JsonValue::Array buildOpenAiMessages(const ChatRequest& request) {
    util::JsonValue::Array messages;
    if (!request.system_prompt.empty()) {
        messages.emplace_back(util::JsonValue::Object{
            {"role", "system"},
            {"content", request.system_prompt},
        });
    }

    for (const auto& message : request.messages) {
        messages.emplace_back(util::JsonValue::Object{
            {"role", message.role},
            {"content", message.content},
        });
    }
    return messages;
}

util::JsonValue::Array buildOpenAiTools(const ChatRequest& request) {
    util::JsonValue::Array tools;
    for (const auto& tool : request.tools) {
        tools.emplace_back(util::JsonValue::Object{
            {"type", "function"},
            {"function", util::JsonValue::Object{
                {"name", tool.name},
                {"description", tool.description},
                {"parameters", tool.parameters},
            }},
        });
    }
    return tools;
}

util::JsonValue::Array buildAnthropicMessages(const ChatRequest& request) {
    util::JsonValue::Array messages;
    for (const auto& message : request.messages) {
        messages.emplace_back(util::JsonValue::Object{
            {"role", message.role},
            {"content", message.content},
        });
    }
    return messages;
}

util::JsonValue::Array buildAnthropicTools(const ChatRequest& request) {
    util::JsonValue::Array tools;
    for (const auto& tool : request.tools) {
        tools.emplace_back(util::JsonValue::Object{
            {"name", tool.name},
            {"description", tool.description},
            {"input_schema", tool.parameters},
        });
    }
    return tools;
}

class OpenAiCompatibleProvider final : public LlmProvider {
public:
    OpenAiCompatibleProvider(std::string provider_name, ProviderConfig config, const HttpClient& client)
        : provider_name_(std::move(provider_name)), config_(std::move(config)), client_(client) {}

    std::string id() const override {
        return provider_name_;
    }

    ChatResponse chat(const ChatRequest& request) const override {
        util::JsonValue::Object body{
            {"model", config_.model},
            {"messages", buildOpenAiMessages(request)},
            {"max_tokens", request.max_tokens > 0 ? request.max_tokens : config_.max_tokens},
            {"temperature", request.temperature},
        };

        if (!request.reasoning_effort.empty()) {
            body["reasoning_effort"] = request.reasoning_effort;
        }
        if (!request.tools.empty()) {
            body["tools"] = buildOpenAiTools(request);
            body["tool_choice"] = "auto";
        }

        HttpRequest http_request;
        http_request.url = normalizeOpenAiEndpoint(config_.base_url);
        http_request.headers["Content-Type"] = "application/json";
        http_request.headers["Authorization"] = "Bearer " + config_.api_key;
        if (!config_.site_url.empty()) {
            http_request.headers["HTTP-Referer"] = config_.site_url;
        }
        if (!config_.app_name.empty()) {
            http_request.headers["X-Title"] = config_.app_name;
        }
        http_request.body = util::JsonValue(body).dump();

        const HttpResponse http_response = client_.send(http_request);
        if (http_response.http_status < 200 || http_response.http_status >= 300) {
            std::string message = http_response.body;
            try {
                const util::JsonValue error_root = util::JsonValue::parse(http_response.body);
                const std::string provider_message = readErrorMessage(error_root);
                if (!provider_message.empty()) {
                    message = provider_message;
                }
            } catch (...) {
            }
            throw std::runtime_error("HTTP " + std::to_string(http_response.http_status) + ": " + message);
        }

        const util::JsonValue root = util::JsonValue::parse(http_response.body);
        const auto* choices = root.find("choices");
        if (choices == nullptr || !choices->isArray() || choices->asArray().empty()) {
            throw std::runtime_error("Gecerli choices alinamadi.");
        }

        const auto& firstChoice = choices->asArray().front();
        if (!firstChoice.isObject()) {
            throw std::runtime_error("choices[0] object degil.");
        }

        const auto* message = firstChoice.find("message");
        if (message == nullptr || !message->isObject()) {
            throw std::runtime_error("assistant mesaji eksik.");
        }

        ChatResponse response;
        response.provider = provider_name_;
        response.model = config_.model;
        response.raw_json = http_response.body;

        if (const auto* content = message->find("content"); content != nullptr) {
            response.text = joinOpenAiContent(*content);
        }

        if (const auto* toolCalls = message->find("tool_calls"); toolCalls != nullptr && toolCalls->isArray()) {
            for (const auto& item : toolCalls->asArray()) {
                if (!item.isObject()) {
                    continue;
                }

                ToolCall call;
                if (const auto* id = item.find("id"); id != nullptr && id->isString()) {
                    call.id = id->asString();
                }
                if (const auto* function = item.find("function"); function != nullptr && function->isObject()) {
                    if (const auto* name = function->find("name"); name != nullptr && name->isString()) {
                        call.name = name->asString();
                    }
                    if (const auto* arguments = function->find("arguments"); arguments != nullptr) {
                        if (arguments->isString()) {
                            try {
                                call.arguments = util::JsonValue::parse(arguments->asString());
                            } catch (...) {
                                call.arguments = util::JsonValue::Object{{"_raw", arguments->asString()}};
                            }
                        } else {
                            call.arguments = *arguments;
                        }
                    }
                }
                response.tool_calls.push_back(std::move(call));
            }
        }

        return response;
    }

private:
    std::string provider_name_;
    ProviderConfig config_;
    const HttpClient& client_;
};

class AnthropicProvider final : public LlmProvider {
public:
    AnthropicProvider(std::string provider_name, ProviderConfig config, const HttpClient& client)
        : provider_name_(std::move(provider_name)), config_(std::move(config)), client_(client) {}

    std::string id() const override {
        return provider_name_;
    }

    ChatResponse chat(const ChatRequest& request) const override {
        util::JsonValue::Object body{
            {"model", config_.model},
            {"max_tokens", request.max_tokens > 0 ? request.max_tokens : config_.max_tokens},
            {"temperature", request.temperature},
            {"messages", buildAnthropicMessages(request)},
        };

        if (!request.system_prompt.empty()) {
            body["system"] = request.system_prompt;
        }
        if (!request.tools.empty()) {
            body["tools"] = buildAnthropicTools(request);
        }

        HttpRequest http_request;
        http_request.url = normalizeAnthropicEndpoint(config_.base_url);
        http_request.headers["Content-Type"] = "application/json";
        http_request.headers["x-api-key"] = config_.api_key;
        http_request.headers["anthropic-version"] = "2023-06-01";
        http_request.body = util::JsonValue(body).dump();

        const HttpResponse http_response = client_.send(http_request);
        if (http_response.http_status < 200 || http_response.http_status >= 300) {
            std::string message = http_response.body;
            try {
                const util::JsonValue error_root = util::JsonValue::parse(http_response.body);
                const std::string provider_message = readErrorMessage(error_root);
                if (!provider_message.empty()) {
                    message = provider_message;
                }
            } catch (...) {
            }
            throw std::runtime_error("HTTP " + std::to_string(http_response.http_status) + ": " + message);
        }

        const util::JsonValue root = util::JsonValue::parse(http_response.body);
        const auto* content = root.find("content");
        if (content == nullptr || !content->isArray()) {
            throw std::runtime_error("Claude content blogu alinamadi.");
        }

        ChatResponse response;
        response.provider = provider_name_;
        response.model = config_.model;
        response.raw_json = http_response.body;

        std::vector<std::string> textParts;
        for (const auto& item : content->asArray()) {
            if (!item.isObject()) {
                continue;
            }
            const auto* type = item.find("type");
            if (type == nullptr || !type->isString()) {
                continue;
            }

            if (type->asString() == "text") {
                if (const auto* text = item.find("text"); text != nullptr && text->isString()) {
                    textParts.push_back(text->asString());
                }
                continue;
            }

            if (type->asString() == "tool_use") {
                ToolCall call;
                if (const auto* id = item.find("id"); id != nullptr && id->isString()) {
                    call.id = id->asString();
                }
                if (const auto* name = item.find("name"); name != nullptr && name->isString()) {
                    call.name = name->asString();
                }
                if (const auto* input = item.find("input"); input != nullptr) {
                    call.arguments = *input;
                }
                response.tool_calls.push_back(std::move(call));
            }
        }

        response.text = util::join(textParts, "\n");
        return response;
    }

private:
    std::string provider_name_;
    ProviderConfig config_;
    const HttpClient& client_;
};

}  // namespace

std::unique_ptr<LlmProvider> makeProvider(
    const std::string& provider_name,
    const ProviderConfig& config,
    const HttpClient& http_client) {
    const std::string protocol = util::toLower(config.protocol);
    if (protocol == "anthropic") {
        return std::make_unique<AnthropicProvider>(provider_name, config, http_client);
    }
    return std::make_unique<OpenAiCompatibleProvider>(provider_name, config, http_client);
}

}  // namespace jarvis
