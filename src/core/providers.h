#pragma once

#include "config/app_config.h"
#include "core/chat_types.h"
#include "core/http_client.h"

#include <memory>
#include <string>

namespace jarvis {

class LlmProvider {
public:
    virtual ~LlmProvider() = default;
    virtual std::string id() const = 0;
    virtual ChatResponse chat(const ChatRequest& request) const = 0;
};

std::unique_ptr<LlmProvider> makeProvider(
    const std::string& provider_name,
    const ProviderConfig& config,
    const HttpClient& http_client);

}  // namespace jarvis
