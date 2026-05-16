#include "runtime/runtime.h"

#include "core/chat_types.h"
#include "core/providers.h"
#include "util/common.h"
#include "util/json.h"

#include <stdexcept>

namespace jarvis {

Runtime::Runtime(std::filesystem::path config_path)
    : config_path_(std::move(config_path)),
      config_(loadConfig(config_path_)),
      tools_(ToolRegistry::createDefault(platform_)),
      assets_(config_.components_root) {
    refreshProvider();
}

Runtime::~Runtime() = default;

const AppConfig& Runtime::config() const {
    return config_;
}

const AssetCatalog& Runtime::assets() const {
    return assets_;
}

std::string Runtime::platformName() const {
    return platform_.platformName();
}

std::string Runtime::activeProviderName() const {
    return config_.default_provider;
}

std::string Runtime::helpText() const {
    return
        "Komutlar:\n"
        "  /help                 Yardim\n"
        "  /providers            Provider listesini goster\n"
        "  /use <name>           Aktif provider degistir\n"
        "  /tools                Local araclari goster\n"
        "  /tool <name> <json>   Arac cagir. Ornek: /tool sys_info {\"query\":\"all\"}\n"
        "  /sys <query>          Hizli sistem ozeti\n"
        "  /shell <cmd>          Guvenli shell komutu\n"
        "  /open <target>        URL / uygulama ac\n"
        "  /search <query>       Tarayicida ara\n"
        "  /speak <text>         TTS dene\n"
        "  /reload               Config tekrar yukle\n"
        "  /quit                 Cikis";
}

std::vector<ProviderOverview> Runtime::providerSummaries() const {
    std::vector<ProviderOverview> items;
    for (const auto& [name, provider] : config_.providers) {
        items.push_back(ProviderOverview{
            name,
            provider.protocol,
            provider.model,
            name == config_.default_provider,
            !provider.api_key.empty(),
        });
    }
    return items;
}

std::vector<ToolDefinition> Runtime::toolDefinitions() const {
    return tools_.definitions();
}

bool Runtime::voiceInputSupported() const {
    return platform_.supportsSpeechRecognition();
}

std::string Runtime::speakText(const std::string& text) const {
    return platform_.speakText(text, config_.voice);
}

std::string Runtime::recognizeSpeech(int timeout_seconds) const {
    return platform_.recognizeSpeech(config_.speech_locale, timeout_seconds);
}

std::string Runtime::playUiSound(std::string_view cue) const {
    const std::filesystem::path sound = assets_.soundFileByStem(cue);
    if (sound.empty()) {
        return "Ses cue bulunamadi.";
    }
    return platform_.playAudioFile(sound);
}

bool Runtime::localSecretProtectionEnabled() const {
    return config_.protect_local_secrets;
}

void Runtime::setLocalSecretProtection(bool enabled) {
    config_.protect_local_secrets = enabled;
    saveConfig(config_path_, config_);
    config_ = loadConfig(config_path_);
    refreshProvider();
}

void Runtime::reloadConfig() {
    config_ = loadConfig(config_path_);
    assets_ = AssetCatalog(config_.components_root);
    refreshProvider();
}

void Runtime::setActiveProvider(const std::string& provider_name) {
    if (config_.providers.find(provider_name) == config_.providers.end()) {
        throw std::runtime_error("Provider bulunamadi: " + provider_name);
    }
    config_.default_provider = provider_name;
    refreshProvider();
}

RuntimeResult Runtime::processInput(const std::string& raw_line) {
    const std::string line = util::trim(raw_line);
    if (line.empty()) {
        return RuntimeResult{true, "system", ""};
    }

    if (line == "/help") {
        return RuntimeResult{true, "system", helpText()};
    }
    if (line == "/providers") {
        std::string text = "Provider listesi:\n";
        const auto providers = providerSummaries();
        for (std::size_t index = 0; index < providers.size(); ++index) {
            const auto& provider = providers[index];
            text += std::string(provider.active ? "* " : "- ")
                 + provider.name
                 + " | protocol=" + provider.protocol
                 + " | model=" + provider.model
                 + " | key=" + (provider.has_api_key ? "set" : "missing");
            if (index + 1 < providers.size()) {
                text += "\n";
            }
        }
        return RuntimeResult{true, "system", text};
    }
    if (line == "/tools") {
        std::string text = "Local araclar:\n";
        const auto definitions = toolDefinitions();
        for (std::size_t index = 0; index < definitions.size(); ++index) {
            const auto& tool = definitions[index];
            text += "- " + tool.name + " :: " + tool.description;
            if (index + 1 < definitions.size()) {
                text += "\n";
            }
        }
        return RuntimeResult{true, "system", text};
    }
    if (line == "/reload") {
        reloadConfig();
        return RuntimeResult{true, "system", "Config yenilendi. Aktif provider: " + config_.default_provider};
    }
    if (line.rfind("/use ", 0) == 0) {
        const std::string providerName = util::trim(line.substr(5));
        setActiveProvider(providerName);
        return RuntimeResult{true, "system", "Aktif provider: " + providerName, providerName, config_.providers.at(providerName).model};
    }
    if (line.rfind("/tool ", 0) == 0) {
        return handleToolJson(line.substr(6));
    }
    if (line.rfind("/sys ", 0) == 0) {
        return RuntimeResult{true, "system", platform_.systemInfo(line.substr(5))};
    }
    if (line.rfind("/shell ", 0) == 0) {
        return RuntimeResult{true, "system", platform_.runShellCommand(line.substr(7))};
    }
    if (line.rfind("/open ", 0) == 0) {
        return RuntimeResult{true, "system", platform_.openTarget(line.substr(6))};
    }
    if (line.rfind("/search ", 0) == 0) {
        return RuntimeResult{true, "system", platform_.browserControl("search", "", line.substr(8))};
    }
    if (line.rfind("/speak ", 0) == 0) {
        return RuntimeResult{true, "system", platform_.speakText(line.substr(7))};
    }

    return chat(line);
}

void Runtime::refreshProvider() {
    const auto it = config_.providers.find(config_.default_provider);
    if (it == config_.providers.end()) {
        throw std::runtime_error("Aktif provider config bulunamadi: " + config_.default_provider);
    }
    provider_ = makeProvider(config_.default_provider, it->second, http_client_);
}

std::string Runtime::loadSystemPrompt() const {
    try {
        return util::readTextFile(config_.system_prompt_file);
    } catch (...) {
        return
            "Sen JARVIS'sin.\n"
            "Varsayilan dilin Turkce.\n"
            "Kisa gundelik mesaja kisa ve dogal cevap ver.\n"
            "Analiz yapma, gereksiz maddelendirme yapma.\n"
            "Teknik sorularda net, dogru ve sonuc odakli ol.\n"
            "Bilmedigin noktayi uydurma.\n"
            "Bu surum C++ tabanli ve cross-platformdur.";
    }
}

RuntimeResult Runtime::handleToolJson(const std::string& input) {
    const auto firstSpace = input.find(' ');
    if (firstSpace == std::string::npos) {
        throw std::runtime_error("Format: /tool <name> <json>");
    }

    const std::string toolName = util::trim(input.substr(0, firstSpace));
    const std::string rawJson = util::trim(input.substr(firstSpace + 1));
    const util::JsonValue parsed = util::JsonValue::parse(rawJson);
    if (!parsed.isObject()) {
        throw std::runtime_error("Tool argumani JSON object olmali.");
    }

    const ToolResult result = tools_.execute(toolName, parsed.asObject());
    return RuntimeResult{result.success, result.success ? "system" : "error", result.output};
}

RuntimeResult Runtime::chat(const std::string& prompt) {
    const auto providerIt = config_.providers.find(config_.default_provider);
    if (providerIt == config_.providers.end()) {
        throw std::runtime_error("Aktif provider bulunamadi.");
    }
    if (providerIt->second.api_key.empty()) {
        throw std::runtime_error(
            "API key bos. config/api_keys.json dosyasindaki " + config_.default_provider + " alanini doldur.");
    }

    ChatRequest request;
    request.system_prompt = loadSystemPrompt();
    request.max_tokens = providerIt->second.max_tokens;
    request.temperature = providerIt->second.temperature;
    request.messages.push_back(Message{"user", prompt});

    const ChatResponse response = provider_->chat(request);
    RuntimeResult result;
    result.provider = response.provider;
    result.model = response.model;

    const std::string text = util::trim(response.text);
    if (text.empty() && response.tool_calls.empty()) {
        result.success = false;
        result.role = "error";
        result.text =
            "Provider bos bir yanit dondurdu. Model ve endpoint ayarini kontrol et. "
            "OpenRouter kullaniyorsan belirsiz veya zayif bir model yerine acik bir model slug'i sec.";
        return result;
    }

    result.success = true;
    result.role = "assistant";
    result.text = text;
    if (!response.tool_calls.empty()) {
        if (!result.text.empty()) {
            result.text += "\n\n";
        }
        result.text += "[tool_calls: " + std::to_string(response.tool_calls.size()) + "]";
    }
    return result;
}

}  // namespace jarvis
