#pragma once

#include "config/app_config.h"
#include "core/http_client.h"
#include "platform/platform_services.h"
#include "tools/tool_registry.h"
#include "ui/asset_catalog.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace jarvis {

class LlmProvider;

struct ProviderOverview {
    std::string name;
    std::string protocol;
    std::string model;
    bool active = false;
    bool has_api_key = false;
};

struct RuntimeResult {
    bool success = false;
    std::string role = "system";
    std::string text;
    std::string provider;
    std::string model;
};

class Runtime {
public:
    explicit Runtime(std::filesystem::path config_path);
    ~Runtime();

    const AppConfig& config() const;
    const AssetCatalog& assets() const;
    std::string platformName() const;
    std::string activeProviderName() const;
    std::string helpText() const;
    std::vector<ProviderOverview> providerSummaries() const;
    std::vector<ToolDefinition> toolDefinitions() const;
    bool voiceInputSupported() const;
    std::string speakText(const std::string& text) const;
    std::string recognizeSpeech(int timeout_seconds = 8) const;
    std::string playUiSound(std::string_view cue) const;
    bool localSecretProtectionEnabled() const;
    void setLocalSecretProtection(bool enabled);

    void reloadConfig();
    void setActiveProvider(const std::string& provider_name);
    RuntimeResult processInput(const std::string& line);

private:
    void refreshProvider();
    std::string loadSystemPrompt() const;
    RuntimeResult handleToolJson(const std::string& input);
    RuntimeResult chat(const std::string& prompt);

    std::filesystem::path config_path_;
    AppConfig config_;
    HttpClient http_client_;
    PlatformServices platform_;
    ToolRegistry tools_;
    AssetCatalog assets_;
    std::unique_ptr<LlmProvider> provider_;
};

}  // namespace jarvis
