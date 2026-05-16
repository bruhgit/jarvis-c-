#pragma once

#include <filesystem>
#include <map>
#include <string>

namespace jarvis {

struct ProviderConfig {
    std::string protocol = "openai";
    std::string api_key;
    std::string base_url;
    std::string model;
    int max_tokens = 1024;
    double temperature = 0.2;
    std::string site_url;
    std::string app_name = "JARVIS C++";
};

struct AppConfig {
    std::string default_provider = "openrouter";
    std::string voice = "Charon";
    std::string voice_mode = "text";
    std::string speech_locale = "tr-TR";
    bool auto_speak_replies = false;
    bool protect_local_secrets = false;
    std::string system_prompt_file = "prompts/system_prompt.txt";
    std::string components_root = "components";
    std::string python_source_root = ".ilham";
    std::string youtube_api_key;
    std::string youtube_channel_handle;
    std::map<std::string, ProviderConfig> providers;
};

std::filesystem::path defaultConfigPath(const std::filesystem::path& executable_path = {});
void writeExampleConfig(const std::filesystem::path& path);
void saveConfig(const std::filesystem::path& path, const AppConfig& config);
AppConfig loadConfig(const std::filesystem::path& path);

}  // namespace jarvis
