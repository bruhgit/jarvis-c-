#include "config/app_config.h"

#include "util/common.h"
#include "util/json.h"

#include <stdexcept>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#endif

namespace jarvis {

namespace {

std::filesystem::path locateProjectRoot(std::filesystem::path start) {
    std::error_code error;
    if (start.empty()) {
        start = std::filesystem::current_path(error);
    }
    if (error) {
        return {};
    }

    start = std::filesystem::absolute(start, error);
    if (error) {
        return {};
    }

    if (std::filesystem::is_regular_file(start, error)) {
        start = start.parent_path();
    }

    for (auto current = start; !current.empty(); current = current.parent_path()) {
        const bool hasComponents = std::filesystem::exists(current / "components");
        const bool hasPrompts = std::filesystem::exists(current / "prompts");
        const bool hasIlham = std::filesystem::exists(current / ".ilham");
        if ((hasComponents && hasPrompts) || (hasComponents && hasIlham)) {
            return current;
        }
        if (current == current.root_path()) {
            break;
        }
    }

    return {};
}

bool configHasAnyApiKey(const std::filesystem::path& path) {
    try {
        if (!std::filesystem::exists(path)) {
            return false;
        }
        const util::JsonValue root = util::JsonValue::parse(util::readTextFile(path));
        const auto* providers = root.find("providers");
        if (providers == nullptr || !providers->isObject()) {
            return false;
        }
        for (const auto& [name, value] : providers->asObject()) {
            if (!value.isObject()) {
                continue;
            }
            const auto* key = value.find("api_key");
            if (key != nullptr && key->isString() && !util::trim(key->asString()).empty()) {
                return true;
            }
        }
    } catch (...) {
        return false;
    }
    return false;
}

std::string resolveProjectRelative(const std::string& raw, const std::filesystem::path& config_path) {
    if (raw.empty()) {
        return raw;
    }

    std::filesystem::path candidate(raw);
    if (candidate.is_absolute()) {
        return candidate.lexically_normal().string();
    }

    const std::filesystem::path project_root = locateProjectRoot(config_path);
    if (!project_root.empty()) {
        return (project_root / candidate).lexically_normal().string();
    }

    if (config_path.has_parent_path()) {
        return (config_path.parent_path() / candidate).lexically_normal().string();
    }

    return candidate.lexically_normal().string();
}

#ifdef _WIN32
std::string toBase64(const BYTE* bytes, DWORD size) {
    DWORD required = 0;
    if (!CryptBinaryToStringA(bytes, size, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &required)) {
        throw std::runtime_error("Base64 encode hatasi.");
    }
    std::string out(required, '\0');
    if (!CryptBinaryToStringA(bytes, size, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, out.data(), &required)) {
        throw std::runtime_error("Base64 encode hatasi.");
    }
    if (!out.empty() && out.back() == '\0') {
        out.pop_back();
    }
    return out;
}

std::string fromBase64(std::string_view encoded) {
    DWORD required = 0;
    if (!CryptStringToBinaryA(encoded.data(), static_cast<DWORD>(encoded.size()), CRYPT_STRING_BASE64, nullptr, &required, nullptr, nullptr)) {
        throw std::runtime_error("Base64 decode hatasi.");
    }
    std::string out(required, '\0');
    if (!CryptStringToBinaryA(encoded.data(), static_cast<DWORD>(encoded.size()), CRYPT_STRING_BASE64,
                              reinterpret_cast<BYTE*>(out.data()), &required, nullptr, nullptr)) {
        throw std::runtime_error("Base64 decode hatasi.");
    }
    return out;
}

std::string encryptSecret(const std::string& plain) {
    if (plain.empty()) {
        return plain;
    }

    DATA_BLOB input{};
    input.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plain.data()));
    input.cbData = static_cast<DWORD>(plain.size());

    DATA_BLOB output{};
    if (!CryptProtectData(&input, L"jarvis-secret", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        throw std::runtime_error("DPAPI sifreleme hatasi.");
    }

    const std::string encoded = toBase64(output.pbData, output.cbData);
    LocalFree(output.pbData);
    return "dpapi:" + encoded;
}

std::string decryptSecret(std::string_view value) {
    constexpr std::string_view prefix = "dpapi:";
    if (!value.starts_with(prefix)) {
        return std::string(value);
    }

    const std::string bytes = fromBase64(value.substr(prefix.size()));
    DATA_BLOB input{};
    input.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(bytes.data()));
    input.cbData = static_cast<DWORD>(bytes.size());

    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        throw std::runtime_error("DPAPI sifre cozulmedi.");
    }

    std::string plain(reinterpret_cast<char*>(output.pbData), output.cbData);
    LocalFree(output.pbData);
    return plain;
}
#else
std::string encryptSecret(const std::string& plain) {
    return plain;
}

std::string decryptSecret(std::string_view value) {
    return std::string(value);
}
#endif

ProviderConfig defaultOpenRouter() {
    ProviderConfig cfg;
    cfg.protocol = "openai";
    cfg.base_url = "https://openrouter.ai/api/v1";
    cfg.model = "openai/gpt-4o-mini";
    cfg.site_url = "https://example.com";
    cfg.app_name = "JARVIS C++";
    return cfg;
}

ProviderConfig defaultOpenAi() {
    ProviderConfig cfg;
    cfg.protocol = "openai";
    cfg.base_url = "https://api.openai.com/v1";
    cfg.model = "gpt-4.1-mini";
    return cfg;
}

ProviderConfig defaultGemini() {
    ProviderConfig cfg;
    cfg.protocol = "openai";
    cfg.base_url = "https://generativelanguage.googleapis.com/v1beta/openai";
    cfg.model = "gemini-3-flash-preview";
    return cfg;
}

ProviderConfig defaultClaude() {
    ProviderConfig cfg;
    cfg.protocol = "anthropic";
    cfg.base_url = "https://api.anthropic.com";
    cfg.model = "claude-sonnet-4-20250514";
    return cfg;
}

ProviderConfig defaultDeepSeek() {
    ProviderConfig cfg;
    cfg.protocol = "openai";
    cfg.base_url = "https://api.deepseek.com";
    cfg.model = "deepseek-v4-flash";
    return cfg;
}

ProviderConfig defaultQwen() {
    ProviderConfig cfg;
    cfg.protocol = "openai";
    cfg.base_url = "https://dashscope-intl.aliyuncs.com/compatible-mode/v1";
    cfg.model = "qwen3.5-flash";
    return cfg;
}

AppConfig makeDefaultConfig() {
    AppConfig config;
    config.providers["openrouter"] = defaultOpenRouter();
    config.providers["openai"] = defaultOpenAi();
    config.providers["gemini"] = defaultGemini();
    config.providers["claude"] = defaultClaude();
    config.providers["deepseek"] = defaultDeepSeek();
    config.providers["qwen"] = defaultQwen();
    return config;
}

std::string getString(const util::JsonValue::Object& object, std::string_view key, const std::string& fallback) {
    const auto it = object.find(std::string(key));
    if (it == object.end() || !it->second.isString()) {
        return fallback;
    }
    return it->second.asString();
}

int getInt(const util::JsonValue::Object& object, std::string_view key, int fallback) {
    const auto it = object.find(std::string(key));
    if (it == object.end() || !it->second.isNumber()) {
        return fallback;
    }
    return static_cast<int>(it->second.asNumber());
}

double getDouble(const util::JsonValue::Object& object, std::string_view key, double fallback) {
    const auto it = object.find(std::string(key));
    if (it == object.end() || !it->second.isNumber()) {
        return fallback;
    }
    return it->second.asNumber();
}

bool getBool(const util::JsonValue::Object& object, std::string_view key, bool fallback) {
    const auto it = object.find(std::string(key));
    if (it == object.end() || !it->second.isBool()) {
        return fallback;
    }
    return it->second.asBool();
}

util::JsonValue::Object providerToJson(const ProviderConfig& config) {
    return {
        {"protocol", config.protocol},
        {"api_key", config.api_key},
        {"base_url", config.base_url},
        {"model", config.model},
        {"max_tokens", config.max_tokens},
        {"temperature", config.temperature},
        {"site_url", config.site_url},
        {"app_name", config.app_name},
    };
}

util::JsonValue appConfigToJson(const AppConfig& config) {
    util::JsonValue::Object providers;
    for (const auto& [name, provider] : config.providers) {
        providers.emplace(name, providerToJson(provider));
    }

    return util::JsonValue::Object{
        {"default_provider", config.default_provider},
        {"voice", config.voice},
        {"voice_mode", config.voice_mode},
        {"speech_locale", config.speech_locale},
        {"auto_speak_replies", config.auto_speak_replies},
        {"protect_local_secrets", config.protect_local_secrets},
        {"system_prompt_file", config.system_prompt_file},
        {"components_root", config.components_root},
        {"python_source_root", config.python_source_root},
        {"youtube_api_key", config.youtube_api_key},
        {"youtube_channel_handle", config.youtube_channel_handle},
        {"providers", providers},
    };
}

ProviderConfig loadProviderConfig(const util::JsonValue::Object& object, const ProviderConfig& fallback) {
    ProviderConfig config = fallback;
    config.protocol = getString(object, "protocol", config.protocol);
    config.api_key = decryptSecret(getString(object, "api_key", config.api_key));
    config.base_url = getString(object, "base_url", config.base_url);
    config.model = getString(object, "model", config.model);
    config.max_tokens = getInt(object, "max_tokens", config.max_tokens);
    config.temperature = getDouble(object, "temperature", config.temperature);
    config.site_url = getString(object, "site_url", config.site_url);
    config.app_name = getString(object, "app_name", config.app_name);
    return config;
}

}  // namespace

std::filesystem::path defaultConfigPath(const std::filesystem::path& executable_path) {
    std::filesystem::path executable_candidate;
    if (!executable_path.empty()) {
        executable_candidate = std::filesystem::absolute(executable_path).parent_path() / "config" / "api_keys.json";
    }

    const std::filesystem::path project_root = locateProjectRoot(std::filesystem::current_path());
    const std::filesystem::path root_candidate =
        !project_root.empty()
            ? project_root / "config" / "api_keys.json"
            : std::filesystem::path("config") / "api_keys.json";

    if (!executable_candidate.empty() && configHasAnyApiKey(executable_candidate)) {
        return executable_candidate;
    }
    if (configHasAnyApiKey(root_candidate)) {
        return root_candidate;
    }
    if (!executable_candidate.empty() && std::filesystem::exists(executable_candidate)) {
        return executable_candidate;
    }
    if (std::filesystem::exists(root_candidate)) {
        return root_candidate;
    }
    return !executable_candidate.empty() ? executable_candidate : root_candidate;
}

void writeExampleConfig(const std::filesystem::path& path) {
    saveConfig(path, makeDefaultConfig());
}

void saveConfig(const std::filesystem::path& path, const AppConfig& source_config) {
    AppConfig config = source_config;
    util::JsonValue::Object providers;
    for (const auto& [name, provider] : config.providers) {
        ProviderConfig serializable = provider;
        if (config.protect_local_secrets) {
            serializable.api_key = encryptSecret(serializable.api_key);
        }
        providers.emplace(name, providerToJson(serializable));
    }

    util::JsonValue::Object root{
        {"default_provider", config.default_provider},
        {"voice", config.voice},
        {"voice_mode", config.voice_mode},
        {"speech_locale", config.speech_locale},
        {"auto_speak_replies", config.auto_speak_replies},
        {"protect_local_secrets", config.protect_local_secrets},
        {"system_prompt_file", config.system_prompt_file},
        {"components_root", config.components_root},
        {"python_source_root", config.python_source_root},
        {"youtube_api_key", config.protect_local_secrets ? encryptSecret(config.youtube_api_key) : config.youtube_api_key},
        {"youtube_channel_handle", config.youtube_channel_handle},
        {"providers", providers},
    };

    util::writeTextFile(path, util::JsonValue(root).dump(2));
}

AppConfig loadConfig(const std::filesystem::path& path) {
    AppConfig config = makeDefaultConfig();

    if (!std::filesystem::exists(path)) {
        writeExampleConfig(path);
        return config;
    }

    const util::JsonValue root = util::JsonValue::parse(util::readTextFile(path));
    if (!root.isObject()) {
        throw std::runtime_error("Config JSON object olmali.");
    }

    const auto& object = root.asObject();
    config.default_provider = getString(object, "default_provider", config.default_provider);
    config.voice = getString(object, "voice", config.voice);
    config.voice_mode = getString(object, "voice_mode", config.voice_mode);
    if (util::toLower(config.voice_mode) == "e2e") {
        config.voice_mode = "hands-free";
    }
    config.speech_locale = getString(object, "speech_locale", config.speech_locale);
    config.auto_speak_replies = getBool(object, "auto_speak_replies", config.auto_speak_replies);
    config.protect_local_secrets = getBool(object, "protect_local_secrets", config.protect_local_secrets);
    config.system_prompt_file = getString(object, "system_prompt_file", config.system_prompt_file);
    config.components_root = getString(object, "components_root", config.components_root);
    config.python_source_root = getString(object, "python_source_root", config.python_source_root);
    config.youtube_api_key = decryptSecret(getString(object, "youtube_api_key", config.youtube_api_key));
    config.youtube_channel_handle = getString(object, "youtube_channel_handle", config.youtube_channel_handle);

    const auto providersIt = object.find("providers");
    if (providersIt != object.end() && providersIt->second.isObject()) {
        for (const auto& [name, providerValue] : providersIt->second.asObject()) {
            if (!providerValue.isObject()) {
                continue;
            }

            const auto fallbackIt = config.providers.find(name);
            const ProviderConfig fallback = fallbackIt == config.providers.end() ? ProviderConfig{} : fallbackIt->second;
            config.providers[name] = loadProviderConfig(providerValue.asObject(), fallback);
        }
    }

    config.system_prompt_file = resolveProjectRelative(config.system_prompt_file, path);
    config.components_root = resolveProjectRelative(config.components_root, path);
    config.python_source_root = resolveProjectRelative(config.python_source_root, path);

    return config;
}

}  // namespace jarvis
